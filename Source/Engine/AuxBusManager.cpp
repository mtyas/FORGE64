#include "AuxBusManager.h"
#include <cmath>

namespace f64 {

AuxBusManager::AuxBusManager()
{
    auxParams[0].fxType = AUX_FX_REVERB;
    auxParams[1].fxType = AUX_FX_DELAY;
    auxParams[2].fxType = AUX_FX_DRIVE;
    auxParams[3].fxType = AUX_FX_CHORUS;

    for (int i = 0; i < 4; ++i)
        auxMeters[i].store(0.f);
}

AuxBusManager::~AuxBusManager() = default;

void AuxBusManager::prepare(double sr, int maxBlock)
{
    sampleRate = sr;
    const double scale = sr / 44100.0;
    static const int combT[4] = { 1116, 1188, 1277, 1356 };
    static const int apT[2]   = { 556, 441 };

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = 2;

    for (int b = 0; b < 4; ++b)
    {
        for (int c = 0; c < 4; ++c)
        {
            combL[b][c].buf.assign((size_t) std::max(16.0, combT[c] * scale) + 4, 0.f);
            combR[b][c].buf.assign((size_t) std::max(16.0, (combT[c] + 23) * scale) + 4, 0.f);
            combL[b][c].idx = combR[b][c].idx = 0;
            combL[b][c].store = combR[b][c].store = 0.f;
        }
        for (int a = 0; a < 2; ++a)
        {
            apL[b][a].buf.assign((size_t) std::max(16.0, apT[a] * scale) + 4, 0.f);
            apR[b][a].buf.assign((size_t) std::max(16.0, (apT[a] + 23) * scale) + 4, 0.f);
            apL[b][a].idx = apR[b][a].idx = 0;
        }

        dlyBufL[b].assign((size_t) (sr * 2.2) + 8, 0.f);
        dlyBufR[b].assign((size_t) (sr * 2.2) + 8, 0.f);
        dlyIdxL[b] = dlyIdxR[b] = 0;

        flangerBufL[b].assign((size_t) (sr * 0.05) + 8, 0.f);
        flangerBufR[b].assign((size_t) (sr * 0.05) + 8, 0.f);
        flangerIdx[b] = 0;
        flangerPhase[b] = 0.f;

        chorus[b].prepare(spec);
        phaser[b].prepare(spec);

        filterL[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 5000.f, 0.707f);
        filterR[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 5000.f, 0.707f);
        filterL[b].reset();
        filterR[b].reset();
    }

    masterCompEnv = 0.f;
    masterCompGain = 1.f;
    masterCompGR.store(0.f);

    masterEqL[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 80.f, 0.7f, 1.f);
    masterEqR[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 80.f, 0.7f, 1.f);
    masterEqL[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 450.f, 0.9f, 1.f);
    masterEqR[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 450.f, 0.9f, 1.f);
    masterEqL[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 2500.f, 0.9f, 1.f);
    masterEqR[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 2500.f, 0.9f, 1.f);
    masterEqL[3].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 10000.f, 0.7f, 1.f);
    masterEqR[3].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 10000.f, 0.7f, 1.f);

    for (int k = 0; k < 4; ++k)
    {
        masterEqL[k].reset();
        masterEqR[k].reset();
    }
}

void AuxBusManager::reset()
{
    for (int i = 0; i < 4; ++i)
    {
        chorus[i].reset();
        phaser[i].reset();
        filterL[i].reset();
        filterR[i].reset();
        auxMeters[i].store(0.f);
        auxGateTimer[i] = 0;
        auxGateEnv[i] = 0.f;
        auxShimPhase[i] = 0.f;
        auxPitchPh[i] = 0.f;
        dlyDampL[i] = 0.f;
        dlyDampR[i] = 0.f;
    }
    masterCompEnv = 0.f;
    masterCompGain = 1.f;
    masterCompGR.store(0.f);
}

void AuxBusManager::processAux(int b, float* L, float* R, int n, const AuxBusParams& p)
{
    if (b < 0 || b >= 4 || ! p.enabled)
    {
        if (b >= 0 && b < 4)
            auxMeters[(size_t) b].store(auxMeters[(size_t) b].load() * 0.9f);
        std::fill(L, L + n, 0.f);
        std::fill(R, R + n, 0.f);
        return;
    }

    switch (p.fxType)
    {
        case AUX_FX_REVERB:
        {
            const float fb = 0.72f + juce::jlimit(0.f, 1.f, p.p1) * 0.26f;
            const float d1 = juce::jlimit(0.f, 1.f, p.p2) * 0.42f;
            const float d2 = 1.f - d1;
            const float width = 0.5f + p.p4 * 0.5f;

            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.5f;
                float outL = 0.f, outR = 0.f;
                for (int c = 0; c < 4; ++c)
                {
                    auto& cl = combL[b][c];
                    float oL = cl.buf[cl.idx];
                    cl.store = oL * d2 + cl.store * d1;
                    cl.buf[cl.idx] = in + cl.store * fb;
                    if (++cl.idx >= cl.buf.size()) cl.idx = 0;
                    outL += oL;

                    auto& cr = combR[b][c];
                    float oR = cr.buf[cr.idx];
                    cr.store = oR * d2 + cr.store * d1;
                    cr.buf[cr.idx] = in + cr.store * fb;
                    if (++cr.idx >= cr.buf.size()) cr.idx = 0;
                    outR += oR;
                }
                for (int a = 0; a < 2; ++a)
                {
                    auto& al = apL[b][a];
                    float bL = al.buf[al.idx];
                    float nL = -outL + bL;
                    al.buf[al.idx] = outL + bL * 0.5f;
                    if (++al.idx >= al.buf.size()) al.idx = 0;
                    outL = nL;

                    auto& ar = apR[b][a];
                    float bR = ar.buf[ar.idx];
                    float nR = -outR + bR;
                    ar.buf[ar.idx] = outR + bR * 0.5f;
                    if (++ar.idx >= ar.buf.size()) ar.idx = 0;
                    outR = nR;
                }
                L[i] = (outL * width + outR * (1.f - width)) * 0.4f;
                R[i] = (outR * width + outL * (1.f - width)) * 0.4f;
            }
            break;
        }

        case AUX_FX_DELAY:
        {
            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();

            // P4 >= 0.5f enables BPM SYNC
            const bool sync = (p.p4 >= 0.5f);
            double dL = 0.0, dR = 0.0;
            if (sync)
            {
                const auto* kSyncDivs = getSyncDivisionMultipliers();
                const int divIdx = juce::jlimit(0, 11, (int) std::floor(p.p1 * 11.999f));
                const double beatSamples = (60.0 / currentBpm) * sampleRate;
                dL = juce::jlimit(4.0, (double) len - 16.0, beatSamples * kSyncDivs[divIdx]);
                dR = dL;
            }
            else
            {
                const double ms = 10.0 + (double) p.p1 * 1400.0;
                dL = juce::jlimit(4.0, (double) len - 16.0, ms * 0.001 * sampleRate);
                dR = dL * 0.85; // Natural stereo offset in free mode
            }

            const float fb = juce::jlimit(0.f, 0.92f, p.p2);
            const float dampAlpha = 0.10f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.70f;

            for (int i = 0; i < n; ++i)
            {
                double rpL = (double) dlyIdxL[b] - dL;
                while (rpL < 0.0) rpL += (double) len;
                size_t i0L = (size_t) rpL % len;
                size_t i1L = (i0L + 1) % len;
                float frL = (float) (rpL - std::floor(rpL));
                float rL = bL[i0L] + frL * (bL[i1L] - bL[i0L]);

                double rpR = (double) dlyIdxR[b] - dR;
                while (rpR < 0.0) rpR += (double) len;
                size_t i0R = (size_t) rpR % len;
                size_t i1R = (i0R + 1) % len;
                float frR = (float) (rpR - std::floor(rpR));
                float rR = bR[i0R] + frR * (bR[i1R] - bR[i0R]);

                // Feedback filtering with high-damping
                dlyDampL[b] += (rL - dlyDampL[b]) * dampAlpha;
                dlyDampR[b] += (rR - dlyDampR[b]) * dampAlpha;

                bL[dlyIdxL[b]] = std::tanh(L[i] + dlyDampL[b] * fb);
                bR[dlyIdxR[b]] = std::tanh(R[i] + dlyDampR[b] * fb);
                if (++dlyIdxL[b] >= len) dlyIdxL[b] = 0;
                if (++dlyIdxR[b] >= len) dlyIdxR[b] = 0;

                L[i] = rL;
                R[i] = rR;
            }
            break;
        }

        case AUX_FX_DRIVE:
        {
            const float k = 1.0f + p.p1 * 18.0f;
            const float tone = p.p2;
            for (int i = 0; i < n; ++i)
            {
                float xL = L[i] * k;
                float xR = R[i] * k;
                // Soft saturation curve
                xL = std::tanh(xL);
                xR = std::tanh(xR);
                // Simple tone lowpass smoothing
                L[i] = xL * (0.4f + tone * 0.6f);
                R[i] = xR * (0.4f + tone * 0.6f);
            }
            break;
        }

        case AUX_FX_CHORUS:
        {
            chorus[b].setRate(0.2f + p.p1 * 4.0f);
            chorus[b].setDepth(0.1f + p.p2 * 0.9f);
            chorus[b].setFeedback(p.p3 * 0.6f);
            chorus[b].setMix(0.85f);

            float* chPtrs[2] = { L, R };
            juce::dsp::AudioBlock<float> blk(chPtrs, 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx(blk);
            chorus[b].process(ctx);
            break;
        }

        case AUX_FX_FLANGER:
        case AUX_FX_PHASER:
        {
            phaser[b].setRate(0.1f + p.p1 * 5.0f);
            phaser[b].setDepth(0.2f + p.p2 * 0.8f);
            phaser[b].setFeedback(p.p3 * 0.75f);
            phaser[b].setMix(0.85f);

            float* chPtrs[2] = { L, R };
            juce::dsp::AudioBlock<float> blk(chPtrs, 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx(blk);
            phaser[b].process(ctx);
            break;
        }

        case AUX_FX_FILTER:
        {
            const float cutoff = juce::jlimit(40.f, 18000.f, 80.f + std::pow(p.p1, 2.5f) * 17900.f);
            const float q = juce::jlimit(0.5f, 9.f, 0.7f + p.p2 * 7.5f);
            const int mode = juce::jlimit(0, 2, (int) std::floor(p.p3 * 2.999f));
            if (mode == 0)
            {
                filterL[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff, q);
                filterR[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff, q);
            }
            else if (mode == 1)
            {
                filterL[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, cutoff, q);
                filterR[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, cutoff, q);
            }
            else
            {
                filterL[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff, q);
                filterR[b].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff, q);
            }

            const float drive = 1.0f + p.p4 * 3.5f;
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh(filterL[b].processSample(L[i]) * drive);
                R[i] = std::tanh(filterR[b].processSample(R[i]) * drive);
            }
            break;
        }

        case AUX_FX_SHIMMER:
        {
            // P1: DECAY - scales up to 0.965 for deep, vast, singing ethereal ambient tails
            const float fb = 0.60f + juce::jlimit(0.f, 1.f, p.p1) * 0.365f; // Max 0.965
            const float shimAmt = juce::jlimit(0.f, 1.f, p.p2);
            const float dampAlpha = 0.06f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.50f;
            const float width = 0.5f + p.p4 * 0.5f;

            const float grainSamples = 0.075f * (float) sampleRate;
            const float gRate = -1.0f / grainSamples; // +1 octave

            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();

            for (int i = 0; i < n; ++i)
            {
                auxShimPhase[b] += gRate;
                while (auxShimPhase[b] >= 1.0f) auxShimPhase[b] -= 1.0f;
                while (auxShimPhase[b] < 0.0f)  auxShimPhase[b] += 1.0f;

                const float ph1 = auxShimPhase[b];
                float ph2 = ph1 + 0.5f;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                const float w1 = 0.5f * (1.0f - std::cos(ph1 * juce::MathConstants<float>::twoPi));
                const float w2 = 0.5f * (1.0f - std::cos(ph2 * juce::MathConstants<float>::twoPi));

                // Smooth linear-interpolated reads for +1 octave shift
                double rp1 = (double) dlyIdxL[b] - (double) (ph1 * grainSamples);
                while (rp1 < 0.0) rp1 += (double) len;
                size_t i0 = (size_t) rp1 % len;
                size_t i1 = (i0 + 1) % len;
                float f1 = (float) (rp1 - std::floor(rp1));
                float rL1 = bL[i0] + f1 * (bL[i1] - bL[i0]);
                float rR1 = bR[i0] + f1 * (bR[i1] - bR[i0]);

                double rp2 = (double) dlyIdxL[b] - (double) (ph2 * grainSamples);
                while (rp2 < 0.0) rp2 += (double) len;
                size_t j0 = (size_t) rp2 % len;
                size_t j1 = (j0 + 1) % len;
                float f2 = (float) (rp2 - std::floor(rp2));
                float rL2 = bL[j0] + f2 * (bL[j1] - bL[j0]);
                float rR2 = bR[j0] + f2 * (bR[j1] - bR[j0]);

                const float pitchL = rL1 * w1 + rL2 * w2;
                const float pitchR = rR1 * w2 + rR2 * w1;

                const float in = (L[i] + R[i]) * 0.5f;
                float outL = 0.f, outR = 0.f;
                for (int c = 0; c < 4; ++c)
                {
                    auto& cl = combL[b][c];
                    float oL = cl.buf[cl.idx];
                    // Keep base reverberation ringing while injecting shimmering pitch
                    const float loopInjL = oL * (1.0f - shimAmt * 0.22f) + pitchL * (shimAmt * 0.85f);
                    cl.store += (loopInjL - cl.store) * dampAlpha;
                    if (std::abs(cl.store) < 1e-7f) cl.store = 0.f;
                    cl.buf[cl.idx] = std::tanh(in + cl.store * fb);
                    if (++cl.idx >= cl.buf.size()) cl.idx = 0;
                    outL += oL;

                    auto& cr = combR[b][c];
                    float oR = cr.buf[cr.idx];
                    const float loopInjR = oR * (1.0f - shimAmt * 0.22f) + pitchR * (shimAmt * 0.85f);
                    cr.store += (loopInjR - cr.store) * dampAlpha;
                    if (std::abs(cr.store) < 1e-7f) cr.store = 0.f;
                    cr.buf[cr.idx] = std::tanh(in + cr.store * fb);
                    if (++cr.idx >= cr.buf.size()) cr.idx = 0;
                    outR += oR;
                }

                // Allpass diffusion for lush wash
                for (int a = 0; a < 2; ++a)
                {
                    auto& al = apL[b][a];
                    float oAl = al.buf[al.idx];
                    float bInL = outL - 0.5f * oAl;
                    al.buf[al.idx] = bInL;
                    outL = oAl + 0.5f * bInL;
                    if (++al.idx >= al.buf.size()) al.idx = 0;

                    auto& ar = apR[b][a];
                    float oAr = ar.buf[ar.idx];
                    float bInR = outR - 0.5f * oAr;
                    ar.buf[ar.idx] = bInR;
                    outR = oAr + 0.5f * bInR;
                    if (++ar.idx >= ar.buf.size()) ar.idx = 0;
                }

                // Feed pitch buffer with average comb level (out / 4)
                bL[dlyIdxL[b]] = outL * 0.25f;
                bR[dlyIdxR[b]] = outR * 0.25f;
                if (++dlyIdxL[b] >= len) dlyIdxL[b] = 0;
                if (++dlyIdxR[b] >= len) dlyIdxR[b] = 0;

                L[i] = (outL * width + outR * (1.f - width)) * 0.35f;
                R[i] = (outR * width + outL * (1.f - width)) * 0.35f;
            }
            break;
        }

        case AUX_FX_PINGPONG:
        {
            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();

            // P4 >= 0.5f enables BPM SYNC
            const bool sync = (p.p4 >= 0.5f);
            double d = 0.0;
            if (sync)
            {
                const auto* kSyncDivs = getSyncDivisionMultipliers();
                const int divIdx = juce::jlimit(0, 11, (int) std::floor(p.p1 * 11.999f));
                const double beatSamples = (60.0 / currentBpm) * sampleRate;
                d = juce::jlimit(4.0, (double) len - 16.0, beatSamples * kSyncDivs[divIdx]);
            }
            else
            {
                const double ms = 20.0 + (double) p.p1 * 1200.0;
                d = juce::jlimit(4.0, (double) len - 16.0, ms * 0.001 * sampleRate);
            }

            const float fb = juce::jlimit(0.f, 0.90f, p.p2);
            const float dampAlpha = 0.10f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.70f;

            for (int i = 0; i < n; ++i)
            {
                double rpL = (double) dlyIdxL[b] - d;
                while (rpL < 0.0) rpL += (double) len;
                size_t i0L = (size_t) rpL % len;
                size_t i1L = (i0L + 1) % len;
                float frL = (float) (rpL - std::floor(rpL));
                float rL = bL[i0L] + frL * (bL[i1L] - bL[i0L]);

                double rpR = (double) dlyIdxR[b] - d;
                while (rpR < 0.0) rpR += (double) len;
                size_t i0R = (size_t) rpR % len;
                size_t i1R = (i0R + 1) % len;
                float frR = (float) (rpR - std::floor(rpR));
                float rR = bR[i0R] + frR * (bR[i1R] - bR[i0R]);

                // Ping-pong cross feedback with tone damping
                dlyDampL[b] += (rR - dlyDampL[b]) * dampAlpha;
                dlyDampR[b] += (rL - dlyDampR[b]) * dampAlpha;

                bL[dlyIdxL[b]] = std::tanh(L[i] + dlyDampL[b] * fb);
                bR[dlyIdxR[b]] = std::tanh(R[i] + dlyDampR[b] * fb);
                if (++dlyIdxL[b] >= len) dlyIdxL[b] = 0;
                if (++dlyIdxR[b] >= len) dlyIdxR[b] = 0;

                L[i] = rL;
                R[i] = rR;
            }
            break;
        }

        case AUX_FX_GATED_VERB:
        {
            const float gateSec = 0.04f + std::pow(juce::jlimit(0.f, 1.f, p.p1), 1.6f) * 2.46f;
            const int gateSamples = (int) (gateSec * sampleRate);
            const float fb = 0.82f + juce::jlimit(0.f, 1.f, p.p2) * 0.14f;
            const float dampAlpha = 0.15f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.65f;

            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.5f;
                if (std::abs(in) > 0.035f)
                    auxGateTimer[b] = gateSamples;
                else if (auxGateTimer[b] > 0)
                    --auxGateTimer[b];

                const float gTarg = auxGateTimer[b] > 0 ? 1.0f : 0.0f;
                const float coeff = gTarg > 0.5f ? 0.08f : 0.008f;
                auxGateEnv[b] += (gTarg - auxGateEnv[b]) * coeff;

                float outL = 0.f, outR = 0.f;
                for (int c = 0; c < 4; ++c)
                {
                    auto& cl = combL[b][c];
                    float oL = cl.buf[cl.idx];
                    cl.store += (oL - cl.store) * dampAlpha;
                    if (std::abs(cl.store) < 1e-7f) cl.store = 0.f;
                    cl.buf[cl.idx] = in + cl.store * fb;
                    if (++cl.idx >= cl.buf.size()) cl.idx = 0;
                    outL += oL;

                    auto& cr = combR[b][c];
                    float oR = cr.buf[cr.idx];
                    cr.store += (oR - cr.store) * dampAlpha;
                    if (std::abs(cr.store) < 1e-7f) cr.store = 0.f;
                    cr.buf[cr.idx] = in + cr.store * fb;
                    if (++cr.idx >= cr.buf.size()) cr.idx = 0;
                    outR += oR;
                }

                // Diffusion stages for dense, thick gated reverb sound
                for (int a = 0; a < 2; ++a)
                {
                    auto& al = apL[b][a];
                    float oAl = al.buf[al.idx];
                    float bInL = outL - 0.5f * oAl;
                    al.buf[al.idx] = bInL;
                    outL = oAl + 0.5f * bInL;
                    if (++al.idx >= al.buf.size()) al.idx = 0;

                    auto& ar = apR[b][a];
                    float oAr = ar.buf[ar.idx];
                    float bInR = outR - 0.5f * oAr;
                    ar.buf[ar.idx] = bInR;
                    outR = oAr + 0.5f * bInR;
                    if (++ar.idx >= ar.buf.size()) ar.idx = 0;
                }

                L[i] = std::tanh(outL * 0.42f) * auxGateEnv[b];
                R[i] = std::tanh(outR * 0.42f) * auxGateEnv[b];
            }
            break;
        }

        case AUX_FX_TUBE:
        {
            const float k = 1.0f + p.p1 * 25.0f;
            const float bias = p.p2 * 0.6f;
            for (int i = 0; i < n; ++i)
            {
                float xL = L[i] * k;
                float xR = R[i] * k;
                L[i] = (xL + bias * xL * std::abs(xL)) / (1.0f + std::abs(xL));
                R[i] = (xR + bias * xR * std::abs(xR)) / (1.0f + std::abs(xR));
            }
            break;
        }

        case AUX_FX_PITCH:
        {
            // Full-range chromatic Pitch Shifter (-12 to +12 semitones + fine cents)
            const float semitones = -12.0f + juce::jlimit(0.f, 1.f, p.p1) * 24.0f;
            const float fineCents = (juce::jlimit(0.f, 1.f, p.p2) - 0.5f) * 100.0f;
            const float totalSemitones = semitones + fineCents * 0.01f;
            const float ratio = std::pow(2.0f, totalSemitones / 12.0f);
            const float fb = juce::jlimit(0.f, 0.85f, p.p3 * 0.85f);
            const float mix = juce::jlimit(0.f, 1.0f, p.p4);

            const float grainSamples = 0.075f * (float) sampleRate;
            const float gRate = (1.0f - ratio) / grainSamples;

            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();

            for (int i = 0; i < n; ++i)
            {
                auxPitchPh[b] += gRate;
                while (auxPitchPh[b] >= 1.0f) auxPitchPh[b] -= 1.0f;
                while (auxPitchPh[b] < 0.0f)  auxPitchPh[b] += 1.0f;

                const float ph1 = auxPitchPh[b];
                float ph2 = ph1 + 0.5f;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                const float w1 = 0.5f * (1.0f - std::cos(ph1 * juce::MathConstants<float>::twoPi));
                const float w2 = 0.5f * (1.0f - std::cos(ph2 * juce::MathConstants<float>::twoPi));

                // Linear-interpolated reads for pristine pitch transposition
                double rp1 = (double) dlyIdxL[b] - (double) (ph1 * grainSamples);
                while (rp1 < 0.0) rp1 += (double) len;
                size_t i0 = (size_t) rp1 % len;
                size_t i1 = (i0 + 1) % len;
                float f1 = (float) (rp1 - std::floor(rp1));
                float rL1 = bL[i0] + f1 * (bL[i1] - bL[i0]);
                float rR1 = bR[i0] + f1 * (bR[i1] - bR[i0]);

                double rp2 = (double) dlyIdxL[b] - (double) (ph2 * grainSamples);
                while (rp2 < 0.0) rp2 += (double) len;
                size_t j0 = (size_t) rp2 % len;
                size_t j1 = (j0 + 1) % len;
                float f2 = (float) (rp2 - std::floor(rp2));
                float rL2 = bL[j0] + f2 * (bL[j1] - bL[j0]);
                float rR2 = bR[j0] + f2 * (bR[j1] - bR[j0]);

                const float shiftL = rL1 * w1 + rL2 * w2;
                const float shiftR = rR1 * w2 + rR2 * w1;

                const float dryL = L[i];
                const float dryR = R[i];

                // Write into buffer with feedback spiral
                bL[dlyIdxL[b]] = std::tanh(dryL + shiftL * fb);
                bR[dlyIdxR[b]] = std::tanh(dryR + shiftR * fb);
                if (++dlyIdxL[b] >= len) dlyIdxL[b] = 0;
                if (++dlyIdxR[b] >= len) dlyIdxR[b] = 0;

                L[i] = dryL + (shiftL - dryL) * mix;
                R[i] = dryR + (shiftR - dryR) * mix;
            }
            break;
        }

        case AUX_FX_SPRING:
        {
            const float tension = 0.40f + juce::jlimit(0.f, 1.f, p.p1) * 0.54f; // Max 0.94 for multi-second dub spring
            const float boing = 0.20f + juce::jlimit(0.f, 1.f, p.p2) * 0.65f;
            const float toneDamp = 0.15f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.55f;
            const float mix = juce::jlimit(0.f, 1.f, p.p4);

            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.5f;

                // Allpass dispersion stages create the physical spring "boing" / chirp
                float chirpL = in;
                float chirpR = in;
                const float apC = 0.45f * boing;
                for (int a = 0; a < 2; ++a)
                {
                    auto& al = apL[b][a];
                    float oAl = al.buf[al.idx];
                    float bufInL = chirpL - apC * oAl;
                    al.buf[al.idx] = bufInL;
                    chirpL = oAl + apC * bufInL;
                    if (++al.idx >= al.buf.size()) al.idx = 0;

                    auto& ar = apR[b][a];
                    float oAr = ar.buf[ar.idx];
                    float bufInR = chirpR - apC * oAr;
                    ar.buf[ar.idx] = bufInR;
                    chirpR = oAr + apC * bufInR;
                    if (++ar.idx >= ar.buf.size()) ar.idx = 0;
                }

                // Dual resonant spring tank lines with warm soft saturation
                float tankL = 0.f, tankR = 0.f;
                for (int c = 0; c < 2; ++c)
                {
                    auto& cl = combL[b][c];
                    float oL = cl.buf[cl.idx];
                    cl.store += (oL - cl.store) * toneDamp;
                    if (std::abs(cl.store) < 1e-7f) cl.store = 0.f;
                    cl.buf[cl.idx] = std::tanh(chirpL + cl.store * tension);
                    if (++cl.idx >= cl.buf.size()) cl.idx = 0;
                    tankL += oL;

                    auto& cr = combR[b][c];
                    float oR = cr.buf[cr.idx];
                    cr.store += (oR - cr.store) * toneDamp;
                    if (std::abs(cr.store) < 1e-7f) cr.store = 0.f;
                    cr.buf[cr.idx] = std::tanh(chirpR + cr.store * tension);
                    if (++cr.idx >= cr.buf.size()) cr.idx = 0;
                    tankR += oR;
                }

                float dryL = L[i], dryR = R[i];
                float wetL = tankL * 0.45f;
                float wetR = tankR * 0.45f;
                L[i] = dryL + (wetL - dryL) * mix;
                R[i] = dryR + (wetR - dryR) * mix;
            }
            break;
        }

        default:
            break;
    }

    // Apply return level & return pan
    const float retLvl = juce::jlimit(0.f, 2.f, p.returnLevel);
    const float pan = juce::jlimit(-1.f, 1.f, p.returnPan);
    const float panL = std::cos((pan + 1.f) * 0.25f * juce::MathConstants<float>::pi);
    const float panR = std::sin((pan + 1.f) * 0.25f * juce::MathConstants<float>::pi);

    float retPeak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        L[i] = L[i] * retLvl * panL;
        R[i] = R[i] * retLvl * panR;
        const float aL = std::abs(L[i]);
        const float aR = std::abs(R[i]);
        if (aL > retPeak) retPeak = aL;
        if (aR > retPeak) retPeak = aR;
    }
    const float prev = auxMeters[(size_t) b].load();
    auxMeters[(size_t) b].store(juce::jmax(retPeak, prev * 0.91f));
}

void AuxBusManager::processMasterChain(float* L, float* R, int n, const MasterFXParams& p)
{
    // 1. Master Bus Compressor (SSL-style VCA glue)
    if (p.compOn)
    {
        const float threshDb = p.compThresh;
        const float ratio = juce::jmax(1.f, p.compRatio);
        const float atkTime = juce::jmax(0.0001f, p.compAtk * 0.001f);
        const float relTime = juce::jmax(0.01f, p.compRel * 0.001f);
        const float atkCoef = 1.f - std::exp(-1.f / (atkTime * (float) sampleRate));
        const float relCoef = 1.f - std::exp(-1.f / (relTime * (float) sampleRate));
        const float makeupLin = std::pow(10.f, p.compMakeup / 20.f);

        float maxGRDb = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max(std::abs(L[i]), std::abs(R[i]));
            const float pkDb = pk > 1e-5f ? 20.f * std::log10(pk) : -100.f;

            float targetGRDb = 0.f;
            if (pkDb > threshDb)
                targetGRDb = (pkDb - threshDb) * (1.f - 1.f / ratio);

            if (targetGRDb > masterCompEnv)
                masterCompEnv += (targetGRDb - masterCompEnv) * atkCoef;
            else
                masterCompEnv += (targetGRDb - masterCompEnv) * relCoef;

            if (masterCompEnv > maxGRDb)
                maxGRDb = masterCompEnv;

            const float curGain = std::pow(10.f, -masterCompEnv / 20.f) * makeupLin;
            L[i] *= curGain;
            R[i] *= curGain;
        }
        masterCompGR.store(maxGRDb);
    }
    else
    {
        masterCompGR.store(0.f);
    }

    // 2. Master 4-Band EQ
    if (p.eqOn)
    {
        const float lg = std::pow(10.f, p.eqLowGain / 20.f);
        const float lmg = std::pow(10.f, p.eqLowMidGain / 20.f);
        const float hmg = std::pow(10.f, p.eqHiMidGain / 20.f);
        const float hg = std::pow(10.f, p.eqHighGain / 20.f);

        masterEqL[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 80.f, 0.707f, lg);
        masterEqR[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 80.f, 0.707f, lg);
        masterEqL[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 450.f, 0.9f, lmg);
        masterEqR[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 450.f, 0.9f, lmg);
        masterEqL[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2500.f, 0.9f, hmg);
        masterEqR[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2500.f, 0.9f, hmg);
        masterEqL[3].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 10000.f, 0.707f, hg);
        masterEqR[3].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 10000.f, 0.707f, hg);

        for (int i = 0; i < n; ++i)
        {
            float sL = L[i], sR = R[i];
            for (int k = 0; k < 4; ++k)
            {
                sL = masterEqL[k].processSample(sL);
                sR = masterEqR[k].processSample(sR);
            }
            L[i] = sL;
            R[i] = sR;
        }
    }

    // 3. Master Tape Drive & Ceiling Limiter
    if (p.driveOn && p.drive > 0.001f)
    {
        const float k = 1.0f + p.drive * 2.2f;
        for (int i = 0; i < n; ++i)
        {
            L[i] = std::tanh(L[i] * k) / k;
            R[i] = std::tanh(R[i] * k) / k;
        }
    }

    if (p.limiterOn)
    {
        const float ceilLin = std::pow(10.f, p.ceiling / 20.f);
        const float thresh = ceilLin * 0.707f; // completely linear up to -3 dBFS
        const float margin = ceilLin - thresh;
        for (int i = 0; i < n; ++i)
        {
            auto softLimit = [thresh, margin, ceilLin](float x) -> float
            {
                const float ax = std::abs(x);
                if (ax <= thresh)
                    return x;
                const float excess = ax - thresh;
                const float sat = thresh + margin * std::tanh(excess / margin);
                return (x > 0.f ? std::min(sat, ceilLin) : -std::min(sat, ceilLin));
            };
            L[i] = softLimit(L[i]);
            R[i] = softLimit(R[i]);
        }
    }
}

} // namespace f64
