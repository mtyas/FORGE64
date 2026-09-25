#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

namespace f64 {

enum AuxFxType
{
    AUX_FX_REVERB = 0,
    AUX_FX_DELAY = 1,
    AUX_FX_DRIVE = 2,
    AUX_FX_CHORUS = 3,
    AUX_FX_FLANGER = 4,
    AUX_FX_PHASER = 5,
    AUX_FX_COMP = 6,
    AUX_FX_FILTER = 7,
    AUX_FX_SHIMMER = 8,
    AUX_FX_PINGPONG = 9,
    AUX_FX_GATED_VERB = 10,
    AUX_FX_TUBE = 11,
    AUX_FX_PITCH = 12,
    AUX_FX_SPRING = 13,
    AUX_FX_COUNT = 14
};

inline const char* auxFxTypeName(int type)
{
    switch (type)
    {
        case AUX_FX_REVERB:      return "Studio Reverb";
        case AUX_FX_DELAY:       return "Stereo Delay";
        case AUX_FX_DRIVE:       return "Tape / Saturation";
        case AUX_FX_CHORUS:      return "Stereo Chorus";
        case AUX_FX_FLANGER:     return "Jet Flanger";
        case AUX_FX_PHASER:      return "Multi Phaser";
        case AUX_FX_COMP:        return "Bus Compressor";
        case AUX_FX_FILTER:      return "Resonant Filter";
        case AUX_FX_SHIMMER:     return "Shimmer Reverb";
        case AUX_FX_PINGPONG:    return "Ping-Pong Delay";
        case AUX_FX_GATED_VERB:  return "Gated Drum Reverb";
        case AUX_FX_TUBE:        return "Tube Warmth";
        case AUX_FX_PITCH:       return "Pitch Shifter";
        case AUX_FX_SPRING:      return "Spring Reverb";
        default:                 return "Studio Reverb";
    }
}

inline const char* getSyncDivisionName(int index)
{
    static const char* const kDivNames[12] = {
        "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8",
        "1/4T", "1/8D", "1/4", "1/4D", "1/2", "1/2D"
    };
    if (index >= 0 && index < 12)
        return kDivNames[index];
    return "1/8";
}

inline const double* getSyncDivisionMultipliers()
{
    static const double kSyncDivs[12] = {
        0.125,      // 0: 1/32
        0.166667,   // 1: 1/16T
        0.25,       // 2: 1/16
        0.333333,   // 3: 1/8T
        0.375,      // 4: 1/16D
        0.5,        // 5: 1/8
        0.666667,   // 6: 1/4T
        0.75,       // 7: 1/8D
        1.0,        // 8: 1/4
        1.5,        // 9: 1/4D
        2.0,        // 10: 1/2
        3.0         // 11: 1/2D
    };
    return kSyncDivs;
}

inline juce::String formatAuxParam(int fxType, int paramIdx, float val, float syncParamVal)
{
    const float v = juce::jlimit(0.0f, 1.0f, val);
    switch (fxType)
    {
        case AUX_FX_REVERB:
            if (paramIdx == 0) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " ms";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_DELAY:
            if (paramIdx == 0)
            {
                if (syncParamVal >= 0.5f)
                {
                    int divIdx = juce::jlimit(0, 11, (int) std::floor(v * 11.999f));
                    return getSyncDivisionName(divIdx);
                }
                float ms = 10.0f + v * 1400.0f;
                return (ms < 1000.f) ? juce::String((int) std::round(ms)) + " ms"
                                     : juce::String(ms * 0.001f, 2) + " s";
            }
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return (v >= 0.5f) ? "BPM" : "FREE";
            break;

        case AUX_FX_DRIVE:
            if (paramIdx == 0) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_CHORUS:
        case AUX_FX_FLANGER:
        case AUX_FX_PHASER:
            if (paramIdx == 0) return juce::String(0.1f + v * 5.0f, 1) + " Hz";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_COMP:
            if (paramIdx == 0) return juce::String(-v * 40.0f, 1) + " dB";
            if (paramIdx == 1) return juce::String(1.0f + v * 19.0f, 1) + ":1";
            if (paramIdx == 2) return juce::String((int) std::round(1.0f + v * 99.0f)) + " ms";
            if (paramIdx == 3) return juce::String((int) std::round(10.0f + v * 490.0f)) + " ms";
            break;

        case AUX_FX_FILTER:
            if (paramIdx == 0)
            {
                float hz = 80.f + std::pow(v, 2.5f) * 17900.f;
                return (hz < 1000.f) ? juce::String((int) std::round(hz)) + " Hz"
                                     : juce::String(hz * 0.001f, 1) + " kHz";
            }
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2)
            {
                int m = juce::jlimit(0, 2, (int) std::floor(v * 2.999f));
                return m == 0 ? "LP" : (m == 1 ? "BP" : "HP");
            }
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_SHIMMER:
            if (paramIdx == 0) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_PINGPONG:
            if (paramIdx == 0)
            {
                if (syncParamVal >= 0.5f)
                {
                    int divIdx = juce::jlimit(0, 11, (int) std::floor(v * 11.999f));
                    return getSyncDivisionName(divIdx);
                }
                float ms = 20.0f + v * 1200.0f;
                return (ms < 1000.f) ? juce::String((int) std::round(ms)) + " ms"
                                     : juce::String(ms * 0.001f, 2) + " s";
            }
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return (v >= 0.5f) ? "BPM" : "FREE";
            break;

        case AUX_FX_GATED_VERB:
            if (paramIdx == 0)
            {
                float sec = 0.04f + std::pow(v, 1.6f) * 2.46f;
                return (sec < 1.0f) ? juce::String((int) std::round(sec * 1000.f)) + " ms"
                                    : juce::String(sec, 2) + " s";
            }
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_TUBE:
            if (paramIdx == 0) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_PITCH:
            if (paramIdx == 0)
            {
                int semi = (int) std::round(-12.0f + v * 24.0f);
                return (semi > 0 ? "+" : "") + juce::String(semi) + " st";
            }
            if (paramIdx == 1)
            {
                int cents = (int) std::round((v - 0.5f) * 100.0f);
                return (cents > 0 ? "+" : "") + juce::String(cents) + " ct";
            }
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        case AUX_FX_SPRING:
            if (paramIdx == 0) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 1) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 2) return juce::String((int) std::round(v * 100.f)) + " %";
            if (paramIdx == 3) return juce::String((int) std::round(v * 100.f)) + " %";
            break;

        default:
            return juce::String((int) std::round(v * 100.f)) + " %";
    }
    return juce::String((int) std::round(v * 100.f)) + " %";
}

inline double parseAuxParamText(int fxType, int paramIdx, const juce::String& text, float syncParamVal)
{
    juce::String t = text.trim().toLowerCase();
    if (fxType == AUX_FX_DELAY || fxType == AUX_FX_PINGPONG)
    {
        if (paramIdx == 3)
        {
            if (t.contains("bpm") || t.contains("sync") || t.contains("on") || t.contains("1")) return 1.0;
            return 0.0;
        }
        if (paramIdx == 0 && syncParamVal >= 0.5f)
        {
            for (int i = 0; i < 12; ++i)
            {
                if (t.containsIgnoreCase(getSyncDivisionName(i)))
                    return ((double) i + 0.5) / 12.0;
            }
        }
    }
    if (t.contains("%"))
        return juce::jlimit(0.0, 1.0, (double) t.getFloatValue() / 100.0);
    if (t.contains("ms"))
        return juce::jlimit(0.0, 1.0, (double) t.getFloatValue() / 1000.0);
    double val = t.getDoubleValue();
    if (val > 1.0 && val <= 100.0) val /= 100.0;
    return juce::jlimit(0.0, 1.0, val);
}

inline juce::String formatAuxReturnLevel(float val)
{
    return juce::String((int) std::round(val * 100.f)) + " %";
}

inline juce::String formatAuxReturnPan(float val)
{
    if (val < -0.03f) return juce::String((int) std::round(-val * 100.f)) + " L";
    if (val > 0.03f)  return juce::String((int) std::round(val * 100.f)) + " R";
    return "C";
}

struct AuxBusParams
{
    int fxType = AUX_FX_REVERB;
    float p1 = 0.5f; // Size / Time / Drive / Rate / Thresh / Cutoff
    float p2 = 0.5f; // Damp / Feedback / Tone / Depth / Ratio / Reso
    float p3 = 0.5f; // PreDelay / HighCut / Bias / Feedback / Attack / Mode
    float p4 = 0.5f; // Width / PingPong / Drive / Width / Release / Drive
    float returnLevel = 0.8f;
    float returnPan = 0.0f;
    bool enabled = true;
};

struct MasterFXParams
{
    // Master Bus Compressor
    bool compOn = true;
    float compThresh = -14.f; // -40 .. 0 dB
    float compRatio = 4.f;    // 1.5 .. 10
    float compAtk = 10.f;     // 0.1 .. 30 ms
    float compRel = 100.f;    // 50 .. 1200 ms
    float compMakeup = 2.f;   // 0 .. 18 dB

    // Master 4-Band EQ
    bool eqOn = true;
    float eqLowGain = 0.f;    // -12 .. +12 dB @ 80 Hz
    float eqLowMidGain = 0.f; // -12 .. +12 dB @ 450 Hz
    float eqHiMidGain = 0.f;  // -12 .. +12 dB @ 2500 Hz
    float eqHighGain = 0.f;   // -12 .. +12 dB @ 10000 Hz

    // Master Drive & Limiter
    bool driveOn = true;
    float drive = 0.15f;      // 0 .. 1 (tape warmth)
    bool limiterOn = true;
    float ceiling = -0.3f;    // -3 .. 0 dB
};

class AuxBusManager
{
public:
    AuxBusManager();
    ~AuxBusManager();

    void prepare(double sr, int maxBlock);
    void reset();

    // Process one Aux bus (in-place on stereo buffer L, R)
    void processAux(int auxIdx, float* L, float* R, int n, const AuxBusParams& p);

    // Process Master Bus chain on stereo output
    void processMasterChain(float* L, float* R, int n, const MasterFXParams& p);

    // Gain reduction readouts for metering
    float getAuxMeter(int auxIdx) const { return auxMeters[(size_t) juce::jlimit(0, 3, auxIdx)].load(); }
    float getMasterCompGR() const { return masterCompGR.load(); }

    void setBpm(double bpm) { currentBpm = bpm > 20.0 ? bpm : 120.0; }
    double getBpm() const { return currentBpm; }

    AuxBusParams auxParams[4];
    MasterFXParams masterParams;

private:
    double sampleRate = 48000.0;
    double currentBpm = 120.0;

    // --- Reverb engine ---
    struct Comb { std::vector<float> buf; size_t idx = 0; float store = 0.f; };
    struct AP   { std::vector<float> buf; size_t idx = 0; };
    Comb combL[4][4], combR[4][4];
    AP apL[4][2], apR[4][2];

    // --- Delay engine ---
    std::vector<float> dlyBufL[4], dlyBufR[4];
    size_t dlyIdxL[4] = { 0 }, dlyIdxR[4] = { 0 };

    // --- Modulation engines ---
    juce::dsp::Chorus<float> chorus[4];
    juce::dsp::Phaser<float> phaser[4];
    float flangerPhase[4] = { 0.f };
    std::vector<float> flangerBufL[4], flangerBufR[4];
    size_t flangerIdx[4] = { 0 };

    // --- Filter engines ---
    juce::dsp::IIR::Filter<float> filterL[4], filterR[4];

    // --- Aux FX instance state ---
    int auxGateTimer[4] = { 0 };
    float auxGateEnv[4] = { 0.f };
    float auxShimPhase[4] = { 0.f };
    float auxPitchPh[4] = { 0.f };
    float dlyDampL[4] = { 0.f };
    float dlyDampR[4] = { 0.f };

    // --- Master Bus FX ---
    float masterCompEnv = 0.f, masterCompGain = 1.f;
    std::atomic<float> masterCompGR { 0.f };
    juce::dsp::IIR::Filter<float> masterEqL[4], masterEqR[4];
    std::atomic<float> auxMeters[4];
};

} // namespace f64
