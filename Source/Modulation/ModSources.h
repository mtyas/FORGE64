#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include <array>
#include <atomic>

namespace f64 {

// Tempo-sync divisions (in beats), longest first.
extern const double kSyncDivBeats[];
extern const char*  kSyncDivNames[];
constexpr int kNumSyncDivs = 15;

// ---------------------------------------------------------------------------
// Base class for all modulation sources.
// Threading model: parameters live in atomics. The message thread writes them
// (via syncFromState / setters), the audio thread only reads them in render().
class ModSource
{
public:
    virtual ~ModSource() = default;

    virtual void prepare(double sr, int /*maxBlock*/) { sampleRate = sr; }
    virtual void syncFromState() {}
    virtual void render(float* out, int n) = 0;
    virtual void retrigger() {}
    virtual void setTempo(double bpm) { tempoBpm = bpm; }

    std::atomic<bool> enabled { false };
    juce::ValueTree   state;
    double            sampleRate = 48000.0;
    double            tempoBpm   = 120.0;
    juce::Random      rnd;

protected:
    double rateHz(float freeHz, bool sync, int div) const
    {
        if (sync)
        {
            const double beats = kSyncDivBeats[clampRange(div, 0, kNumSyncDivs - 1)];
            const double sec = beats * 60.0 / juce::jmax(20.0, tempoBpm);
            return sec > 0.0 ? 1.0 / sec : 1.0;
        }
        return juce::jmax(0.001, (double) freeHz);
    }
};

// ---------------------------------------------------------------------------
class LFOSource final : public ModSource
{
public:
    static juce::ValueTree makeDefault();
    void syncFromState() override;
    void render(float* out, int n) override;
    void retrigger() override { phase = 0.0; prevStep = -1; }

    std::atomic<int>   shape { 0 }; // 0 sin, 1 tri, 2 saw, 3 sqr, 4 S&H, 5 S&H glide
    std::atomic<float> rate { 2.0f };
    std::atomic<bool>  sync { false };
    std::atomic<int>   div { 8 };
    std::atomic<bool>  uni { false };
    std::atomic<float> glide { 0.f };
    std::atomic<float> phaseOff { 0.f };

private:
    double phase = 0.0;
    int    prevStep = -1;
    float  held = 0.f, glideMem = 0.f;
};

// ---------------------------------------------------------------------------
class RandomSource final : public ModSource
{
public:
    static juce::ValueTree makeDefault();
    void syncFromState() override;
    void render(float* out, int n) override;
    void retrigger() override { phase = 0.0; prevStep = -1; }

    std::atomic<int>   kind { 0 }; // 0 S&H, 1 smooth, 2 drunk, 3 prob-step, 4 Lorenz
    std::atomic<float> rate { 4.0f };
    std::atomic<bool>  sync { true };
    std::atomic<int>   div { 7 };
    std::atomic<float> p1 { 0.5f };
    std::atomic<bool>  uni { false };

private:
    double phase = 0.0;
    int    prevStep = -1;
    float  held = 0.f, prevHeld = 0.f;
    double lx = 0.1, ly = 0.0, lz = 0.0;
};

// ---------------------------------------------------------------------------
// DAHDSR envelope. One global instance (rendered into the source buffer) plus
// a pool of per-voice instances used when routed to v_* destinations.
class EnvSource final : public ModSource
{
public:
    static juce::ValueTree makeDefault();
    void syncFromState() override;
    void render(float* out, int n) override;
    void retrigger() override { globalInst.trigReq = true; }

    void  triggerInstance(int i) { inst[(size_t) i].trigReq = true; }
    void  renderInstance(int i, int n);
    float instanceValue(int i) const { return enabled.load() ? inst[(size_t) i].last : 0.f; }

    std::atomic<float> dly { 0.f }, atk { 0.01f }, hold { 0.f }, dec { 0.3f };
    std::atomic<float> sus { 0.5f }, rel { 0.1f }, curve { 0.f };
    std::atomic<bool>  loop { false };
    std::atomic<int>   triggerPad { -1 };  // -1 = all, 0..63 = specific pad
    std::atomic<int>   triggerNote { -1 }; // -1 = any, 0..127 = specific note

private:
    struct Instance
    {
        int   stage = -1;
        float t = 0.f, level = 0.f, last = 0.f;
        std::atomic<bool> trigReq { false };
    };
    void advance(Instance& in, float* out, int n);
    Instance globalInst;
    std::array<Instance, kEnvInstances> inst;
};

// ---------------------------------------------------------------------------
class SeqSource final : public ModSource
{
public:
    static juce::ValueTree makeDefault();
    void syncFromState() override;
    void render(float* out, int n) override;
    void retrigger() override { restartReq = true; }
    void setStep(int i, float v); // message thread

    std::atomic<int>   numSteps { 16 };
    std::atomic<float> rate { 4.f };
    std::atomic<bool>  sync { true };
    std::atomic<int>   div { 9 };
    std::atomic<float> gate { 0.8f }, slew { 0.1f }, swing { 0.f };
    std::atomic<int>   dir { 0 }; // 0 fwd, 1 rev, 2 pingpong, 3 random
    std::atomic<bool>  uni { true };
    std::array<std::atomic<float>, 32> steps;

private:
    void persistSteps(); // message thread: mirror atomics into state property
    std::atomic<bool> restartReq { false };
    double t = 0.0, stepStart = 0.0, stepLen = -1.0;
    int    curStep = 0, pingDir = 1;
    float  cur = 0.f, mem = 0.f;
};

// ---------------------------------------------------------------------------
class MidiSource final : public ModSource
{
public:
    explicit MidiSource(int kind_) : kind(kind_) {}
    void render(float* out, int n) override
    {
        const float v = value.load();
        for (int i = 0; i < n; ++i) out[i] = v;
    }
    void setValue(float v) { value = v; }
    const int kind;

private:
    std::atomic<float> value { 0.f };
};

class MacroSource final : public ModSource
{
public:
    explicit MacroSource(int index_) : index(index_) {}
    void render(float* out, int n) override
    {
        const float v = value.load();
        for (int i = 0; i < n; ++i) out[i] = v;
    }
    void setValue(float v) { value = v; }
    const int index;

private:
    std::atomic<float> value { 0.f };
};

} // namespace f64
