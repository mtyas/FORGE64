#pragma once
#include <JuceHeader.h>
#include "PadDefs.h"
#include <vector>

namespace f64 {

// Per-pad processing chain: 3-band parametric EQ -> VCA compressor ->
// saturation/drive -> multi-FX insert (Flanger / Chorus / Bitcrusher / Phaser).
// Aux sends are applied by the processor during bus routing.
class PadChain
{
public:
    PadChain();
    ~PadChain();

    void prepare(double sr, int maxBlock);
    void process(float* L, float* R, int n, const PadParams& p);

private:
    double sampleRate = 48000.0;

    // Resonant VCF (State Variable TPT Filter: Lowpass, Highpass, Bandpass, Notch)
    juce::dsp::StateVariableTPTFilter<float> vcfL, vcfR;
    float smVcfCut = 20000.f;
    float smVcfRes = 0.707f;
    int lastVcfType = -1;

    juce::dsp::IIR::Filter<float> eqL[3], eqR[3];
    float smEq[6] = { 200.f, 0.f, 1000.f, 0.f, 8000.f, 0.f };
    bool coeffsDirty = true;

    float cEnv = 0.f, cGain = 1.f;
    float smDrive = 0.f;

    std::vector<float> flL, flR;
    int flWrite = 0;
    float flPhase = 0.f;

    float holdL = 0.f, holdR = 0.f, crushAcc = 0.f;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    int lastFx = -1;

    // Additional Multi-FX States
    // 5: Warm Overdrive & 6: Fuzz
    float odToneL = 0.f, odToneR = 0.f;
    float fuzzHpL = 0.f, fuzzHpR = 0.f;

    // 7: Tape Delay
    std::vector<float> tapeL, tapeR;
    int tapeWrite = 0;
    float tapeDampL = 0.f, tapeDampR = 0.f;

    // 8: Plate Reverb (4-channel FDN)
    std::vector<float> plateBuf[4];
    int plateWrite[4] = { 0, 0, 0, 0 };
    float plateDamp[4] = { 0.f, 0.f, 0.f, 0.f };

    // 9: Pitch Shifter (Dual crossfaded delay grains)
    std::vector<float> pitchBufL, pitchBufR;
    int pitchWrite = 0;
    float pitchPhase = 0.f;
    float pitchFbL = 0.f, pitchFbR = 0.f;

    // 10: Formant Filter (Dual parallel resonant bandpasses)
    juce::dsp::IIR::Filter<float> formantF1_L, formantF1_R, formantF2_L, formantF2_R;
    float lastVowelPos = -1.f, lastReso = -1.f;

    // 11: Ring Modulator
    float ringPhase = 0.f;
};

} // namespace f64
