#pragma once
#include <JuceHeader.h>
#include <vector>

namespace f64 {

// Global aux FX buses: A = reverb (Freeverb-style Schroeder network),
// B = tempo-friendly stereo delay. Both operate fully wet on the aux buffers;
// their returns are summed into the main bus by the processor.
class GlobalFX
{
public:
    void prepare(double sr, int /*maxBlock*/)
    {
        sampleRate = sr;
        const double scale = sr / 44100.0;
        static const int combT[4] = { 1116, 1188, 1277, 1356 };
        static const int apT[2]   = { 556, 441 };

        for (int c = 0; c < 4; ++c)
        {
            combL[c].buf.assign((size_t) std::max(16.0, combT[c] * scale) + 4, 0.f);
            combR[c].buf.assign((size_t) std::max(16.0, (combT[c] + 23) * scale) + 4, 0.f);
            combL[c].idx = combR[c].idx = 0;
            combL[c].store = combR[c].store = 0.f;
        }
        for (int a = 0; a < 2; ++a)
        {
            apL[a].buf.assign((size_t) std::max(16.0, apT[a] * scale) + 4, 0.f);
            apR[a].buf.assign((size_t) std::max(16.0, (apT[a] + 23) * scale) + 4, 0.f);
            apL[a].idx = apR[a].idx = 0;
        }

        dlyL.assign((size_t) (sr * 2.0) + 8, 0.f);
        dlyR.assign((size_t) (sr * 2.0) + 8, 0.f);
        dlyIdxL = dlyIdxR = 0;
    }

    void processReverb(float* L, float* R, int n, float size, float damp)
    {
        const float fb = 0.7f + juce::jlimit(0.f, 1.f, size) * 0.28f;
        const float d1 = juce::jlimit(0.f, 1.f, damp) * 0.4f;
        const float d2 = 1.f - d1;

        for (int i = 0; i < n; ++i)
        {
            const float in = (L[i] + R[i]) * 0.5f;
            float outL = 0.f, outR = 0.f;
            for (int c = 0; c < 4; ++c)
            {
                outL += combStep(combL[c], in, fb, d1, d2);
                outR += combStep(combR[c], in, fb, d1, d2);
            }
            for (int a = 0; a < 2; ++a)
            {
                outL = apStep(apL[a], outL);
                outR = apStep(apR[a], outR);
            }
            L[i] = outL * 0.35f;
            R[i] = outR * 0.35f;
        }
    }

    void processDelay(float* L, float* R, int n, float timeMs, float fb)
    {
        const double d = juce::jlimit(1.0, sampleRate * 1.9, (double) timeMs * 0.001 * sampleRate);
        const float f = juce::jlimit(0.f, 0.92f, fb);
        const size_t len = dlyL.size();

        for (int i = 0; i < n; ++i)
        {
            L[i] = delayStep(dlyL, dlyIdxL, L[i], d, f, len);
            R[i] = delayStep(dlyR, dlyIdxR, R[i], d, f, len);
        }
    }

private:
    struct Comb { std::vector<float> buf; size_t idx = 0; float store = 0.f; };
    struct AP   { std::vector<float> buf; size_t idx = 0; };

    static float combStep(Comb& c, float in, float fb, float d1, float d2)
    {
        const float out = c.buf[c.idx];
        c.store = out * d2 + c.store * d1;
        c.buf[c.idx] = in + c.store * fb;
        if (++c.idx >= c.buf.size())
            c.idx = 0;
        return out;
    }

    static float apStep(AP& a, float in)
    {
        const float bufOut = a.buf[a.idx];
        const float out = -in + bufOut;
        a.buf[a.idx] = in + bufOut * 0.5f;
        if (++a.idx >= a.buf.size())
            a.idx = 0;
        return out;
    }

    static float delayStep(std::vector<float>& buf, size_t& idx, float in,
                           double delay, float fb, size_t len)
    {
        double rp = (double) idx - delay;
        while (rp < 0.0)
            rp += (double) len;
        const size_t i0 = (size_t) rp % len;
        const size_t i1 = (i0 + 1) % len;
        const float fr = (float) (rp - std::floor(rp));
        const float read = buf[i0] + fr * (buf[i1] - buf[i0]);
        buf[idx] = in + read * fb;
        if (++idx >= len)
            idx = 0;
        return read;
    }

    Comb combL[4], combR[4];
    AP apL[2], apR[2];
    std::vector<float> dlyL, dlyR;
    size_t dlyIdxL = 0, dlyIdxR = 0;
    double sampleRate = 48000.0;
};

} // namespace f64
