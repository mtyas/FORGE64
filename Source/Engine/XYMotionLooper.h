#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace f64 {
// A bounded 30 Hz movement recording; playback advances on the audio clock.
// Atomic points keep loading/recording safe without an audio-thread mutex.
class XYMotionLooper
{
public:
    static constexpr int capacity = 3600;
    struct Point { std::atomic<float> x { .5f }, y { .5f }; };
    std::atomic<int> xDestination { 1 }, yDestination { 2 };
    std::atomic<float> speed { 1.f }, x { .5f }, y { .5f };
    std::atomic<bool> recording { false }, playing { false };

    void start(float px, float py)
    {
        playing.store(false);
        count.store(0);
        rewind.store(true);
        recording.store(true);
        append(px, py);
    }
    void append(float px, float py)
    {
        if (!recording.load()) return;
        const int k = count.load();
        if (k >= capacity) { finish(); return; }
        points[(size_t)k].x.store(juce::jlimit(0.f, 1.f, px));
        points[(size_t)k].y.store(juce::jlimit(0.f, 1.f, py));
        count.store(k + 1);
    }
    void finish()
    {
        recording.store(false);
        rewind.store(true);
        playing.store(count.load() > 1);
    }
    void clear() { playing.store(false); recording.store(false); count.store(0); rewind.store(true); }
    int size() const { return count.load(); }
    bool process(int samples, double sampleRate)
    {
        if (rewind.exchange(false)) phase = 0.;
        const int length = count.load();
        if (!playing.load() || recording.load() || length < 2 || sampleRate <= 0.) return false;
        phase = std::fmod(phase, (double)length);
        const int a = (int)phase, b = (a + 1) % length;
        const float fraction = (float)(phase - a);
        x.store(points[(size_t)a].x.load() * (1.f - fraction) + points[(size_t)b].x.load() * fraction);
        y.store(points[(size_t)a].y.load() * (1.f - fraction) + points[(size_t)b].y.load() * fraction);
        phase = std::fmod(phase + samples / sampleRate * 30. * juce::jlimit(.125f, 8.f, speed.load()), (double)length);
        return true;
    }
    juce::ValueTree serialize() const
    {
        juce::ValueTree tree("XY_LOOP");
        tree.setProperty("xDest", xDestination.load(), nullptr);
        tree.setProperty("yDest", yDestination.load(), nullptr);
        tree.setProperty("speed", speed.load(), nullptr);
        tree.setProperty("playing", playing.load(), nullptr);
        for (int k = 0; k < count.load(); ++k)
        {
            juce::ValueTree point("POINT");
            point.setProperty("x", points[(size_t)k].x.load(), nullptr);
            point.setProperty("y", points[(size_t)k].y.load(), nullptr);
            tree.appendChild(point, nullptr);
        }
        return tree;
    }
    void deserialize(const juce::ValueTree& tree)
    {
        clear();
        xDestination.store((int)tree.getProperty("xDest", xDestination.load()));
        yDestination.store((int)tree.getProperty("yDest", yDestination.load()));
        speed.store(juce::jlimit(.125f, 8.f, (float)tree.getProperty("speed", 1.f)));
        const int length = juce::jmin(capacity, tree.getNumChildren());
        for (int k = 0; k < length; ++k)
        {
            auto point = tree.getChild(k);
            points[(size_t)k].x.store(juce::jlimit(0.f, 1.f, (float)point.getProperty("x", .5f)));
            points[(size_t)k].y.store(juce::jlimit(0.f, 1.f, (float)point.getProperty("y", .5f)));
        }
        count.store(length);
        playing.store(length > 1 && (bool)tree.getProperty("playing", false));
    }
private:
    std::array<Point, capacity> points;
    std::atomic<int> count { 0 };
    std::atomic<bool> rewind { true };
    double phase = 0.; // audio thread only
};
}
