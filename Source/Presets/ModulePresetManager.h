#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include <vector>

namespace f64 {

// ===========================================================================
// Tier 1: Module DSP Engine (The Lua Algorithm & Parameter Definitions)
// ===========================================================================
struct ModuleInfo
{
    juce::String id;          // unique id, e.g. "kick_808"
    juce::String name;        // display name, e.g. "808 Bass Kick"
    juce::String category;    // "Kicks", "Snares", "Hi-Hats", etc.
    juce::String author = "Matthew Tyas / Forge64";
    juce::String description;
    juce::String p1Label = "P1";
    juce::String p2Label = "P2";
    juce::String p3Label = "P3";
    juce::String p4Label = "P4";
    juce::String p5Label = "P5";
    float defTune = 0.f;
    float defDecay = 0.5f;
    float defDrive = 0.f;
    float defP1 = 0.5f, defP2 = 0.5f, defP3 = 0.5f, defP4 = 0.5f, defP5 = 0.5f;
    int   defVcfType = 0;
    float defVcfCut = 20000.f;
    float defVcfRes = 0.707f;
    juce::String scriptCode;
};

// ===========================================================================
// Tier 2: Sound Preset (Specific Parameter Values for a Module)
// ===========================================================================
struct SoundPreset
{
    juce::String name;        // e.g. "Deep Sub 808"
    juce::String moduleId;    // matches ModuleInfo::id
    float tune = 0.f;
    float decay = 0.5f;
    float drive = 0.f;
    float p1 = 0.5f, p2 = 0.5f, p3 = 0.5f, p4 = 0.5f, p5 = 0.5f;
    int   vcfType = 0;
    float vcfCut = 20000.f;
    float vcfRes = 0.707f;
    float vcfEnv = 0.0f;

    SoundPreset() = default;

    SoundPreset(juce::String n, juce::String m, float tu, float dec, float drv,
                float _p1, float _p2, float _p3, float _p4,
                int vt = 0, float vc = 20000.f, float vr = 0.707f, float ve = 0.f,
                float _p5 = 0.5f)
        : name(std::move(n)), moduleId(std::move(m)), tune(tu), decay(dec), drive(drv),
          p1(_p1), p2(_p2), p3(_p3), p4(_p4), p5(_p5),
          vcfType(vt), vcfCut(vc), vcfRes(vr), vcfEnv(ve) {}

    SoundPreset(juce::String n, juce::String m, float tu, float dec, float drv,
                float _p1, float _p2, float _p3, float _p4, float _p5,
                int vt, float vc, float vr, float ve)
        : name(std::move(n)), moduleId(std::move(m)), tune(tu), decay(dec), drive(drv),
          p1(_p1), p2(_p2), p3(_p3), p4(_p4), p5(_p5),
          vcfType(vt), vcfCut(vc), vcfRes(vr), vcfEnv(ve) {}
};

class ModulePresetManager
{
public:
    static void initializeOnDisk();

    // Tier 1: Modules
    static const std::vector<ModuleInfo>& getFactoryModules();
    static std::vector<ModuleInfo> getAllModules();
    static ModuleInfo getModuleById(const juce::String& id);
    static juce::StringArray getModuleCategories();
    static std::vector<ModuleInfo> getModulesForCategory(const juce::String& cat);
    static bool saveUserModule(const ModuleInfo& mod);

    // Tier 2: Sound Presets (specific to a moduleId)
    static std::vector<SoundPreset> getSoundPresetsForModule(const juce::String& moduleId);
    static bool saveSoundPreset(const SoundPreset& preset);
    static bool deleteSoundPreset(const juce::String& moduleId, const juce::String& presetName);

    // Unified Category Sounds lookup
    struct CategorySoundEntry
    {
        juce::String displayName;
        juce::String moduleId;
        SoundPreset preset;
    };
    static std::vector<CategorySoundEntry> getSoundsForCategory(const juce::String& cat);

    // File directories
    static juce::File getModulesDir();
    static juce::File getSoundPresetsDir(const juce::String& moduleId);
};

} // namespace f64
