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
        case SC_LFO:   return juce::Colour(0xFF3898EC); // Tempered blued steel
        case SC_RND:   return juce::Colour(0xFFE056FD); // Heat-tint violet
        case SC_ENV:   return juce::Colour(0xFFFF7700); // Blaze forge flame
        case SC_SEQ:   return juce::Colour(0xFF38B000); // Sulphur green flame
        case SC_MIDI:  return juce::Colour(0xFFFFB703); // Crucible molten gold
        default:       return juce::Colour(0xFFFFF0A0); // White-hot incandescent iron
    }
}

// Per-voice modulation destinations (not host parameters).
inline constexpr const char* kDestVoiceAmp   = "v_amp";
inline constexpr const char* kDestVoicePitch = "v_pitch";
inline constexpr const char* kDestVoicePan   = "v_pan";

enum PadSourceType
{
    SRC_SAMPLE = 0,
    SRC_KICK = 1,
    SRC_SNARE = 2,
    SRC_HAT_CLOSED = 3,
    SRC_HAT_OPEN = 4,
    SRC_CLAP = 5,
    SRC_TOM = 6,
    SRC_CRASH = 7,
    SRC_RIDE = 8,
    SRC_RIM = 9,
    SRC_BELL = 10,
    SRC_CONGA = 11,
    SRC_LUA = 12,
    SRC_COUNT
};

inline const char* padSourceTypeName(int srcType)
{
    switch (srcType)
    {
        case SRC_SAMPLE:     return "Sample Playback";
        case SRC_KICK:       return "Kick 808";
        case SRC_SNARE:      return "Snare 808";
        case SRC_HAT_CLOSED: return "Closed Hat";
        case SRC_HAT_OPEN:   return "Open Hat";
        case SRC_CLAP:       return "Handclap";
        case SRC_TOM:        return "Tom Drum";
        case SRC_CRASH:      return "Crash Cymbal";
        case SRC_RIDE:       return "Ride Cymbal";
        case SRC_RIM:        return "Rimshot";
        case SRC_BELL:       return "Cowbell / Bell";
        case SRC_CONGA:      return "Conga / Bongo";
        case SRC_LUA:        return "Lua DSP Script";
        default:             return "Sample Playback";
    }
}

inline const char* padCategoryName(int srcType)
{
    switch (srcType)
    {
        case SRC_SAMPLE:     return "SMPL";
        case SRC_KICK:       return "KICK";
        case SRC_SNARE:      return "SNAR";
        case SRC_HAT_CLOSED: return "CHAT";
        case SRC_HAT_OPEN:   return "OHAT";
        case SRC_CLAP:       return "CLAP";
        case SRC_TOM:        return "TOM";
        case SRC_CRASH:      return "CRSH";
        case SRC_RIDE:       return "RIDE";
        case SRC_RIM:        return "RIM";
        case SRC_BELL:       return "BELL";
        case SRC_CONGA:      return "CONG";
        case SRC_LUA:        return "LUA";
        default:             return "PAD";
    }
}

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
    { "fx4",   "FX P4" },     { "fx5",   "FX P5" },
    { "snda",  "Send A" },    { "sndb",  "Send B" },
    { "sndc",  "Send C" },    { "sndd",  "Send D" },
    { "chok",  "Choke" },     { "obus",  "Out Bus" },
    { "pmode", "Mode" },      { "mchan", "MIDI Ch" },
    { "mnote", "MIDI Note" }, { "src",   "Source" },
    { "satk",  "SMPL Atk" },  { "sdec",  "SMPL Dec" },
    { "ssus",  "SMPL Sus" },  { "srel",  "SMPL Rel" },
    { "ifx",   "Insert FX" }, { "ifx1",  "IFX P1" },
    { "ifx2",  "IFX P2" },    { "ifx3",  "IFX P3" },
    { "ifx4",  "IFX P4" },
    { "vcft",  "VCF Type" },  { "vcfc",  "VCF Cut" },
    { "vcfr",  "VCF Res" },   { "vcfe",  "VCF Env" }
};
constexpr int kNumPadParams = (int) (sizeof(kPadParams) / sizeof(kPadParams[0])); // 44

struct PadParams
{
    float level = 0.55f, pan = 0.f, tune = 0.f, decay = 1.f;
    float eqLF = 200.f, eqLG = 0.f, eqMF = 1000.f, eqMG = 0.f, eqHF = 8000.f, eqHG = 0.f;
    float cThr = 0.f, cRat = 1.f, cAtk = 5.f, cRel = 100.f;
    float drive = 0.f;
    int   fxType = 0;
    float fx1 = 0.5f, fx2 = 0.5f, fx3 = 0.5f, fx4 = 0.5f, fx5 = 0.5f;
    float sendA = 0.f, sendB = 0.f, sendC = 0.f, sendD = 0.f;
    int   choke = 0, outBus = 1, mode = 0, mchan = 0, mnote = 36, srcType = 0;
    float smplAtk = 0.001f, smplDec = 1.0f, smplSus = 1.0f, smplRel = 0.1f;
    int   ifxType = 0;
    float ifx1 = 0.5f, ifx2 = 0.5f, ifx3 = 0.5f, ifx4 = 0.5f;
    int   vcfType = 0; // 0: Bypass, 1: LP, 2: HP, 3: BP, 4: Notch
    float vcfCut = 20000.f;
    float vcfRes = 0.707f;
    float vcfEnv = 0.0f;
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
