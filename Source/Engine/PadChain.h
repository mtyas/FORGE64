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
    void prepare(double sr, int maxBlock);
    void process(float* L, float* R, int n, const PadParams& p);

private:
    double sampleRate = 48000.0;

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
};

} // namespace f64
