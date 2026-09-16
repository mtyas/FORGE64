#include "PadChain.h"
#include <cmath>

namespace f64 {

void PadChain::prepare(double sr, int maxBlock)
{
    sampleRate = sr;
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

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
    fxParamsDirty = true;
}

void PadChain::process(float* L, float* R, int n, const PadParams& p)
{
    // ---------------- EQ: smooth params, rebuild coefficients when they move
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
        *eqL[0].state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowShelf(
            sampleRate, (double) juce::jlimit(20.f, 18000.f, smEq[0]), (double) juce::jlimit(-24.f, 24.f, smEq[1]));
        *eqR[0].state = *eqL[0].state;
        *eqL[1].state = *juce::dsp::IIR::Coefficients<float>::makePeak(
            sampleRate, (double) juce::jlimit(40.f, 18000.f, smEq[2]), 1.0, (double) juce::jlimit(-24.f, 24.f, smEq[3]));
        *eqR[1].state = *eqL[1].state;
        *eqL[2].state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighShelf(
            sampleRate, (double) juce::jlimit(100.f, 19000.f, smEq[4]), (double) juce::jlimit(-24.f, 24.f, smEq[5]));
        *eqR[2].state = *eqL[2].state;
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
    const int fx = juce::jlimit(0, 4, p.fxType);
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
        fxParamsDirty = true;
    }

    const float fxp[4] = { p.fx1, p.fx2, p.fx3, p.fx4 };
    if (fx == 2 || fx == 4)
    {
        for (int i = 0; i < 4; ++i)
            if (std::abs(fxp[i] - lastFxP[i]) > 0.0005f)
            {
                lastFxP[i] = fxp[i];
                fxParamsDirty = true;
            }
        if (fxParamsDirty)
        {
            fxParamsDirty = false;
            if (fx == 2)
            {
                chorus.parameters.rate.setValueNotifyingHost(juce::jmax(0.01f, fxp[0] * 4.f));
                chorus.parameters.depth.setValueNotifyingHost(fxp[1] * 0.8f);
                chorus.parameters.feedback.setValueNotifyingHost(fxp[3] * 0.6f);
                chorus.parameters.mix.setValueNotifyingHost(0.3f + fxp[2] * 0.5f);
            }
            else
            {
                phaser.parameters.rate.setValueNotifyingHost(juce::jmax(0.01f, fxp[0] * 5.f));
                phaser.parameters.depth.setValueNotifyingHost(fxp[1] * 0.9f);
                phaser.parameters.centreFrequency.setValueNotifyingHost(200.f + fxp[2] * 6000.f);
                phaser.parameters.feedback.setValueNotifyingHost(fxp[3] * 0.7f);
            }
        }
    }

    const float flRate = 0.05f + p.fx1 * 5.f;
    const float flDepth = p.fx2 * 0.004f;
    const float flFb = juce::jlimit(0.f, 0.85f, p.fx3);
    const float flMix = p.fx4;
    const float crushLevels = std::pow(2.f, 2.f + p.fx1 * 12.f) * 0.5f;
    const float crushDiv = 1.f + p.fx2 * 39.f;
    const float crushMix = 0.2f + p.fx3 * 0.8f;
    const size_t flLen = flL.size();

    for (int i = 0; i < n; ++i)
    {
        float x = L[i], y = R[i];

        // EQ
        x = eqL[0].processSample(0, x);
        x = eqL[1].processSample(0, x);
        x = eqL[2].processSample(0, x);
        y = eqR[0].processSample(1, y);
        y = eqR[1].processSample(1, y);
        y = eqR[2].processSample(1, y);

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
            case 2: // Chorus
                x = chorus.processSample(0, x);
                y = chorus.processSample(1, y);
                break;
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
            case 4: // Phaser
                x = phaser.processSample(0, x);
                y = phaser.processSample(1, y);
                break;
            default:
                break;
        }

        L[i] = x;
        R[i] = y;
    }
}

} // namespace f64
