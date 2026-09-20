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
    const int fx = juce::jlimit(0, 11, p.ifxType);
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
}

} // namespace f64
