#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace f64 {

// Realtime-safe parallel-for used by the audio thread to spread independent
// per-pad DSP work (Lua scripts + insert chains) across CPU cores.
//
// - No locks or allocations in run(): jobs are claimed with a single atomic
//   CAS on a (generation | index) word.
// - The calling (audio) thread participates in the work, so if workers are
//   asleep or descheduled the block still completes - worst case it simply
//   degrades to the old single-threaded behaviour.
// - Workers sleep on std::atomic::wait (WaitOnAddress / futex) between blocks.
class PadWorkerPool
{
public:
    using JobFn = void (*)(void* ctx, int index);

    PadWorkerPool() = default;
    ~PadWorkerPool() { stop(); }

    void start(int numWorkers)
    {
        stop();
        quit.store(false);
        for (int i = 0; i < numWorkers; ++i)
        {
            auto w = std::make_unique<Worker>(*this, i);
            w->startRealtimeThread(juce::Thread::RealtimeOptions{}.withPriority(9));
            workers.push_back(std::move(w));
        }
    }

    void stop()
    {
        quit.store(true);
        wake.fetch_add(1);
        wake.notify_all();
        for (auto& w : workers)
            w->stopThread(2000);
        workers.clear();
    }

    int numWorkers() const { return (int) workers.size(); }

    // Runs fn(ctx, i) for every i in [0, count). Returns when all are done.
    void run(int count, JobFn fn, void* ctx)
    {
        if (count <= 0)
            return;

        if (workers.empty() || count == 1)
        {
            for (int i = 0; i < count; ++i)
                fn(ctx, i);
            return;
        }

        jobFn.store(fn, std::memory_order_relaxed);
        jobCtx.store(ctx, std::memory_order_relaxed);
        jobCount.store(count, std::memory_order_relaxed);
        done.store(0, std::memory_order_relaxed);

        const uint64_t gen = (state.load(std::memory_order_relaxed) >> 32) + 1;
        state.store(gen << 32, std::memory_order_release);

        wake.fetch_add(1, std::memory_order_release);
        wake.notify_all();

        workOn(gen);

        // Only claimed jobs remain; whoever claimed them is actively running.
        while (done.load(std::memory_order_acquire) < count)
            juce::Thread::yield();
    }

private:
    // Claim and execute jobs belonging to generation `gen` until none remain.
    void workOn(uint64_t gen)
    {
        for (;;)
        {
            uint64_t s = state.load(std::memory_order_acquire);
            if ((s >> 32) != gen)
                return;
            const int count = jobCount.load(std::memory_order_relaxed);
            const int idx = (int) (s & 0xFFFFFFFFull);
            if (idx >= count)
                return;
            JobFn fn = jobFn.load(std::memory_order_relaxed);
            void* ctx = jobCtx.load(std::memory_order_relaxed);
            if (! state.compare_exchange_weak(s, s + 1, std::memory_order_acq_rel))
                continue;

            fn(ctx, idx);
            done.fetch_add(1, std::memory_order_release);
        }
    }

    struct Worker : juce::Thread
    {
        Worker(PadWorkerPool& p, int i) : juce::Thread("FORGE64 DSP " + juce::String(i + 1)), pool(p) {}

        void run() override
        {
            juce::ScopedNoDenormals noDenormals;
            uint32_t seen = pool.wake.load();
            while (! threadShouldExit() && ! pool.quit.load())
            {
                pool.wake.wait(seen);
                seen = pool.wake.load(std::memory_order_acquire);
                if (pool.quit.load())
                    break;
                pool.workOn(pool.state.load(std::memory_order_acquire) >> 32);
            }
        }

        PadWorkerPool& pool;
    };

    std::vector<std::unique_ptr<Worker>> workers;
    std::atomic<uint64_t> state { 0 };   // [generation:32 | next index:32]
    std::atomic<int> jobCount { 0 };
    std::atomic<int> done { 0 };
    std::atomic<JobFn> jobFn { nullptr };
    std::atomic<void*> jobCtx { nullptr };
    std::atomic<uint32_t> wake { 0 };
    std::atomic<bool> quit { false };
};

} // namespace f64
