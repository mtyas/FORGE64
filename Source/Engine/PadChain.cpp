#include "PadChain.h"
#include <cmath>

namespace f64 {

PadChain::PadChain() = default;
PadChain::~PadChain() = default;

void PadChain::prepare(double sr, int maxBlock)
{
    sampleRate = sr;
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    vcfL.prepare(spec);
    vcfR.prepare(spec);
    vcfL.reset();
    vcfR.reset();

    for (int b = 0; b < 3; ++b)
    {
        eqL[b].prepare(spec);
        eqR[b].prepare(spec);
        eqL[b].reset();
        eqR[b].reset();
    }

    chorus.prepare(spec);
    phaser.prepare(spec);

    const size_t flLen = (size_t) (sr * 0.02) + 8;
    flL.assign(flLen, 0.f);
    flR.assign(flLen, 0.f);
    flWrite = 0;
    coeffsDirty = true;

    const size_t tapeLen = (size_t) (sr * 1.2) + 16;
    tapeL.assign(tapeLen, 0.f);
    tapeR.assign(tapeLen, 0.f);
    tapeWrite = 0;
    tapeDampL = tapeDampR = 0.f;

    static const int plateSizes[4] = { 1047, 1361, 1693, 2039 };
    for (int k = 0; k < 4; ++k)
    {
        const size_t sz = (size_t) std::max(64.0, plateSizes[k] * (sr / 44100.0)) + 16;
        plateBuf[k].assign(sz, 0.f);
        plateWrite[k] = 0;
        plateDamp[k] = 0.f;
    }

    const size_t pitchLen = (size_t) (sr * 0.1) + 16;
    pitchBufL.assign(pitchLen, 0.f);
    pitchBufR.assign(pitchLen, 0.f);
    pitchWrite = 0;
    pitchPhase = 0.f;
    pitchFbL = pitchFbR = 0.f;

    formantF1_L.prepare(spec);
    formantF1_R.prepare(spec);
    formantF2_L.prepare(spec);
    formantF2_R.prepare(spec);
    formantF1_L.reset();
    formantF1_R.reset();
    formantF2_L.reset();
    formantF2_R.reset();
    lastVowelPos = -1.f;
    lastReso = -1.f;

    ringPhase = 0.f;
    odToneL = odToneR = 0.f;
    fuzzHpL = fuzzHpR = 0.f;

    dcX_L = dcY_L = dcX_R = dcY_R = 0.f;

    static const int hallSizes[8] = { 1357, 1583, 1867, 2153, 2477, 2749, 3121, 3571 };
    for (int k = 0; k < 8; ++k)
    {
        const size_t sz = (size_t) std::max(64.0, hallSizes[k] * (sr / 44100.0)) + 32;
        hallBuf[k].assign(sz, 0.f);
        hallWrite[k] = 0;
        hallDamp[k] = 0.f;
    }

    const size_t shimLen = (size_t) (sr * 0.8) + 16;
    shimBufL.assign(shimLen, 0.f);
    shimBufR.assign(shimLen, 0.f);
    shimWrite = 0;
    shimPhase = 0.f;
    shimDampL = shimDampR = 0.f;

    const size_t spLen = (size_t) (sr * 0.15) + 16;
    springBufL.assign(spLen, 0.f);
    springBufR.assign(spLen, 0.f);
    springWrite = 0;
    std::fill(std::begin(spApL), std::end(spApL), 0.f);
    std::fill(std::begin(spApR), std::end(spApR), 0.f);
    springDampL = springDampR = 0.f;

    const size_t gateLen = (size_t) (sr * 3.0) + 16;
    gateBufL.assign(gateLen, 0.f);
    gateBufR.assign(gateLen, 0.f);
    gateWrite = 0;
    gateEnv = 0.f;
    gateTimer = 0;

    const size_t ppLen = (size_t) (sr * 1.5) + 16;
    ppDlyL.assign(ppLen, 0.f);
    ppDlyR.assign(ppLen, 0.f);
    ppWriteL = ppWriteR = 0;
    ppDampL = ppDampR = 0.f;

    const size_t dubLen = (size_t) (sr * 1.5) + 16;
    dubDlyL.assign(dubLen, 0.f);
    dubDlyR.assign(dubLen, 0.f);
    dubWrite = 0;
    dubFiltL1 = dubFiltL2 = dubFiltR1 = dubFiltR2 = 0.f;

    tubeToneL = tubeToneR = 0.f;
    foldSmL = foldSmR = 0.f;

    std::fill(std::begin(hilb1_L), std::end(hilb1_L), 0.f);
    std::fill(std::begin(hilb2_L), std::end(hilb2_L), 0.f);
    std::fill(std::begin(hilb1_R), std::end(hilb1_R), 0.f);
    std::fill(std::begin(hilb2_R), std::end(hilb2_R), 0.f);
    freqShiftPhase = 0.f;
    fsFbL = fsFbR = 0.f;

    const size_t detuneLen = (size_t) (sr * 0.1) + 16;
    detuneBufL.assign(detuneLen, 0.f);
    detuneBufR.assign(detuneLen, 0.f);
    detuneWrite = 0;
    detunePhaseL = detunePhaseR = 0.f;
}

void PadChain::process(float* L, float* R, int n, const PadParams& p)
{
    // ---------------- 1. Resonant VCF (before EQ)
    const int vType = juce::jlimit(0, 4, p.vcfType);
    if (vType != 0)
    {
        const float targetCut = juce::jlimit(20.f, (float) (sampleRate * 0.49), p.vcfCut);
        const float targetRes = juce::jlimit(0.1f, 10.f, p.vcfRes);
        smVcfCut += (targetCut - smVcfCut) * 0.25f;
        smVcfRes += (targetRes - smVcfRes) * 0.25f;

        if (vType != lastVcfType)
        {
            lastVcfType = vType;
            if (vType == 1)
            {
                vcfL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                vcfR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            }
            else if (vType == 2)
            {
                vcfL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
                vcfR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
            }
            else if (vType == 3 || vType == 4)
            {
                vcfL.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
                vcfR.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
            }
        }
        vcfL.setCutoffFrequency(smVcfCut);
        vcfR.setCutoffFrequency(smVcfCut);
        vcfL.setResonance(smVcfRes);
        vcfR.setResonance(smVcfRes);

        for (int i = 0; i < n; ++i)
        {
            if (vType == 4) // Notch = input - bandpass
            {
                const float bpL = vcfL.processSample(0, L[i]);
                const float bpR = vcfR.processSample(1, R[i]);
                L[i] = L[i] - bpL;
                R[i] = R[i] - bpR;
            }
            else
            {
                L[i] = vcfL.processSample(0, L[i]);
                R[i] = vcfR.processSample(1, R[i]);
            }
        }
    }

    // ---------------- 2. EQ: smooth params, rebuild coefficients when they move
    const float targets[6] = { p.eqLF, p.eqLG, p.eqMF, p.eqMG, p.eqHF, p.eqHG };
    bool moved = coeffsDirty;
    for (int i = 0; i < 6; ++i)
    {
        const float d = targets[i] - smEq[i];
        if (std::abs(d) > 0.01f * juce::jmax(1.f, std::abs(targets[i])))
        {
            smEq[i] += d * 0.2f;
            moved = true;
        }
        else
        {
            smEq[i] = targets[i];
        }
    }

    if (moved)
    {
        coeffsDirty = false;
        const float gLo = std::pow(10.f, juce::jlimit(-24.f, 24.f, smEq[1]) / 20.f);
        const float gMd = std::pow(10.f, juce::jlimit(-24.f, 24.f, smEq[3]) / 20.f);
        const float gHi = std::pow(10.f, juce::jlimit(-24.f, 24.f, smEq[5]) / 20.f);
        *eqL[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, juce::jlimit(20.f, 18000.f, smEq[0]), 0.707f, gLo);
        *eqR[0].coefficients = *eqL[0].coefficients;
        *eqL[1].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, juce::jlimit(40.f, 18000.f, smEq[2]), 1.0f, gMd);
        *eqR[1].coefficients = *eqL[1].coefficients;
        *eqL[2].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, juce::jlimit(100.f, 19000.f, smEq[4]), 0.707f, gHi);
        *eqR[2].coefficients = *eqL[2].coefficients;
    }

    // ---------------- compressor coefficients
    const bool compOn = p.cRat > 1.01f && p.cThr > -59.5f;
    const float atkC = std::exp(-1.f / (juce::jmax(0.1f, p.cAtk) * 0.001f * (float) sampleRate));
    const float relC = std::exp(-1.f / (juce::jmax(5.f, p.cRel) * 0.001f * (float) sampleRate));
    const float thrLin = std::pow(10.f, p.cThr / 20.f);

    // ---------------- drive
    smDrive += (p.drive - smDrive) * 0.2f;
    const bool driveOn = smDrive > 0.002f;
    const float driveGain = 1.f + smDrive * 12.f;
    const float driveNorm = 1.f / std::tanh(driveGain * 0.7f);

    // ---------------- insert FX state
    const int fx = juce::jlimit(0, 21, p.ifxType);
    if (fx != lastFx)
    {
        lastFx = fx;
        flPhase = 0.f;
        std::fill(flL.begin(), flL.end(), 0.f);
        std::fill(flR.begin(), flR.end(), 0.f);
        holdL = holdR = 0.f;
        crushAcc = 0.f;
        chorus.reset();
        phaser.reset();
        odToneL = odToneR = 0.f;
        fuzzHpL = fuzzHpR = 0.f;
        std::fill(tapeL.begin(), tapeL.end(), 0.f);
        std::fill(tapeR.begin(), tapeR.end(), 0.f);
        tapeWrite = 0;
        tapeDampL = tapeDampR = 0.f;
        for (int k = 0; k < 4; ++k)
        {
            std::fill(plateBuf[k].begin(), plateBuf[k].end(), 0.f);
            plateWrite[k] = 0;
            plateDamp[k] = 0.f;
        }
        std::fill(pitchBufL.begin(), pitchBufL.end(), 0.f);
        std::fill(pitchBufR.begin(), pitchBufR.end(), 0.f);
        pitchWrite = 0;
        pitchPhase = 0.f;
        pitchFbL = pitchFbR = 0.f;
        formantF1_L.reset();
        formantF1_R.reset();
        formantF2_L.reset();
        formantF2_R.reset();
        lastVowelPos = -1.f;
        lastReso = -1.f;
        ringPhase = 0.f;

        for (int k = 0; k < 8; ++k)
        {
            std::fill(hallBuf[k].begin(), hallBuf[k].end(), 0.f);
            hallWrite[k] = 0;
            hallDamp[k] = 0.f;
        }
        std::fill(shimBufL.begin(), shimBufL.end(), 0.f);
        std::fill(shimBufR.begin(), shimBufR.end(), 0.f);
        shimWrite = 0; shimPhase = 0.f; shimDampL = shimDampR = 0.f;
        std::fill(springBufL.begin(), springBufL.end(), 0.f);
        std::fill(springBufR.begin(), springBufR.end(), 0.f);
        springWrite = 0;
        std::fill(std::begin(spApL), std::end(spApL), 0.f);
        std::fill(std::begin(spApR), std::end(spApR), 0.f);
        springDampL = springDampR = 0.f;
        std::fill(gateBufL.begin(), gateBufL.end(), 0.f);
        std::fill(gateBufR.begin(), gateBufR.end(), 0.f);
        gateWrite = 0; gateEnv = 0.f; gateTimer = 0;
        std::fill(ppDlyL.begin(), ppDlyL.end(), 0.f);
        std::fill(ppDlyR.begin(), ppDlyR.end(), 0.f);
        ppWriteL = ppWriteR = 0; ppDampL = ppDampR = 0.f;
        std::fill(dubDlyL.begin(), dubDlyL.end(), 0.f);
        std::fill(dubDlyR.begin(), dubDlyR.end(), 0.f);
        dubWrite = 0; dubFiltL1 = dubFiltL2 = dubFiltR1 = dubFiltR2 = 0.f;
        tubeToneL = tubeToneR = 0.f; foldSmL = foldSmR = 0.f;
        std::fill(std::begin(hilb1_L), std::end(hilb1_L), 0.f);
        std::fill(std::begin(hilb2_L), std::end(hilb2_L), 0.f);
        std::fill(std::begin(hilb1_R), std::end(hilb1_R), 0.f);
        std::fill(std::begin(hilb2_R), std::end(hilb2_R), 0.f);
        freqShiftPhase = 0.f; fsFbL = fsFbR = 0.f;
        std::fill(detuneBufL.begin(), detuneBufL.end(), 0.f);
        std::fill(detuneBufR.begin(), detuneBufR.end(), 0.f);
        detuneWrite = 0; detunePhaseL = detunePhaseR = 0.f;
    }

    if (fx == 2)
    {
        chorus.setRate(juce::jmax(0.01f, p.ifx1 * 4.f));
        chorus.setDepth(p.ifx2 * 0.8f);
        chorus.setCentreDelay(7.f);
        chorus.setFeedback(p.ifx4 * 0.6f);
        chorus.setMix(0.3f + p.ifx3 * 0.5f);
    }
    else if (fx == 4)
    {
        phaser.setRate(juce::jmax(0.01f, p.ifx1 * 5.f));
        phaser.setDepth(p.ifx2 * 0.9f);
        phaser.setCentreFrequency(200.f + p.ifx3 * 6000.f);
        phaser.setFeedback(p.ifx4 * 0.7f);
        phaser.setMix(0.5f);
    }

    const float flRate = 0.05f + p.ifx1 * 5.f;
    const float flDepth = p.ifx2 * 0.004f;
    const float flFb = juce::jlimit(0.f, 0.85f, p.ifx3);
    const float flMix = p.ifx4;
    const float crushLevels = std::pow(2.f, 2.f + p.ifx1 * 12.f) * 0.5f;
    const float crushDiv = 1.f + p.ifx2 * 39.f;
    const float crushMix = 0.2f + p.ifx3 * 0.8f;
    const size_t flLen = flL.size();

    // FX 5: Warm Overdrive
    const float odDrive = 1.0f + p.ifx1 * 18.0f;
    const float odToneCut = 400.0f + p.ifx2 * 8000.0f;
    const float odToneAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * odToneCut / (float) sampleRate);
    const float odMix = p.ifx4;

    // FX 6: Fuzz / Distortion
    const float fzGain = 2.0f + p.ifx1 * 35.0f;
    const float fzHpCut = 20.0f + p.ifx3 * 350.0f;
    const float fzHpAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * fzHpCut / (float) sampleRate);
    const float fzBite = p.ifx2;
    const float fzMix = p.ifx4;

    // FX 7: Tape Delay
    const double dlySamples = (0.02 + (double) p.ifx1 * 0.73) * sampleRate;
    const float dlyFb = juce::jlimit(0.0f, 0.85f, p.ifx2);
    const float dlyDamp = 0.1f + p.ifx3 * 0.8f;
    const float dlyDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (8000.0f * (1.0f - dlyDamp * 0.75f)) / (float) sampleRate);
    const float dlyMix = p.ifx4;
    const size_t tLen = tapeL.size();

    // FX 8: Plate Reverb
    const float revFb = 0.3f + p.ifx2 * 0.62f;
    const float revDampCut = 1200.0f + (1.0f - p.ifx3) * 7000.0f;
    const float revDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * revDampCut / (float) sampleRate);
    const float revMix = p.ifx4;

    // FX 9: Pitch Shifter
    const float semitones = (p.ifx1 - 0.5f) * 24.0f + (p.ifx2 - 0.5f);
    const float pitchRatio = std::pow(2.0f, semitones / 12.0f);
    const float grainRate = (1.0f - pitchRatio) / (0.035f * (float) sampleRate);
    const float pShFb = juce::jlimit(0.0f, 0.75f, p.ifx3);
    const float pShMix = p.ifx4;
    const size_t pLen = pitchBufL.size();
    const float grainSamples = 0.035f * (float) sampleRate;

    // FX 10: Formant Filter
    static const float vF1[5] = { 800.f, 400.f, 280.f, 500.f, 320.f };
    static const float vF2[5] = { 1200.f, 2200.f, 2400.f, 900.f, 750.f };
    float vPos = juce::jlimit(0.0f, 3.999f, p.ifx1 * 4.0f);
    int vIdx = (int) vPos;
    float vFrac = vPos - (float) vIdx;
    float targetF1 = vF1[vIdx] + vFrac * (vF1[vIdx + 1] - vF1[vIdx]);
    float targetF2 = vF2[vIdx] + vFrac * (vF2[vIdx + 1] - vF2[vIdx]);
    float reso = 2.0f + p.ifx2 * 12.0f;
    if (std::abs(targetF1 - lastVowelPos) > 1.f || std::abs(reso - lastReso) > 0.05f)
    {
        lastVowelPos = targetF1;
        lastReso = reso;
        *formantF1_L.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, targetF1, reso);
        *formantF1_R.coefficients = *formantF1_L.coefficients;
        *formantF2_L.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, targetF2, reso);
        *formantF2_R.coefficients = *formantF2_L.coefficients;
    }
    const float f2Gain = 0.5f + p.ifx3 * 1.5f;
    const float fmtMix = p.ifx4;

    // FX 11: Ring Modulator
    const float modFreq = 20.0f * std::pow(200.0f, p.ifx1);
    const float phaseInc = juce::MathConstants<float>::twoPi * modFreq / (float) sampleRate;
    const float rMix = p.ifx4;
    const float rShape = p.ifx2;
    const float rDrive = 1.0f + p.ifx3 * 3.0f;

    // FX 12: Hall Reverb
    const float hallFb = 0.40f + p.ifx1 * 0.48f;
    const float hallDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (2000.f + (1.f - p.ifx2) * 11000.f) / (float) sampleRate);
    const float hallMix = p.ifx4;

    // FX 13: Shimmer Reverb
    const float shimFb = 0.30f + p.ifx1 * 0.48f; // Max 0.78
    const float shimOctaveAmt = p.ifx2 * 0.70f;
    const float shimDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (1500.f + (1.f - p.ifx3) * 9000.f) / (float) sampleRate);
    const float shimMix = p.ifx4;
    const float shimGrainSamples = 0.070f * (float) sampleRate;
    const float shimGrainRate = -1.0f / shimGrainSamples; // +1 octave
    const size_t sLen = shimBufL.size();

    // FX 14: Spring Reverb
    const float spTension = 0.30f + p.ifx1 * 0.45f; // Max 0.75
    const float spBoing = 0.2f + p.ifx2 * 0.6f;
    const float spDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (800.f + (1.f - p.ifx3) * 6000.f) / (float) sampleRate);
    const float spMix = p.ifx4;
    const size_t spLen = springBufL.size();

    // FX 15: Gated Reverb
    const int gateDurationSamples = (int) ((0.04f + std::pow(juce::jlimit(0.f, 1.f, p.ifx1), 1.6f) * 2.46f) * (float) sampleRate);
    const float gateToneAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (1000.f + p.ifx3 * 9000.f) / (float) sampleRate);
    const float gateMix = p.ifx4;
    const size_t gLen = gateBufL.size();

    // FX 16: Ping-Pong Delay
    const size_t ppSamples = (size_t) juce::jlimit(64.0, (double) ppDlyL.size() - 8.0, (0.02 + (double) p.ifx1 * 0.75) * sampleRate);
    const float ppFb = juce::jlimit(0.f, 0.86f, p.ifx2);
    const float ppDampAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (1500.f + (1.f - p.ifx3) * 12000.f) / (float) sampleRate);
    const float ppMix = p.ifx4;
    const size_t ppLen = ppDlyL.size();

    // FX 17: Filtered Dub Delay
    const size_t dubSamples = (size_t) juce::jlimit(64.0, (double) dubDlyL.size() - 8.0, (0.03 + (double) p.ifx1 * 0.85) * sampleRate);
    const float dubFb = juce::jlimit(0.f, 0.94f, p.ifx2);
    const float dubCut = juce::jlimit(120.f, 9000.f, 150.f + p.ifx3 * 8000.f);
    const float dubF = 2.0f * std::sin(juce::MathConstants<float>::pi * dubCut / (float) sampleRate);
    const float dubQ = 2.0f;
    const float dubMix = p.ifx4;
    const size_t dubLen = dubDlyL.size();

    // FX 18: Tube Saturator
    const float tubeGain = 1.0f + p.ifx1 * 22.0f;
    const float tubeBias = p.ifx2 * 0.6f;
    const float tubeToneCut = 1500.f + p.ifx3 * 10000.f;
    const float tubeToneAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * tubeToneCut / (float) sampleRate);
    const float tubeMix = p.ifx4;

    // FX 19: Wavefolder
    const float foldGain = 1.0f + p.ifx1 * 14.0f;
    const float foldSym = (p.ifx2 - 0.5f) * 0.4f;
    const float foldSmoothAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * (1000.f + (1.f - p.ifx3) * 12000.f) / (float) sampleRate);
    const float foldMix = p.ifx4;

    // FX 20: Frequency Shifter
    const float shiftHz = (p.ifx1 - 0.5f) * 1000.0f; // -500 Hz to +500 Hz
    const float fsInc = juce::MathConstants<float>::twoPi * shiftHz / (float) sampleRate;
    const float fsFb = juce::jlimit(0.f, 0.85f, p.ifx2);
    const float fsDir = p.ifx3;
    const float fsMix = p.ifx4;

    // FX 21: Stereo Detuner
    const float detuneCents = 1.0f + p.ifx1 * 36.0f;
    const float detRatioL = std::pow(2.0f, detuneCents / 1200.0f);
    const float detRatioR = std::pow(2.0f, -detuneCents / 1200.0f);
    const float grainSize = 0.03f * (float) sampleRate;
    const float gRateL = (1.0f - detRatioL) / grainSize;
    const float gRateR = (1.0f - detRatioR) / grainSize;
    const float detSpreadSamples = (0.003f + p.ifx2 * 0.022f) * (float) sampleRate;
    const float detFb = juce::jlimit(0.f, 0.70f, p.ifx3);
    const float detMix = p.ifx4;
    const size_t dLen = detuneBufL.size();

    for (int i = 0; i < n; ++i)
    {
        float x = L[i], y = R[i];

        // EQ
        x = eqL[0].processSample(x);
        x = eqL[1].processSample(x);
        x = eqL[2].processSample(x);
        y = eqR[0].processSample(y);
        y = eqR[1].processSample(y);
        y = eqR[2].processSample(y);

        // Compressor (stereo-linked detector)
        if (compOn)
        {
            const float det = juce::jmax(std::abs(x), std::abs(y));
            cEnv = det > cEnv ? cEnv + (det - cEnv) * (1.f - atkC)
                              : cEnv + (det - cEnv) * (1.f - relC);
            float g = 1.f;
            if (cEnv > thrLin)
            {
                const float envDb = 20.f * std::log10(cEnv + 1e-9f);
                const float gr = (p.cThr - envDb) * (1.f - 1.f / p.cRat);
                g = std::pow(10.f, gr / 20.f);
            }
            cGain = g < cGain ? cGain + (g - cGain) * (1.f - atkC)
                              : cGain + (g - cGain) * (1.f - relC);
            x *= cGain;
            y *= cGain;
        }

        // Drive / saturation
        if (driveOn)
        {
            x = std::tanh(x * driveGain) * driveNorm;
            y = std::tanh(y * driveGain) * driveNorm;
        }

        // Insert multi-FX
        switch (fx)
        {
            case 1: // Flanger
            {
                flPhase += juce::MathConstants<float>::twoPi * flRate / (float) sampleRate;
                if (flPhase > juce::MathConstants<float>::twoPi)
                    flPhase -= juce::MathConstants<float>::twoPi;
                const double delay = (0.003 + (double) flDepth * 0.5 * (1.0 + std::sin(flPhase))) * sampleRate;
                const size_t w = (size_t) flWrite;

                auto readAt = [&](const std::vector<float>& buf, double d)
                {
                    double rp = (double) w - d;
                    while (rp < 0.0)
                        rp += (double) flLen;
                    const size_t i0 = (size_t) rp % flLen;
                    const size_t i1 = (i0 + 1) % flLen;
                    const float f = (float) (rp - std::floor(rp));
                    return buf[i0] + f * (buf[i1] - buf[i0]);
                };

                const float dl = readAt(flL, delay);
                const float dr = readAt(flR, delay);
                flL[w] = x + dl * flFb;
                flR[w] = y + dr * flFb;
                flWrite = (flWrite + 1) % (int) flLen;
                x += dl * flMix;
                y += dr * flMix;
                break;
            }
            case 3: // Bitcrusher
            {
                crushAcc += 1.f;
                if (crushAcc >= crushDiv)
                {
                    crushAcc -= crushDiv;
                    holdL = std::round(x * crushLevels) / crushLevels;
                    holdR = std::round(y * crushLevels) / crushLevels;
                }
                x += (holdL - x) * crushMix;
                y += (holdR - y) * crushMix;
                break;
            }
            case 5: // Warm Overdrive
            {
                const float inL_od = x * odDrive;
                const float inR_od = y * odDrive;
                const float satL = (inL_od + 0.15f * inL_od * std::abs(inL_od)) / (1.0f + std::abs(inL_od));
                const float satR = (inR_od + 0.15f * inR_od * std::abs(inR_od)) / (1.0f + std::abs(inR_od));
                odToneL += (satL - odToneL) * odToneAlpha;
                odToneR += (satR - odToneR) * odToneAlpha;
                x += (odToneL - x) * odMix;
                y += (odToneR - y) * odMix;
                break;
            }
            case 6: // Fuzz / Distortion
            {
                fuzzHpL += (x - fuzzHpL) * fzHpAlpha;
                fuzzHpR += (y - fuzzHpR) * fzHpAlpha;
                const float fzInL = (x - fuzzHpL) * fzGain;
                const float fzInR = (y - fuzzHpR) * fzGain;
                float fzOutL = std::tanh(fzInL);
                float fzOutR = std::tanh(fzInR);
                if (fzBite > 0.05f)
                {
                    fzOutL += std::sin(fzInL * 1.5f) * fzBite * 0.4f;
                    fzOutR += std::sin(fzInR * 1.5f) * fzBite * 0.4f;
                    fzOutL = std::tanh(fzOutL);
                    fzOutR = std::tanh(fzOutR);
                }
                x += (fzOutL - x) * fzMix;
                y += (fzOutR - y) * fzMix;
                break;
            }
            case 7: // Tape Delay
            {
                double rp = (double) tapeWrite - dlySamples;
                while (rp < 0.0) rp += (double) tLen;
                const size_t i0 = (size_t) rp % tLen;
                const size_t i1 = (i0 + 1) % tLen;
                const float frac = (float) (rp - std::floor(rp));
                const float readL = tapeL[i0] + frac * (tapeL[i1] - tapeL[i0]);
                const float readR = tapeR[i0] + frac * (tapeR[i1] - tapeR[i0]);
                tapeDampL += (readL - tapeDampL) * dlyDampAlpha;
                tapeDampR += (readR - tapeDampR) * dlyDampAlpha;
                tapeL[(size_t) tapeWrite] = std::tanh(x + tapeDampL * dlyFb);
                tapeR[(size_t) tapeWrite] = std::tanh(y + tapeDampR * dlyFb);
                tapeWrite = (tapeWrite + 1) % (int) tLen;
                x += readL * dlyMix;
                y += readR * dlyMix;
                break;
            }
            case 8: // Plate Reverb
            {
                const float inSum = (x + y) * 0.5f;
                float outs[4];
                for (int k = 0; k < 4; ++k)
                {
                    outs[k] = plateBuf[k][(size_t) plateWrite[k]];
                    plateDamp[k] += (outs[k] - plateDamp[k]) * revDampAlpha;
                }
                const float h0 = 0.5f * ( plateDamp[0] + plateDamp[1] + plateDamp[2] + plateDamp[3]);
                const float h1 = 0.5f * ( plateDamp[0] - plateDamp[1] + plateDamp[2] - plateDamp[3]);
                const float h2 = 0.5f * ( plateDamp[0] + plateDamp[1] - plateDamp[2] - plateDamp[3]);
                const float h3 = 0.5f * ( plateDamp[0] - plateDamp[1] - plateDamp[2] + plateDamp[3]);
                plateBuf[0][(size_t) plateWrite[0]] = inSum + h0 * revFb;
                plateBuf[1][(size_t) plateWrite[1]] = inSum + h1 * revFb;
                plateBuf[2][(size_t) plateWrite[2]] = inSum + h2 * revFb;
                plateBuf[3][(size_t) plateWrite[3]] = inSum + h3 * revFb;
                for (int k = 0; k < 4; ++k)
                    plateWrite[k] = (plateWrite[k] + 1) % (int) plateBuf[k].size();
                x += (outs[0] + outs[2]) * 0.5f * revMix;
                y += (outs[1] + outs[3]) * 0.5f * revMix;
                break;
            }
            case 9: // Pitch Shifter
            {
                pitchPhase += grainRate;
                while (pitchPhase >= 1.0f) pitchPhase -= 1.0f;
                while (pitchPhase < 0.0f)  pitchPhase += 1.0f;
                const float phase1 = pitchPhase;
                float phase2 = pitchPhase + 0.5f;
                if (phase2 >= 1.0f) phase2 -= 1.0f;
                const float w1 = 1.0f - std::abs(2.0f * phase1 - 1.0f);
                const float w2 = 1.0f - std::abs(2.0f * phase2 - 1.0f);
                double rp1 = (double) pitchWrite - (double) (phase1 * grainSamples);
                while (rp1 < 0.0) rp1 += (double) pLen;
                double rp2 = (double) pitchWrite - (double) (phase2 * grainSamples);
                while (rp2 < 0.0) rp2 += (double) pLen;
                const size_t i0_1 = (size_t) rp1 % pLen;
                const size_t i0_2 = (size_t) rp2 % pLen;
                const float s1L = pitchBufL[i0_1];
                const float s2L = pitchBufL[i0_2];
                const float s1R = pitchBufR[i0_1];
                const float s2R = pitchBufR[i0_2];
                const float outShiftL = s1L * w1 + s2L * w2;
                const float outShiftR = s1R * w1 + s2R * w2;
                pitchBufL[(size_t) pitchWrite] = std::tanh(x + outShiftL * pShFb);
                pitchBufR[(size_t) pitchWrite] = std::tanh(y + outShiftR * pShFb);
                pitchWrite = (pitchWrite + 1) % (int) pLen;
                x += (outShiftL - x) * pShMix;
                y += (outShiftR - y) * pShMix;
                break;
            }
            case 10: // Formant Filter
            {
                const float fOutL = formantF1_L.processSample(x) + formantF2_L.processSample(x) * f2Gain;
                const float fOutR = formantF1_R.processSample(y) + formantF2_R.processSample(y) * f2Gain;
                x += (fOutL * 2.5f - x) * fmtMix;
                y += (fOutR * 2.5f - y) * fmtMix;
                break;
            }
            case 11: // Ring Modulator
            {
                ringPhase += phaseInc;
                if (ringPhase > juce::MathConstants<float>::twoPi) ringPhase -= juce::MathConstants<float>::twoPi;
                const float carSine = std::sin(ringPhase);
                const float carTri = 2.0f * std::asin(juce::jlimit(-0.999f, 0.999f, carSine)) / juce::MathConstants<float>::halfPi;
                const float carSq = carSine >= 0.f ? 1.0f : -1.0f;
                float carrier = (rShape <= 0.5f) ? (carSine + (carTri - carSine) * (rShape * 2.0f))
                                                 : (carTri + (carSq - carTri) * ((rShape - 0.5f) * 2.0f));
                carrier = std::tanh(carrier * rDrive);
                x += (x * carrier - x) * rMix;
                y += (y * carrier - y) * rMix;
                break;
            }
            case 12: // Hall Reverb (8-delay FDN)
            {
                const float inSum = (x + y) * 0.5f;
                float outs[8];
                for (int k = 0; k < 8; ++k)
                {
                    outs[k] = hallBuf[k][(size_t) hallWrite[k]];
                    hallDamp[k] += (outs[k] - hallDamp[k]) * hallDampAlpha;
                }
                // 8x8 Householder reflection matrix
                float sumDamp = 0.f;
                for (int k = 0; k < 8; ++k) sumDamp += hallDamp[k];
                const float hSub = sumDamp * 0.25f;

                for (int k = 0; k < 8; ++k)
                {
                    hallBuf[k][(size_t) hallWrite[k]] = inSum + (hallDamp[k] - hSub) * hallFb;
                    hallWrite[k] = (hallWrite[k] + 1) % (int) hallBuf[k].size();
                }
                const float wetL = (outs[0] + outs[2] + outs[4] + outs[6]) * 0.25f;
                const float wetR = (outs[1] + outs[3] + outs[5] + outs[7]) * 0.25f;
                x += (wetL - x) * hallMix;
                y += (wetR - y) * hallMix;
                break;
            }
            case 13: // Shimmer Reverb (FDN with octave-up pitch in feedback)
            {
                const float inSum = (x + y) * 0.5f;
                shimPhase += shimGrainRate;
                while (shimPhase >= 1.0f) shimPhase -= 1.0f;
                while (shimPhase < 0.0f)  shimPhase += 1.0f;
                const float ph1 = shimPhase;
                float ph2 = shimPhase + 0.5f;
                const float w1 = 0.5f * (1.0f - std::cos(ph1 * juce::MathConstants<float>::twoPi));
                const float w2 = 0.5f * (1.0f - std::cos(ph2 * juce::MathConstants<float>::twoPi));

                double rp1 = (double) shimWrite - (double) (ph1 * shimGrainSamples);
                while (rp1 < 0.0) rp1 += (double) sLen;
                size_t i0_1 = (size_t) rp1 % sLen;
                size_t i1_1 = (i0_1 + 1) % sLen;
                float f1 = (float) (rp1 - std::floor(rp1));
                float rL1 = shimBufL[i0_1] + f1 * (shimBufL[i1_1] - shimBufL[i0_1]);
                float rR1 = shimBufR[i0_1] + f1 * (shimBufR[i1_1] - shimBufR[i0_1]);

                double rp2 = (double) shimWrite - (double) (ph2 * shimGrainSamples);
                while (rp2 < 0.0) rp2 += (double) sLen;
                size_t i0_2 = (size_t) rp2 % sLen;
                size_t i1_2 = (i0_2 + 1) % sLen;
                float f2 = (float) (rp2 - std::floor(rp2));
                float rL2 = shimBufL[i0_2] + f2 * (shimBufL[i1_2] - shimBufL[i0_2]);
                float rR2 = shimBufR[i0_2] + f2 * (shimBufR[i1_2] - shimBufR[i0_2]);

                const float pitchOutL = rL1 * w1 + rL2 * w2;
                const float pitchOutR = rR1 * w2 + rR2 * w1;

                shimDampL += (pitchOutL - shimDampL) * shimDampAlpha;
                shimDampR += (pitchOutR - shimDampR) * shimDampAlpha;
                if (std::abs(shimDampL) < 1e-7f) shimDampL = 0.f;
                if (std::abs(shimDampR) < 1e-7f) shimDampR = 0.f;

                const float readL = shimBufL[(size_t) shimWrite];
                const float readR = shimBufR[(size_t) shimWrite];

                const float octMix = shimOctaveAmt * 0.5f;
                const float loopL = (readR * (1.0f - octMix) + shimDampL * octMix) * shimFb;
                const float loopR = (readL * (1.0f - octMix) + shimDampR * octMix) * shimFb;

                shimBufL[(size_t) shimWrite] = std::tanh(inSum + loopL);
                shimBufR[(size_t) shimWrite] = std::tanh(inSum + loopR);
                shimWrite = (shimWrite + 1) % (int) sLen;

                x += (readL - x) * shimMix;
                y += (readR - y) * shimMix;
                break;
            }
            case 14: // Spring Reverb (Dispersive allpasses + dual tank)
            {
                const float inL = x, inR = y;
                const float c1 = 0.5f * spBoing, c2 = 0.4f * spBoing, c3 = 0.35f * spBoing;
                float apL1 = inL - c1 * spApL[0]; float outApL1 = spApL[0] + c1 * apL1; spApL[0] = apL1;
                float apL2 = outApL1 - c2 * spApL[1]; float outApL2 = spApL[1] + c2 * apL2; spApL[1] = apL2;
                float apL3 = outApL2 - c3 * spApL[2]; float outApL3 = spApL[2] + c3 * apL3; spApL[2] = apL3;

                float apR1 = inR - c1 * spApR[0]; float outApR1 = spApR[0] + c1 * apR1; spApR[0] = apR1;
                float apR2 = outApR1 - c2 * spApR[1]; float outApR2 = spApR[1] + c2 * apR2; spApR[1] = apR2;
                float apR3 = outApR2 - c3 * spApR[2]; float outApR3 = spApR[2] + c3 * apR3; spApR[2] = apR3;

                const size_t spDlyL = (size_t) (0.027 * sampleRate);
                const size_t spDlyR = (size_t) (0.034 * sampleRate);
                double rpL = (double) springWrite - (double) spDlyL;
                while (rpL < 0.0) rpL += (double) spLen;
                double rpR = (double) springWrite - (double) spDlyR;
                while (rpR < 0.0) rpR += (double) spLen;

                const float tankOutL = springBufL[(size_t) rpL % spLen];
                const float tankOutR = springBufR[(size_t) rpR % spLen];

                springDampL += (tankOutL - springDampL) * spDampAlpha;
                springDampR += (tankOutR - springDampR) * spDampAlpha;
                if (std::abs(springDampL) < 1e-7f) springDampL = 0.f;
                if (std::abs(springDampR) < 1e-7f) springDampR = 0.f;

                springBufL[(size_t) springWrite] = std::tanh(outApL3 + springDampR * spTension);
                springBufR[(size_t) springWrite] = std::tanh(outApR3 + springDampL * spTension);
                springWrite = (springWrite + 1) % (int) spLen;

                x += (springDampL - x) * spMix;
                y += (springDampR - y) * spMix;
                break;
            }
            case 15: // Gated Reverb (Early reflections + timed gate envelope)
            {
                const float inSig = std::abs(x) + std::abs(y);
                if (inSig > 0.04f)
                    gateTimer = gateDurationSamples;
                else if (gateTimer > 0)
                    --gateTimer;

                const float gateGain = gateTimer > 0 ? 1.0f : 0.0f;
                gateEnv += (gateGain - gateEnv) * 0.05f;

                const size_t gW = (size_t) gateWrite;
                gateBufL[gW] = x;
                gateBufR[gW] = y;

                static const float gTaps[6] = { 0.012f, 0.027f, 0.045f, 0.068f, 0.092f, 0.125f };
                float refL = 0.f, refR = 0.f;
                for (int k = 0; k < 6; ++k)
                {
                    double rpL = (double) gW - (double) (gTaps[k] * sampleRate);
                    while (rpL < 0.0) rpL += (double) gLen;
                    double rpR = (double) gW - (double) ((gTaps[k] + 0.007f) * sampleRate);
                    while (rpR < 0.0) rpR += (double) gLen;
                    refL += gateBufL[(size_t) rpL % gLen] * (1.0f - (float) k * 0.12f);
                    refR += gateBufR[(size_t) rpR % gLen] * (1.0f - (float) k * 0.12f);
                }
                gateWrite = (gateWrite + 1) % (int) gLen;
                refL = std::tanh(refL * 0.8f) * gateEnv;
                refR = std::tanh(refR * 0.8f) * gateEnv;

                x += (refL - x) * gateMix;
                y += (refR - y) * gateMix;
                break;
            }
            case 16: // Ping-Pong Delay (Cross-feedback stereo delay)
            {
                double rpL = (double) ppWriteL - (double) ppSamples;
                while (rpL < 0.0) rpL += (double) ppLen;
                double rpR = (double) ppWriteR - (double) ppSamples;
                while (rpR < 0.0) rpR += (double) ppLen;

                const size_t iL0 = (size_t) rpL % ppLen;
                const size_t iL1 = (iL0 + 1) % ppLen;
                const float fracL = (float) (rpL - std::floor(rpL));
                const float dlyOutL = ppDlyL[iL0] + fracL * (ppDlyL[iL1] - ppDlyL[iL0]);

                const size_t iR0 = (size_t) rpR % ppLen;
                const size_t iR1 = (iR0 + 1) % ppLen;
                const float fracR = (float) (rpR - std::floor(rpR));
                const float dlyOutR = ppDlyR[iR0] + fracR * (ppDlyR[iR1] - ppDlyR[iR0]);

                ppDampL += (dlyOutL - ppDampL) * ppDampAlpha;
                ppDampR += (dlyOutR - ppDampR) * ppDampAlpha;

                ppDlyL[(size_t) ppWriteL] = std::tanh(x + ppDampR * ppFb);
                ppDlyR[(size_t) ppWriteR] = std::tanh(y + ppDampL * ppFb);
                ppWriteL = (ppWriteL + 1) % (int) ppLen;
                ppWriteR = (ppWriteR + 1) % (int) ppLen;

                x += (dlyOutL - x) * ppMix;
                y += (dlyOutR - y) * ppMix;
                break;
            }
            case 17: // Filtered Dub Delay (Resonant lowpass in feedback loop)
            {
                double rp = (double) dubWrite - (double) dubSamples;
                while (rp < 0.0) rp += (double) dubLen;
                const size_t i0 = (size_t) rp % dubLen;
                const size_t i1 = (i0 + 1) % dubLen;
                const float frac = (float) (rp - std::floor(rp));
                const float dL = dubDlyL[i0] + frac * (dubDlyL[i1] - dubDlyL[i0]);
                const float dR = dubDlyR[i0] + frac * (dubDlyR[i1] - dubDlyR[i0]);

                dubFiltL1 += dubF * (dL - dubFiltL1 - dubFiltL2 / dubQ);
                dubFiltL2 += dubF * dubFiltL1;
                dubFiltR1 += dubF * (dR - dubFiltR1 - dubFiltR2 / dubQ);
                dubFiltR2 += dubF * dubFiltR1;

                const float fbL = std::tanh(dubFiltL2 * 1.1f);
                const float fbR = std::tanh(dubFiltR2 * 1.1f);

                dubDlyL[(size_t) dubWrite] = std::tanh(x + fbL * dubFb);
                dubDlyR[(size_t) dubWrite] = std::tanh(y + fbR * dubFb);
                dubWrite = (dubWrite + 1) % (int) dubLen;

                x += (dL - x) * dubMix;
                y += (dR - y) * dubMix;
                break;
            }
            case 18: // Tube Saturator (Triode warm saturation)
            {
                const float inL_t = x * tubeGain;
                const float inR_t = y * tubeGain;
                float satL = (inL_t + tubeBias * inL_t * std::abs(inL_t)) / (1.0f + std::abs(inL_t));
                float satR = (inR_t + tubeBias * inR_t * std::abs(inR_t)) / (1.0f + std::abs(inR_t));
                tubeToneL += (satL - tubeToneL) * tubeToneAlpha;
                tubeToneR += (satR - tubeToneR) * tubeToneAlpha;
                x += (tubeToneL - x) * tubeMix;
                y += (tubeToneR - y) * tubeMix;
                break;
            }
            case 19: // Wavefolder (West-coast multi-stage wavefolding)
            {
                float sL = (x + foldSym) * foldGain;
                float sR = (y + foldSym) * foldGain;
                sL = std::sin(sL * 1.5707963f);
                sL = std::sin(sL * 1.5707963f * (1.0f + p.ifx1 * 1.5f));
                sR = std::sin(sR * 1.5707963f);
                sR = std::sin(sR * 1.5707963f * (1.0f + p.ifx1 * 1.5f));

                foldSmL += (sL - foldSmL) * foldSmoothAlpha;
                foldSmR += (sR - foldSmR) * foldSmoothAlpha;
                x += (foldSmL - x) * foldMix;
                y += (foldSmR - y) * foldMix;
                break;
            }
            case 20: // Frequency Shifter (Quadrature Hilbert Transform)
            {
                freqShiftPhase += fsInc;
                if (freqShiftPhase > juce::MathConstants<float>::twoPi)  freqShiftPhase -= juce::MathConstants<float>::twoPi;
                if (freqShiftPhase < -juce::MathConstants<float>::twoPi) freqShiftPhase += juce::MathConstants<float>::twoPi;

                const float cosCar = std::cos(freqShiftPhase);
                const float sinCar = std::sin(freqShiftPhase);

                static const float c1[4] = { 0.161758f, 0.733029f, 0.945350f, 0.990598f };
                static const float c2[4] = { 0.479401f, 0.876218f, 0.976599f, 0.997500f };

                const float inL_fs = x + fsFbL * fsFb;
                const float inR_fs = y + fsFbR * fsFb;

                float x1L = inL_fs, x1R = inR_fs;
                for (int k = 0; k < 4; ++k)
                {
                    float yL = c1[k] * (x1L - hilb1_L[k]) + hilb1_L[k];
                    hilb1_L[k] = x1L; x1L = yL;
                    float yR = c1[k] * (x1R - hilb1_R[k]) + hilb1_R[k];
                    hilb1_R[k] = x1R; x1R = yR;
                }
                float x2L = inL_fs, x2R = inR_fs;
                for (int k = 0; k < 4; ++k)
                {
                    float yL = c2[k] * (x2L - hilb2_L[k]) + hilb2_L[k];
                    hilb2_L[k] = x2L; x2L = yL;
                    float yR = c2[k] * (x2R - hilb2_R[k]) + hilb2_R[k];
                    hilb2_R[k] = x2R; x2R = yR;
                }

                const float upL = x1L * cosCar - x2L * sinCar;
                const float downL = x1L * cosCar + x2L * sinCar;
                const float shiftL = downL * (1.0f - fsDir) + upL * fsDir;

                const float upR = x1R * cosCar - x2R * sinCar;
                const float downR = x1R * cosCar + x2R * sinCar;
                const float shiftR = downR * (1.0f - fsDir) + upR * fsDir;

                fsFbL = std::tanh(shiftL);
                fsFbR = std::tanh(shiftR);

                x += (shiftL - x) * fsMix;
                y += (shiftR - y) * fsMix;
                break;
            }
            case 21: // Stereo Detuner (Dual micro-pitch grains)
            {
                detunePhaseL += gRateL;
                while (detunePhaseL >= 1.0f) detunePhaseL -= 1.0f;
                while (detunePhaseL < 0.0f)  detunePhaseL += 1.0f;
                detunePhaseR += gRateR;
                while (detunePhaseR >= 1.0f) detunePhaseR -= 1.0f;
                while (detunePhaseR < 0.0f)  detunePhaseR += 1.0f;

                const float wL1 = 1.0f - std::abs(2.0f * detunePhaseL - 1.0f);
                float phL2 = detunePhaseL + 0.5f; if (phL2 >= 1.0f) phL2 -= 1.0f;
                const float wL2 = 1.0f - std::abs(2.0f * phL2 - 1.0f);

                const float wR1 = 1.0f - std::abs(2.0f * detunePhaseR - 1.0f);
                float phR2 = detunePhaseR + 0.5f; if (phR2 >= 1.0f) phR2 -= 1.0f;
                const float wR2 = 1.0f - std::abs(2.0f * phR2 - 1.0f);

                double rpL1 = (double) detuneWrite - (double) (detunePhaseL * grainSize);
                while (rpL1 < 0.0) rpL1 += (double) dLen;
                double rpL2 = (double) detuneWrite - (double) (phL2 * grainSize);
                while (rpL2 < 0.0) rpL2 += (double) dLen;

                double rpR1 = (double) detuneWrite - (double) (detunePhaseR * grainSize + detSpreadSamples);
                while (rpR1 < 0.0) rpR1 += (double) dLen;
                double rpR2 = (double) detuneWrite - (double) (phR2 * grainSize + detSpreadSamples);
                while (rpR2 < 0.0) rpR2 += (double) dLen;

                const float outL_det = detuneBufL[(size_t) rpL1 % dLen] * wL1 + detuneBufL[(size_t) rpL2 % dLen] * wL2;
                const float outR_det = detuneBufR[(size_t) rpR1 % dLen] * wR1 + detuneBufR[(size_t) rpR2 % dLen] * wR2;

                detuneBufL[(size_t) detuneWrite] = std::tanh(x + outL_det * detFb);
                detuneBufR[(size_t) detuneWrite] = std::tanh(y + outR_det * detFb);
                detuneWrite = (detuneWrite + 1) % (int) dLen;

                x += (outL_det - x) * detMix;
                y += (outR_det - y) * detMix;
                break;
            }
            default:
                break;
        }

        L[i] = x;
        R[i] = y;
    }

    // Chorus/Phaser are block-based in JUCE 8: wrap L/R in a non-owning buffer.
    if (fx == 2 || fx == 4)
    {
        float* chans[2] = { L, R };
        juce::AudioBuffer<float> wrap(chans, 2, n);
        juce::dsp::AudioBlock<float> block(wrap);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        if (fx == 2) chorus.process(ctx);
        else         phaser.process(ctx);
    }

    // Output 5 Hz DC blocker to cleanse sub-audible DC bias
    const float R_dc = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        dcY_L = inL - dcX_L + R_dc * dcY_L;
        dcX_L = inL;
        L[i] = dcY_L;

        const float inR = R[i];
        dcY_R = inR - dcX_R + R_dc * dcY_R;
        dcX_R = inR;
        R[i] = dcY_R;
    }
}

} // namespace f64
