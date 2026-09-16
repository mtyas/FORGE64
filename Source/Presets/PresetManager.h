#pragma once
#include <JuceHeader.h>
#include <functional>

namespace f64 {

// Preset file formats:
//   .kit - complete 64-pad setup: all params, pad states, modulation, scripts
//   .bnk - one bank (16 pads) incl. their parameters
//   .pad - a single pad incl. its parameters
// Files are XML; samples are referenced by absolute path (embedding = later phase).
class PresetManager
{
public:
    std::function<void()> onLoaded;

    PresetManager(juce::ValueTree& kitRoot, juce::AudioProcessorValueTreeState& apvtsIn);

    bool saveKit(const juce::File& f);
    bool loadKit(const juce::File& f);
    bool saveBank(const juce::File& f, int bank);
    bool loadBank(const juce::File& f, int bank);
    bool savePad(const juce::File& f, int pad);
    bool loadPad(const juce::File& f, int pad);

    const juce::String& lastError() const { return err; }

    static void copyTreeInPlace(juce::ValueTree dst, const juce::ValueTree& src);
    static int padIndexOfParamId(juce::StringRef id);

private:
    bool saveParamSubset(const juce::File& f, const char* tag, int firstPad, int padCount, int bankOrPad);
    bool loadParamSubset(const juce::File& f, const char* tag, int destFirstPad, int padCount);

    juce::ValueTree& kit;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String err;
};

} // namespace f64
