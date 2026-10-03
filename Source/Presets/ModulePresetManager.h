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
    float vcfDrive = 0.f;
    juce::String category;
    juce::String scriptCode;
    juce::String p1Label;
    juce::String p2Label;
    juce::String p3Label;
    juce::String p4Label;
    juce::String p5Label;
    bool  isUserPreset = false;

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

// ===========================================================================
// Dynamic Script Macro Knob Labels Parser
// ===========================================================================
struct ScriptMacroLabels
{
    juce::String p1, p2, p3, p4, p5;
    bool hasAny() const { return p1.isNotEmpty() || p2.isNotEmpty() || p3.isNotEmpty() || p4.isNotEmpty() || p5.isNotEmpty(); }
};

inline ScriptMacroLabels parseMacroLabelsFromScript(const juce::String& scriptCode)
{
    ScriptMacroLabels labels;
    juce::StringArray lines;
    lines.addLines(scriptCode);

    auto cleanLabel = [](juce::String s) -> juce::String
    {
        s = s.trim();
        while (s.startsWithChar(':') || s.startsWithChar('-') || s.startsWithChar('=') || s.startsWithChar(' '))
            s = s.substring(1).trim();
        if (s.containsChar('('))
            s = s.upToFirstOccurrenceOf("(", false, false).trim();
        if (s.containsChar(','))
            s = s.upToFirstOccurrenceOf(",", false, false).trim();
        if (s.startsWithIgnoreCase("Macro"))
        {
            int colon = s.indexOfChar(':');
            if (colon >= 0) s = s.substring(colon + 1).trim();
            else
            {
                int dash = s.indexOfChar('-');
                if (dash >= 0) s = s.substring(dash + 1).trim();
            }
        }
        s = s.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _+-/").trim();
        if (s.length() > 12)
        {
            auto words = juce::StringArray::fromTokens(s, " ", "");
            if (words.size() > 0)
            {
                s = words[0];
                if (words.size() > 1 && (s.length() + 1 + words[1].length() <= 12))
                    s += " " + words[1];
            }
            else
            {
                s = s.substring(0, 12).trim();
            }
        }
        return s.toUpperCase();
    };

    for (const auto& rawLine : lines)
    {
        auto line = rawLine.trim();

        // 1. Check for -- @labels: P1, P2, P3, P4, P5
        if (line.startsWithIgnoreCase("--") && line.containsIgnoreCase("@labels"))
        {
            auto after = line.fromFirstOccurrenceOf(":", false, false).trim();
            juce::StringArray parts;
            parts.addTokens(after, ",", "\"");
            if (parts.size() >= 1 && labels.p1.isEmpty()) labels.p1 = cleanLabel(parts[0]);
            if (parts.size() >= 2 && labels.p2.isEmpty()) labels.p2 = cleanLabel(parts[1]);
            if (parts.size() >= 3 && labels.p3.isEmpty()) labels.p3 = cleanLabel(parts[2]);
            if (parts.size() >= 4 && labels.p4.isEmpty()) labels.p4 = cleanLabel(parts[3]);
            if (parts.size() >= 5 && labels.p5.isEmpty()) labels.p5 = cleanLabel(parts[4]);
            continue;
        }

        // 2. Check for @pX, @macroX, pX:, macro X:
        for (int pIdx = 1; pIdx <= 5; ++pIdx)
        {
            juce::String& target = (pIdx == 1 ? labels.p1 :
                                   (pIdx == 2 ? labels.p2 :
                                   (pIdx == 3 ? labels.p3 :
                                   (pIdx == 4 ? labels.p4 : labels.p5))));
            if (target.isNotEmpty())
                continue;

            juce::String tag1 = "@p" + juce::String(pIdx);
            juce::String tag2 = "@macro" + juce::String(pIdx);
            juce::String tag3 = "-- p" + juce::String(pIdx) + ":";
            juce::String tag4 = "-- macro " + juce::String(pIdx) + ":";

            int pos = -1;
            int prefixLen = 0;
            if ((pos = line.indexOfIgnoreCase(tag1)) >= 0) { prefixLen = tag1.length(); }
            else if ((pos = line.indexOfIgnoreCase(tag2)) >= 0) { prefixLen = tag2.length(); }
            else if ((pos = line.indexOfIgnoreCase(tag3)) >= 0) { prefixLen = tag3.length(); }
            else if ((pos = line.indexOfIgnoreCase(tag4)) >= 0) { prefixLen = tag4.length(); }
            else
            {
                juce::String pStr = "\"p" + juce::String(pIdx) + "\"";
                juce::String fxStr = "\"fx" + juce::String(pIdx) + "\"";
                int pPos = line.indexOfIgnoreCase(pStr);
                if (pPos < 0) pPos = line.indexOfIgnoreCase(fxStr);
                if (pPos >= 0)
                {
                    int cpos = line.indexOfIgnoreCase("--");
                    if (cpos > pPos)
                    {
                        pos = cpos;
                        prefixLen = 2;
                    }
                }
            }

            if (pos >= 0)
            {
                juce::String rem = line.substring(pos + prefixLen);
                juce::String cl = cleanLabel(rem);
                if (cl.isNotEmpty())
                    target = cl;
            }
        }
    }

    return labels;
}

} // namespace f64
