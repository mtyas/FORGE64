#pragma once
#include <JuceHeader.h>
#include <array>
#include <utility>

namespace f64 {

// juce::jlimit with (value, lo, hi) argument order.
template <typename T>
inline T clampRange(T value, T minVal, T maxVal)
{
    return juce::jlimit(minVal, maxVal, value);
}

constexpr int kNumBanks     = 4;
constexpr int kPadsPerBank  = 16;
constexpr int kNumPads      = kNumBanks * kPadsPerBank;
constexpr int kNumBuses     = 16;
constexpr int kNumChokes    = 16;
constexpr int kMaxVoices    = 64;
constexpr int kEnvInstances = 16;

constexpr int kNumLFO = 10, kNumRnd = 10, kNumEnv = 10, kNumSeq = 10;
constexpr int kNumMidiSrc = 5, kNumMacros = 8;
constexpr int kNumSlots = kNumLFO + kNumRnd + kNumEnv + kNumSeq + kNumMidiSrc + kNumMacros; // 53

inline int slotLFO(int i)   { return i; }
inline int slotRnd(int i)   { return kNumLFO + i; }
inline int slotEnv(int i)   { return kNumLFO + kNumRnd + i; }
inline int slotSeq(int i)   { return kNumLFO + kNumRnd + kNumEnv + i; }
inline int slotMidi(int k)  { return kNumLFO + kNumRnd + kNumEnv + kNumSeq + k; }
inline int slotMacro(int i) { return kNumLFO + kNumRnd + kNumEnv + kNumSeq + kNumMidiSrc + i; }

enum SlotClass { SC_LFO = 0, SC_RND, SC_ENV, SC_SEQ, SC_MIDI, SC_MACRO };

inline int slotClassOf(int slot)
{
    if (slot < slotRnd(0))   return SC_LFO;
    if (slot < slotEnv(0))   return SC_RND;
    if (slot < slotSeq(0))   return SC_ENV;
    if (slot < slotMidi(0))  return SC_SEQ;
    if (slot < slotMacro(0)) return SC_MIDI;
    return SC_MACRO;
}

inline juce::String slotName(int slot)
{
    switch (slotClassOf(slot))
    {
        case SC_LFO: return "LFO " + juce::String(slot - slotLFO(0) + 1);
        case SC_RND: return "RND " + juce::String(slot - slotRnd(0) + 1);
        case SC_ENV: return "ENV " + juce::String(slot - slotEnv(0) + 1);
        case SC_SEQ: return "SEQ " + juce::String(slot - slotSeq(0) + 1);
        case SC_MIDI:
        {
            static const char* names[kNumMidiSrc] = { "Velocity", "ModWheel", "PitchBend", "ChanAT", "PolyAT" };
            return names[clampRange(slot - slotMidi(0), 0, kNumMidiSrc - 1)];
        }
        default: return "MACRO " + juce::String(slot - slotMacro(0) + 1);
    }
}

inline juce::String slotShortName(int slot)
{
    switch (slotClassOf(slot))
    {
        case SC_LFO: return "L" + juce::String(slot - slotLFO(0) + 1);
        case SC_RND: return "R" + juce::String(slot - slotRnd(0) + 1);
        case SC_ENV: return "E" + juce::String(slot - slotEnv(0) + 1);
        case SC_SEQ: return "S" + juce::String(slot - slotSeq(0) + 1);
        case SC_MIDI: return "MI";
        default: return "M" + juce::String(slot - slotMacro(0) + 1);
    }
}

inline juce::Colour slotColour(int slot)
{
    switch (slotClassOf(slot))
    {
        case SC_LFO:   return juce::Colour(0xFF35C4F0); // cyan
        case SC_RND:   return juce::Colour(0xFFE35BDB); // magenta
        case SC_ENV:   return juce::Colour(0xFFF59E42); // orange
        case SC_SEQ:   return juce::Colour(0xFF7BDC5A); // green
        case SC_MIDI:  return juce::Colour(0xFFF0DE3C); // yellow
        default:       return juce::Colour(0xFFE8E8E8); // white
    }
}

// Per-voice modulation destinations (not host parameters).
inline constexpr const char* kDestVoiceAmp   = "v_amp";
inline constexpr const char* kDestVoicePitch = "v_pitch";
inline constexpr const char* kDestVoicePan   = "v_pan";

struct PadParamDef { const char* base; const char* label; };

// Order here defines the index used by PluginProcessor::fillPadParams - keep in sync!
inline constexpr PadParamDef kPadParams[] = {
    { "lvl",   "Level" },     { "pan",   "Pan" },
    { "tune",  "Tune" },      { "dec",   "Decay" },
    { "eqlf",  "EQ Lo Hz" },  { "eqlg",  "EQ Lo dB" },
    { "eqmf",  "EQ Mid Hz" }, { "eqmg",  "EQ Mid dB" },
    { "eqhf",  "EQ Hi Hz" },  { "eqhg",  "EQ Hi dB" },
    { "cthr",  "Comp Thr" },  { "crat",  "Comp Ratio" },
    { "catk",  "Comp Atk" },  { "crel",  "Comp Rel" },
    { "drv",   "Drive" },
    { "fx",    "FX Type" },   { "fx1",   "FX P1" },
    { "fx2",   "FX P2" },     { "fx3",   "FX P3" },
    { "fx4",   "FX P4" },
    { "snda",  "Send A" },    { "sndb",  "Send B" },
    { "chok",  "Choke" },     { "obus",  "Out Bus" },
    { "pmode", "Mode" },      { "mchan", "MIDI Ch" },
    { "mnote", "MIDI Note" }, { "src",   "Source" },
};
constexpr int kNumPadParams = (int) (sizeof(kPadParams) / sizeof(kPadParams[0])); // 28

struct PadParams
{
    float level = 0.8f, pan = 0.f, tune = 0.f, decay = 1.f;
    float eqLF = 200.f, eqLG = 0.f, eqMF = 1000.f, eqMG = 0.f, eqHF = 8000.f, eqHG = 0.f;
    float cThr = 0.f, cRat = 1.f, cAtk = 5.f, cRel = 100.f;
    float drive = 0.f;
    int   fxType = 0;
    float fx1 = 0.5f, fx2 = 0.5f, fx3 = 0.5f, fx4 = 0.5f;
    float sendA = 0.f, sendB = 0.f;
    int   choke = 0, outBus = 1, mode = 0, mchan = 0, mnote = 36, srcType = 0;
};

inline juce::String padParamId(int pad, juce::StringRef base)
{
    return "p" + juce::String(pad) + "_" + base;
}

inline juce::String destDisplayName(const juce::String& id)
{
    if (id == kDestVoiceAmp)   return "VOICE Amp";
    if (id == kDestVoicePitch) return "VOICE Pitch";
    if (id == kDestVoicePan)   return "VOICE Pan";

    if (id.startsWithChar('p') && id.contains("_"))
    {
        const int pad = id.substring(1, id.indexOf("_")).getIntValue();
        if (pad >= 0 && pad < kNumPads)
        {
            const auto base = id.substring(id.indexOf("_") + 1);
            for (const auto& d : kPadParams)
                if (base == d.base)
                    return "PAD " + juce::String(pad + 1) + " " + d.label;
        }
    }

    static const std::pair<const char*, const char*> globals[] = {
        { "master",  "Master Level" }, { "revsize", "Reverb Size" },
        { "revdamp", "Reverb Damp" },  { "dlytime", "Delay Time" },
        { "dlyfb",   "Delay Feedback" },
    };
    for (const auto& g : globals)
        if (id == g.first)
            return g.second;

    if (id.length() == 2 && id.startsWithChar('m'))
        return "MACRO " + juce::String(id.substring(1).getIntValue() + 1);

    return id;
}

inline juce::Font uiFont(float size, bool bold = false)
{
    juce::Font f{ juce::FontOptions(size) };
    if (bold)
        f.setBold(true);
    return f;
}

} // namespace f64
