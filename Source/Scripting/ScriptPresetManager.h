#pragma once
#include <JuceHeader.h>
#include <vector>

namespace f64 {

struct ScriptPreset
{
    juce::String name;
    juce::String category; // "Kicks", "Snares", "HiHats", "Cymbals", "Claps", "Toms", "Synths & Noise", "Custom"
    juce::String author;
    juce::String description;
    juce::String scriptCode;
    juce::String k1Label = "PARAM 1";
    juce::String k2Label = "PARAM 2";
    juce::String k3Label = "PARAM 3";
    juce::String k4Label = "PARAM 4";
    float defTune  = 0.0f;
    float defFx1   = 0.5f;
    float defFx2   = 0.5f;
    float defFx3   = 0.5f;
    float defFx4   = 0.5f;
    float defDecay = 1.0f;
    float defDrive = 0.0f;
    juce::File filePath;
};

class ScriptPresetManager
{
public:
    static juce::File getPresetsDirectory();
    static void initializePresetsOnDisk();

    static std::vector<ScriptPreset> getAllPresets();
    static std::vector<ScriptPreset> getPresetsForCategory(const juce::String& category);
    static juce::StringArray getCategories();

    static bool savePreset(const ScriptPreset& preset);
    static bool loadPresetFromFile(const juce::File& file, ScriptPreset& outPreset);
    static bool exportPresetToFile(const ScriptPreset& preset, const juce::File& targetFile);

    static std::vector<ScriptPreset> getFactoryPresets();
};

} // namespace f64
