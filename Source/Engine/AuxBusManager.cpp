#include "AuxBusManager.h"
#include "Saturation.h"
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

    lastEqLowGain = lastEqLowMidGain = lastEqHiMidGain = lastEqHighGain = -999.f;
    masterCompEnv = 0.f;
    masterCompGain = 1.f;
    masterCompGR.store(0.f);

    masterEqL[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 80.f, 0.7f, 1.f);
    masterEqR[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 80.f, 0.7f, 1.f);
    masterEqL[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jlimit(20.f, (float)sampleRate * .45f, masterParams.eqFrequency[1]), juce::jlimit(.1f, 12.f, masterParams.eqQ[1]), 1.f);
    masterEqR[1].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jlimit(20.f, (float)sampleRate * .45f, masterParams.eqFrequency[1]), juce::jlimit(.1f, 12.f, masterParams.eqQ[1]), 1.f);
    masterEqL[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jlimit(20.f, (float)sampleRate * .45f, masterParams.eqFrequency[2]), juce::jlimit(.1f, 12.f, masterParams.eqQ[2]), 1.f);
    masterEqR[2].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jlimit(20.f, (float)sampleRate * .45f, masterParams.eqFrequency[2]), juce::jlimit(.1f, 12.f, masterParams.eqQ[2]), 1.f);
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
        auxModPhase[i] = 0.f;
        dlyDampL[i] = 0.f;
        dlyDampR[i] = 0.f;
        std::fill(dlyBufL[i].begin(), dlyBufL[i].end(), 0.f);
        std::fill(dlyBufR[i].begin(), dlyBufR[i].end(), 0.f);
        dlyIdxL[i] = dlyIdxR[i] = 0;
    }
    masterCompEnv = 0.f;
    masterCompGain = 1.f;
    masterCompGR.store(0.f);
    masterLimiterEnv = 0.f;
    masterTapeMakeup = 1.f;
    auxTapeMakeup.fill(1.f);
}

// 4-point Hermite cubic interpolation for silky-smooth pitch transposition without aliasing
inline float interpolateHermite4(const std::vector<float>& buf, double pos)
{
    const size_t len = buf.size();
    while (pos < 0.0) pos += (double) len;
    const size_t i1 = (size_t) pos % len;
    const size_t i0 = (i1 + len - 1) % len;
    const size_t i2 = (i1 + 1) % len;
    const size_t i3 = (i1 + 2) % len;
    const float f = (float) (pos - std::floor(pos));

    const float y0 = buf[i0];
    const float y1 = buf[i1];
    const float y2 = buf[i2];
    const float y3 = buf[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
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
        case AUX_FX_PINGPONG:
        {
            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();
            const int mode = p.fxType == AUX_FX_PINGPONG ? 1 : juce::jlimit(0, 6, p.delayMode);
            const double ratios[] = { 4. / 3., 1., 1., 1., 2., 1.5, 2. / 3. };
            const double base = p.p4 >= .5f
                ? 60. / currentBpm * sampleRate * getSyncDivisionMultipliers()[juce::jlimit(0, 11, (int)std::floor(p.p1 * 11.999f))]
                : (10. + p.p1 * 1400.) * .001 * sampleRate;
            const double dL = juce::jlimit(4., (double)len - 16., base);
            const double dR = juce::jlimit(4., (double)len - 16., base * ratios[mode]);
            const float fb = juce::jlimit(0.f, .92f, p.p2);
            const float alpha = .10f + (1.f - juce::jlimit(0.f, 1.f, p.p3)) * .70f;
            auto read = [len](const std::vector<float>& buf, size_t index, double delay)
            {
                double pos = (double)index - delay;
                while (pos < 0.) pos += len;
                size_t k = (size_t)pos % len;
                return buf[k] + (float)(pos - std::floor(pos)) * (buf[(k + 1) % len] - buf[k]);
            };
            for (int i = 0; i < n; ++i)
            {
                const float l = read(bL, dlyIdxL[b], dL), r = read(bR, dlyIdxR[b], dR);
                float outL = l, outR = r;
                if (mode == 2)
                {
                    outL = (l + read(bR, dlyIdxR[b], dL * .5) + read(bL, dlyIdxL[b], dL * .75)) / 3.f;
                    outR = (r + read(bL, dlyIdxL[b], dR * .25) + read(bR, dlyIdxR[b], dR * .625)) / 3.f;
                }
                dlyDampL[b] += ((mode == 1 ? r : l) - dlyDampL[b]) * alpha;
                dlyDampR[b] += ((mode == 1 ? l : r) - dlyDampR[b]) * alpha;
                const float inputL = mode == 1 ? .5f * (L[i] + R[i]) : L[i];
                const float inputR = mode == 1 ? 0.f : R[i];
                bL[dlyIdxL[b]] = std::tanh(inputL + dlyDampL[b] * fb);
                bR[dlyIdxR[b]] = std::tanh(inputR + dlyDampR[b] * fb);
                dlyIdxL[b] = (dlyIdxL[b] + 1) % len;
                dlyIdxR[b] = (dlyIdxR[b] + 1) % len;
                L[i] = outL;
                R[i] = outR;
            }
            break;
        }

        case AUX_FX_DRIVE:
        {
            saturateStereo(L, R, n, p.p1, 64.f, auxTapeMakeup[(size_t) b]);
            const float tone = p.p2;
            for (int i = 0; i < n; ++i)
            {
                float xL = L[i];
                float xR = R[i];
                // Soft saturation curve
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
            if (std::abs(cutoff - lastFilterCutoff[b]) > 1.f ||
                std::abs(q - lastFilterQ[b]) > 0.05f ||
                mode != lastFilterMode[b])
            {
                lastFilterCutoff[b] = cutoff;
                lastFilterQ[b] = q;
                lastFilterMode[b] = mode;
                if (mode == 0)
                {
                    *filterL[b].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate, cutoff, q);
                    *filterR[b].coefficients = *filterL[b].coefficients;
                }
                else if (mode == 1)
                {
                    *filterL[b].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(sampleRate, cutoff, q);
                    *filterR[b].coefficients = *filterL[b].coefficients;
                }
                else
                {
                    *filterL[b].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, cutoff, q);
                    *filterR[b].coefficients = *filterL[b].coefficients;
                }
            }

            const float drive = 1.0f + p.p4 * 3.5f;
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh(filterL[b].processSample(L[i]) * drive);
                R[i] = std::tanh(filterR[b].processSample(R[i]) * drive);
            }
            break;
        }

        case AUX_FX_PLATE:
        {
            // Studio Plate Reverb: Dense, metallic, lush plate simulation
            // P1: Decay Time
            // P2: Plate Size
            // P3: High Damping (HF absorption of plate sheet)
            // P4: Diffusion
            const float fb = 0.50f + juce::jlimit(0.f, 1.f, p.p1) * 0.44f; // Max 0.94
            const float damp = 0.10f + (1.0f - juce::jlimit(0.f, 1.f, p.p3)) * 0.55f;
            const float diffCoeff = 0.45f + juce::jlimit(0.f, 1.f, p.p4) * 0.25f;

            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.5f;

                // 2-stage input allpass diffusers
                float diffL = in, diffR = in;
                for (int a = 0; a < 2; ++a)
                {
                    auto& al = apL[b][a];
                    float oAl = al.buf[al.idx];
                    float bInL = diffL - diffCoeff * oAl;
                    al.buf[al.idx] = bInL;
                    diffL = oAl + diffCoeff * bInL;
                    if (++al.idx >= al.buf.size()) al.idx = 0;

                    auto& ar = apR[b][a];
                    float oAr = ar.buf[ar.idx];
                    float bInR = diffR - diffCoeff * oAr;
                    ar.buf[ar.idx] = bInR;
                    diffR = oAr + diffCoeff * bInR;
                    if (++ar.idx >= ar.buf.size()) ar.idx = 0;
                }

                // 4-tank parallel FDN with cross-coupling
                float outL = 0.f, outR = 0.f;
                for (int c = 0; c < 4; ++c)
                {
                    auto& cl = combL[b][c];
                    float oL = cl.buf[cl.idx];
                    cl.store += (oL - cl.store) * damp;
                    cl.buf[cl.idx] = std::tanh(diffL * 0.5f + (oL - cl.store * 0.5f) * fb);
                    if (++cl.idx >= cl.buf.size()) cl.idx = 0;
                    outL += oL;

                    auto& cr = combR[b][c];
                    float oR = cr.buf[cr.idx];
                    cr.store += (oR - cr.store) * damp;
                    cr.buf[cr.idx] = std::tanh(diffR * 0.5f + (oR - cr.store * 0.5f) * fb);
                    if (++cr.idx >= cr.buf.size()) cr.idx = 0;
                    outR += oR;
                }

                L[i] = (outL * 0.8f + outR * 0.2f) * 0.35f;
                R[i] = (outR * 0.8f + outL * 0.2f) * 0.35f;
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
            // Studio-grade 4-Phase Quadrature Pitch Shifter
            // P1: Transpose (-12 to +12 semitones)
            // P2: Fine Tune (-50 to +50 cents)
            // P3: Feedback Spiral (0 to 80%) with high-damp smoothing
            // P4: Dry/Wet Mix (0 to 100%)
            const float semitones = -12.0f + juce::jlimit(0.f, 1.f, p.p1) * 24.0f;
            const float fineCents = (juce::jlimit(0.f, 1.f, p.p2) - 0.5f) * 100.0f;
            const float totalSemitones = semitones + fineCents * 0.01f;
            const float ratio = std::pow(2.0f, totalSemitones / 12.0f);
            const float fb = juce::jlimit(0.f, 0.80f, p.p3 * 0.80f);
            const float mix = juce::jlimit(0.f, 1.0f, p.p4);

            // Optimal grain window for percussion & musical clarity: ~38ms
            const float grainSamples = 0.038f * (float) sampleRate;
            const float gRate = (1.0f - ratio) / grainSamples;

            auto& bL = dlyBufL[b];
            auto& bR = dlyBufR[b];
            const size_t len = bL.size();

            for (int i = 0; i < n; ++i)
            {
                auxPitchPh[b] += gRate;
                while (auxPitchPh[b] >= 1.0f) auxPitchPh[b] -= 1.0f;
                while (auxPitchPh[b] < 0.0f)  auxPitchPh[b] += 1.0f;

                const float ph0 = auxPitchPh[b];
                float shiftL = 0.f, shiftR = 0.f;

                // 4 Quadrature Taps spaced at 90 degrees (0.0, 0.25, 0.5, 0.75)
                // Constant power sum eliminates tremolo and amplitude dips
                for (int tap = 0; tap < 4; ++tap)
                {
                    float tapPhL = ph0 + tap * 0.25f;
                    while (tapPhL >= 1.0f) tapPhL -= 1.0f;

                    // Slight stereo phase offset (45 deg) for wide 3D stereo image
                    float tapPhR = tapPhL + 0.125f;
                    while (tapPhR >= 1.0f) tapPhR -= 1.0f;

                    const float wL = 0.5f * (1.0f - std::cos(tapPhL * juce::MathConstants<float>::twoPi));
                    const float wR = 0.5f * (1.0f - std::cos(tapPhR * juce::MathConstants<float>::twoPi));

                    const double rpL = (double) dlyIdxL[b] - (double) (tapPhL * grainSamples);
                    const double rpR = (double) dlyIdxR[b] - (double) (tapPhR * grainSamples);

                    shiftL += interpolateHermite4(bL, rpL) * wL;
                    shiftR += interpolateHermite4(bR, rpR) * wR;
                }
                shiftL *= 0.5f;
                shiftR *= 0.5f;

                const float dryL = L[i];
                const float dryR = R[i];

                // Smooth feedback damping with high-frequency rolloff to prevent harsh buildup
                dlyDampL[b] += (shiftL - dlyDampL[b]) * 0.40f;
                dlyDampR[b] += (shiftR - dlyDampR[b]) * 0.40f;

                // Write into delay buffer with soft saturation limiter
                bL[dlyIdxL[b]] = std::tanh(dryL + dlyDampL[b] * fb);
                bR[dlyIdxR[b]] = std::tanh(dryR + dlyDampR[b] * fb);

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
    auto compress = [&]
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

    };
    auto equalize = [&]
    {
    // 2. Master 4-Band EQ
    if (p.eqOn)
    {
        if (std::abs(p.eqLowGain - lastEqLowGain) > 0.05f ||
            std::abs(p.eqLowMidGain - lastEqLowMidGain) > 0.05f ||
            std::abs(p.eqHiMidGain - lastEqHiMidGain) > 0.05f ||
            std::abs(p.eqHighGain - lastEqHighGain) > 0.05f || p.eqFrequency != lastEqFrequency || p.eqQ != lastEqQ || p.eqShape != lastEqShape)
        {
            lastEqShape = p.eqShape;
            lastEqFrequency = p.eqFrequency;
            lastEqQ = p.eqQ;
            lastEqLowGain = p.eqLowGain;
            lastEqLowMidGain = p.eqLowMidGain;
            lastEqHiMidGain = p.eqHiMidGain;
            lastEqHighGain = p.eqHighGain;

            const float lg = std::pow(10.f, p.eqLowGain / 20.f);
            const float lmg = std::pow(10.f, p.eqLowMidGain / 20.f);
            const float hmg = std::pow(10.f, p.eqHiMidGain / 20.f);
            const float hg = std::pow(10.f, p.eqHighGain / 20.f);

            const float gains[] = {lg, lmg, hmg, hg};
            for (int band = 0; band < 4; ++band)
            {
                const float frequency = juce::jlimit(20.f, (float)sampleRate * .45f, p.eqFrequency[(size_t)band]);
                const float q = juce::jlimit(.1f, 12.f, p.eqQ[(size_t)band]);
                auto coefficients = p.eqShape[(size_t)band] == 1
                    ? juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sampleRate, frequency, q, gains[band])
                    : p.eqShape[(size_t)band] == 2
                    ? juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, frequency, q, gains[band])
                    : juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, frequency, q, gains[band]);
                *masterEqL[band].coefficients = coefficients;
                *masterEqR[band].coefficients = coefficients;
            }
        }

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

    };
    if (p.eqPreComp) { equalize(); compress(); }
    else { compress(); equalize(); }

    // 3. Master Tape Drive & Ceiling Limiter
    if (p.driveOn && p.drive > 0.001f)
    {
        saturateStereo(L, R, n, p.drive, 256.f, masterTapeMakeup, p.driveColour);
    }

    if (p.limiterOn)
    {
        const float ceilLin = std::pow(10.f, p.ceiling / 20.f);
        const float atkCoef = 1.f - std::exp(-1.f / (0.0008f * (float) sampleRate)); // Fast 0.8ms attack
        const float relCoef = 1.f - std::exp(-1.f / (0.060f * (float) sampleRate));   // Musical 60ms release

        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max(std::abs(L[i]), std::abs(R[i]));
            if (pk > masterLimiterEnv)
                masterLimiterEnv += (pk - masterLimiterEnv) * atkCoef;
            else
                masterLimiterEnv += (pk - masterLimiterEnv) * relCoef;

            float gain = 1.f;
            if (masterLimiterEnv > ceilLin)
                gain = ceilLin / masterLimiterEnv;

            L[i] *= gain;
            R[i] *= gain;

            // Safety soft-curve at ceiling to prevent digital overshoot
            auto softCeil = [ceilLin](float x) -> float
            {
                const float ax = std::abs(x);
                if (ax <= ceilLin * 0.96f)
                    return x;
                const float s = std::tanh(ax / ceilLin) * ceilLin;
                return x > 0.f ? std::min(s, ceilLin) : -std::min(s, ceilLin);
            };
            L[i] = softCeil(L[i]);
            R[i] = softCeil(R[i]);
        }
    }
}

juce::ValueTree AuxBusManager::serialize() const
{
    juce::ValueTree root("AUX_MASTER_FX");

    for (int i = 0; i < 4; ++i)
    {
        juce::ValueTree ab("AUX_BUS");
        ab.setProperty("index", i, nullptr);
        ab.setProperty("delayMode", auxParams[i].delayMode, nullptr);
        ab.setProperty("fxType", auxParams[i].fxType, nullptr);
        ab.setProperty("p1", auxParams[i].p1, nullptr);
        ab.setProperty("p2", auxParams[i].p2, nullptr);
        ab.setProperty("p3", auxParams[i].p3, nullptr);
        ab.setProperty("p4", auxParams[i].p4, nullptr);
        ab.setProperty("returnLevel", auxParams[i].returnLevel, nullptr);
        ab.setProperty("returnPan", auxParams[i].returnPan, nullptr);
        ab.setProperty("enabled", auxParams[i].enabled, nullptr);
        root.appendChild(ab, nullptr);
    }

    juce::ValueTree mp("MASTER_PARAMS");
    mp.setProperty("eqPreComp", masterParams.eqPreComp, nullptr);
    mp.setProperty("compOn", masterParams.compOn, nullptr);
    mp.setProperty("compThresh", masterParams.compThresh, nullptr);
    mp.setProperty("compRatio", masterParams.compRatio, nullptr);
    mp.setProperty("compAtk", masterParams.compAtk, nullptr);
    mp.setProperty("compRel", masterParams.compRel, nullptr);
    mp.setProperty("compMakeup", masterParams.compMakeup, nullptr);

    for (int band = 0; band < 4; ++band)
    {
        mp.setProperty("eqShape" + juce::String(band), masterParams.eqShape[(size_t)band], nullptr);
        mp.setProperty("eqFreq" + juce::String(band), masterParams.eqFrequency[(size_t)band], nullptr);
        mp.setProperty("eqQ" + juce::String(band), masterParams.eqQ[(size_t)band], nullptr);
    }
    mp.setProperty("eqOn", masterParams.eqOn, nullptr);
    mp.setProperty("eqLowGain", masterParams.eqLowGain, nullptr);
    mp.setProperty("eqLowMidGain", masterParams.eqLowMidGain, nullptr);
    mp.setProperty("eqHiMidGain", masterParams.eqHiMidGain, nullptr);
    mp.setProperty("eqHighGain", masterParams.eqHighGain, nullptr);

    mp.setProperty("driveOn", masterParams.driveOn, nullptr);
    mp.setProperty("driveColour", masterParams.driveColour, nullptr);
    mp.setProperty("drive", masterParams.drive, nullptr);
    mp.setProperty("limiterOn", masterParams.limiterOn, nullptr);
    mp.setProperty("ceiling", masterParams.ceiling, nullptr);

    root.appendChild(mp, nullptr);
    return root;
}

void AuxBusManager::deserialize(const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (child.hasType("AUX_BUS"))
        {
            const int idx = (int) child.getProperty("index", -1);
            if (idx >= 0 && idx < 4)
            {
                auxParams[idx].delayMode = juce::jlimit(0, 6, (int)child.getProperty("delayMode", 0));
                auxParams[idx].fxType = (int) child.getProperty("fxType", auxParams[idx].fxType);
                auxParams[idx].p1 = (float) child.getProperty("p1", auxParams[idx].p1);
                auxParams[idx].p2 = (float) child.getProperty("p2", auxParams[idx].p2);
                auxParams[idx].p3 = (float) child.getProperty("p3", auxParams[idx].p3);
                auxParams[idx].p4 = (float) child.getProperty("p4", auxParams[idx].p4);
                auxParams[idx].returnLevel = (float) child.getProperty("returnLevel", auxParams[idx].returnLevel);
                auxParams[idx].returnPan = (float) child.getProperty("returnPan", auxParams[idx].returnPan);
                auxParams[idx].enabled = (bool) child.getProperty("enabled", auxParams[idx].enabled);
            }
        }
        else if (child.hasType("MASTER_PARAMS"))
        {
            masterParams.eqPreComp = (bool) child.getProperty("eqPreComp", false);
            masterParams.compOn = (bool) child.getProperty("compOn", masterParams.compOn);
            masterParams.compThresh = (float) child.getProperty("compThresh", masterParams.compThresh);
            masterParams.compRatio = (float) child.getProperty("compRatio", masterParams.compRatio);
            masterParams.compAtk = (float) child.getProperty("compAtk", masterParams.compAtk);
            masterParams.compRel = (float) child.getProperty("compRel", masterParams.compRel);
            masterParams.compMakeup = (float) child.getProperty("compMakeup", masterParams.compMakeup);

            const MasterFXParams defaults;
            for (int band = 0; band < 4; ++band)
            {
                masterParams.eqShape[(size_t)band] = juce::jlimit(0, 2, (int)child.getProperty("eqShape" + juce::String(band), band == 0 ? 1 : band == 3 ? 2 : 0));
                masterParams.eqFrequency[(size_t)band] = juce::jlimit(20.f, 20000.f, (float)child.getProperty("eqFreq" + juce::String(band), defaults.eqFrequency[(size_t)band]));
                masterParams.eqQ[(size_t)band] = juce::jlimit(.1f, 12.f, (float)child.getProperty("eqQ" + juce::String(band), defaults.eqQ[(size_t)band]));
            }
            masterParams.eqOn = (bool) child.getProperty("eqOn", masterParams.eqOn);
            masterParams.eqLowGain = (float) child.getProperty("eqLowGain", masterParams.eqLowGain);
            masterParams.eqLowMidGain = (float) child.getProperty("eqLowMidGain", masterParams.eqLowMidGain);
            masterParams.eqHiMidGain = (float) child.getProperty("eqHiMidGain", masterParams.eqHiMidGain);
            masterParams.eqHighGain = (float) child.getProperty("eqHighGain", masterParams.eqHighGain);

            masterParams.driveOn = (bool) child.getProperty("driveOn", masterParams.driveOn);
            masterParams.driveColour = juce::jlimit(0, 3, (int)child.getProperty("driveColour", 0));
            masterParams.drive = (float) child.getProperty("drive", masterParams.drive);
            masterParams.limiterOn = (bool) child.getProperty("limiterOn", masterParams.limiterOn);
            masterParams.ceiling = (float) child.getProperty("ceiling", masterParams.ceiling);
        }
    }
}

} // namespace f64
