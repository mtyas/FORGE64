#pragma once
#include <JuceHeader.h>
#include <map>
#include <memory>

namespace f64 {

// Decodes and caches audio samples. Loading happens on the message thread;
// the audio thread only ever touches shared_ptr copies stored in PadRuntime.
class SampleManager
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String name;
    };
    using Ptr = std::shared_ptr<const Sample>;

    SampleManager()
    {
        fmt.registerBasicFormats();
    }

    Ptr loadFile(const juce::File& f)
    {
        if (f == juce::File())
            return nullptr;

        const auto key = f.getFullPathName() + ":"
            + juce::String((juce::int64) f.getLastModificationTime().toMilliseconds());

        {
            const juce::ScopedLock sl(lock);
            auto it = cache.find(key);
            if (it != cache.end())
                return it->second;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
        if (! reader)
            return nullptr;

        auto s = std::make_shared<Sample>();
        s->sampleRate = reader->sampleRate;
        s->name = f.getFileNameWithoutExtension();
        const int numCh = juce::jmax(1, (int) reader->numChannels);
        const int numFrames = (int) reader->lengthInSamples;
        s->buffer.setSize(numCh, numFrames);
        if (numFrames > 0)
            reader->read(&s->buffer, 0, numFrames, 0, true, true);

        Ptr p = s;
        {
            const juce::ScopedLock sl(lock);
            cache[key] = p;
        }
        return p;
    }

    // Find an already-decoded sample by raw file path (any modification time).
    Ptr findCached(const juce::String& path)
    {
        if (path.isEmpty())
            return nullptr;
        const juce::ScopedLock sl(lock);
        for (auto& [k, v] : cache)
            if (k.startsWith(path + ":"))
                return v;
        return nullptr;
    }

private:
    juce::AudioFormatManager fmt;
    std::map<juce::String, Ptr> cache;
    juce::CriticalSection lock;
};

} // namespace f64
