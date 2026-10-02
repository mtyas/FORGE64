#pragma once
#include <JuceHeader.h>
#include "../Engine/StepSequencer.h"

namespace f64 {

class SequencePresetManager
{
public:
    static juce::File getPresetsDirectory();
    static juce::StringArray getUserPresetNames();
    static bool savePreset(const juce::String& name, const StepSequencer& seq, int patternIndex);
    static bool loadPreset(const juce::String& name, StepSequencer& seq, int patternIndex);
    static bool loadPresetFromFile(const juce::File& file, StepSequencer& seq, int patternIndex);
    static bool exportPresetToFile(const juce::File& file, const StepSequencer& seq, int patternIndex);
    static void initializePresetsOnDisk();
};

} // namespace f64
