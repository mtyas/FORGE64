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
        case AUX_FX_PITCH:       return "Pitch Detuner";
        case AUX_FX_SPRING:      return "Spring Reverb";
        default:                 return "Studio Reverb";
    }
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

    AuxBusParams auxParams[4];
    MasterFXParams masterParams;

private:
    double sampleRate = 48000.0;

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

    // --- Master Bus FX ---
    float masterCompEnv = 0.f, masterCompGain = 1.f;
    std::atomic<float> masterCompGR { 0.f };
    juce::dsp::IIR::Filter<float> masterEqL[4], masterEqR[4];
    std::atomic<float> auxMeters[4];
};

} // namespace f64
