#include "ModSources.h"

namespace f64 {

const double kSyncDivBeats[kNumSyncDivs] = {
    8.0, 4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25,
    1.0 / 6.0, 0.125, 1.0 / 12.0, 1.0 / 16.0, 1.0 / 32.0
};

const char* kSyncDivNames[kNumSyncDivs] = {
    "8/1", "4/1", "3/1", "2/1", "3/2", "1/1", "3/4", "1/2", "1/3", "1/4",
    "1/6", "1/8", "1/12", "1/16", "1/32"
};

namespace {
    juce::StringArray divisionNames()
    {
        juce::StringArray a;
        for (int i = 0; i < kNumSyncDivs; ++i)
            a.add(kSyncDivNames[i]);
        return a;
    }
}

// ---------------------------------------------------------------------------
// LFO
// ---------------------------------------------------------------------------
juce::ValueTree LFOSource::makeDefault()
{
    juce::ValueTree t("LFO");
    t.setProperty("enabled", false, nullptr);
    t.setProperty("shape", 0, nullptr);
    t.setProperty("rate", 2.0, nullptr);
    t.setProperty("sync", false, nullptr);
    t.setProperty("div", 8, nullptr);
    t.setProperty("uni", false, nullptr);
    t.setProperty("glide", 0.0, nullptr);
    t.setProperty("phase", 0.0, nullptr);
    return t;
}

void LFOSource::syncFromState()
{
    if (! state.isValid()) return;
    enabled  = bool(state.getProperty("enabled", false));
    shape    = (int) state.getProperty("shape", 0);
    rate     = (float) (double) state.getProperty("rate", 2.0);
    sync     = bool(state.getProperty("sync", false));
    div      = (int) state.getProperty("div", 8);
    uni      = bool(state.getProperty("uni", false));
    glide    = (float) (double) state.getProperty("glide", 0.0);
    phaseOff = (float) (double) state.getProperty("phase", 0.0);
}

void LFOSource::render(float* out, int n)
{
    if (! enabled.load())
    {
        std::fill_n(out, n, 0.f);
        return;
    }

    const int   sh = juce::limit(shape.load(), 0, 5);
    const bool  un = uni.load();
    const float gl = glide.load();
    const float po = phaseOff.load();
    const double hz = rateHz(rate.load(), sync.load(), div.load());
    const double dph = hz / sampleRate;

    for (int i = 0; i < n; ++i)
    {
        phase += dph;
        const int step = (int) std::floor(phase);
        if (phase >= 1.0)
            phase -= std::floor(phase);

        float v;
        if (sh >= 4)
        {
            if (step != prevStep)
            {
                prevStep = step;
                held = un ? rnd.nextFloat() : rnd.nextFloat() * 2.f - 1.f;
            }
            if (sh == 5)
            {
                const float tau = juce::jmax(0.002f, gl * 0.2f);
                const float coef = 1.f - std::exp(-1.f / (tau * (float) sampleRate));
                glideMem += (held - glideMem) * coef;
                v = glideMem;
            }
            else
            {
                v = held;
            }
        }
        else
        {
            float ph = (float) phase + po;
            ph -= std::floor(ph);
            switch (sh)
            {
                case 0:  v = std::sin(ph * juce::MathConstants<float>::twoPi); break;
                case 1:  v = ph < 0.5f ? (4.f * ph - 1.f) : (3.f - 4.f * ph); break;
                case 2:  v = 2.f * ph - 1.f; break;
                default: v = ph < 0.5f ? 1.f : -1.f; break;
            }
            if (un)
                v = v * 0.5f + 0.5f;
        }
        out[i] = v;
    }
}

// ---------------------------------------------------------------------------
// Random / Chaos
// ---------------------------------------------------------------------------
juce::ValueTree RandomSource::makeDefault()
{
    juce::ValueTree t("RND");
    t.setProperty("enabled", false, nullptr);
    t.setProperty("kind", 0, nullptr);
    t.setProperty("rate", 4.0, nullptr);
    t.setProperty("sync", true, nullptr);
    t.setProperty("div", 7, nullptr);
    t.setProperty("p1", 0.5, nullptr);
    t.setProperty("uni", false, nullptr);
    return t;
}

void RandomSource::syncFromState()
{
    if (! state.isValid()) return;
    enabled = bool(state.getProperty("enabled", false));
    kind    = (int) state.getProperty("kind", 0);
    rate    = (float) (double) state.getProperty("rate", 4.0);
    sync    = bool(state.getProperty("sync", true));
    div     = (int) state.getProperty("div", 7);
    p1      = (float) (double) state.getProperty("p1", 0.5);
    uni     = bool(state.getProperty("uni", false));
}

void RandomSource::render(float* out, int n)
{
    if (! enabled.load())
    {
        std::fill_n(out, n, 0.f);
        return;
    }

    const int   k = juce::limit(kind.load(), 0, 4);
    const bool  un = uni.load();
    const float a = juce::jlimit(0.001f, 1.f, p1.load());
    auto randBi = [this] { return rnd.nextFloat() * 2.f - 1.f; };
    auto toOut  = [&](float v) { return un ? v * 0.5f + 0.5f : v; };

    if (k == 4) // Lorenz attractor
    {
        const double hz = rateHz(rate.load(), sync.load(), div.load());
        const double dt = 0.0015 * juce::jlimit(0.05, 8.0, hz / 4.0);
        for (int i = 0; i < n; ++i)
        {
            const double dx = 10.0 * (ly - lx);
            const double dy = lx * (28.0 - lz) - ly;
            const double dz = lx * ly - (8.0 / 3.0) * lz;
            lx += dx * dt; ly += dy * dt; lz += dz * dt;
            if (! std::isfinite(lx) || ! std::isfinite(ly) || ! std::isfinite(lz))
            { lx = 0.1; ly = 0.0; lz = 0.0; }
            out[i] = toOut(juce::limitRange((float) (lx / 20.0), -1.f, 1.f));
        }
        return;
    }

    const double hz = rateHz(rate.load(), sync.load(), div.load());
    const double dph = hz / sampleRate;

    for (int i = 0; i < n; ++i)
    {
        phase += dph;
        const int step = (int) std::floor(phase);
        const float ph = (float) (phase - std::floor(phase));

        if (step != prevStep)
        {
            prevStep = step;
            switch (k)
            {
                case 0: held = randBi(); break;                                        // S&H
                case 1: prevHeld = held; held = randBi(); break;                       // smooth
                case 2: held = juce::limitRange(held + randBi() * a, -1.f, 1.f); break; // drunk walk
                case 3: if (rnd.nextFloat() < a) held = randBi(); break;               // probabilistic
                default: break;
            }
        }

        float v = held;
        if (k == 1)
        {
            const float s = ph * ph * (3.f - 2.f * ph); // smoothstep interpolation
            v = prevHeld + (held - prevHeld) * s;
        }
        out[i] = toOut(v);
    }
}

// ---------------------------------------------------------------------------
// Envelope (DAHDSR)
// ---------------------------------------------------------------------------
juce::ValueTree EnvSource::makeDefault()
{
    juce::ValueTree t("ENV");
    t.setProperty("enabled", false, nullptr);
    t.setProperty("dly", 0.0, nullptr);
    t.setProperty("atk", 0.01, nullptr);
    t.setProperty("hold", 0.0, nullptr);
    t.setProperty("dec", 0.3, nullptr);
    t.setProperty("sus", 0.5, nullptr);
    t.setProperty("rel", 0.1, nullptr);
    t.setProperty("curve", 0.0, nullptr);
    t.setProperty("loop", false, nullptr);
    return t;
}

void EnvSource::syncFromState()
{
    if (! state.isValid()) return;
    enabled = bool(state.getProperty("enabled", false));
    dly   = (float) (double) state.getProperty("dly", 0.0);
    atk   = (float) (double) state.getProperty("atk", 0.01);
    hold  = (float) (double) state.getProperty("hold", 0.0);
    dec   = (float) (double) state.getProperty("dec", 0.3);
    sus   = (float) (double) state.getProperty("sus", 0.5);
    rel   = (float) (double) state.getProperty("rel", 0.1);
    curve = (float) (double) state.getProperty("curve", 0.0);
    loop  = bool(state.getProperty("loop", false));
}

void EnvSource::advance(Instance& in, float* out, int n)
{
    const float sDly  = juce::jmax(0.f, dly.load());
    const float sAtk  = juce::jmax(0.0005f, atk.load());
    const float sHold = juce::jmax(0.f, hold.load());
    const float sDec  = juce::jmax(0.0005f, dec.load());
    const float sSus  = juce::jlimit(0.f, 1.f, sus.load());
    const float c     = juce::jlimit(-1.f, 1.f, curve.load());
    const bool  lp    = loop.load();
    const float ea = std::exp(c * 2.f);
    const float ed = std::exp(-c * 2.f);
    const float sr = (float) sampleRate;

    for (int i = 0; i < n; ++i)
    {
        if (in.trigReq.exchange(false))
        {
            in.stage = 0;
            in.t = 0.f;
            in.level = 0.f;
        }

        if (in.stage >= 0)
        {
            in.t += 1.f / sr;
            switch (in.stage)
            {
                case 0:
                    in.level = 0.f;
                    if (in.t >= sDly) { in.stage = 1; in.t = 0.f; }
                    break;
                case 1:
                {
                    const float p = juce::jlimit(0.f, 1.f, in.t / sAtk);
                    in.level = std::pow(p, ea);
                    if (p >= 1.f) { in.stage = 2; in.t = 0.f; }
                    break;
                }
                case 2:
                    in.level = 1.f;
                    if (in.t >= sHold) { in.stage = 3; in.t = 0.f; }
                    break;
                case 3:
                {
                    const float p = juce::jlimit(0.f, 1.f, in.t / sDec);
                    in.level = sSus + (1.f - sSus) * std::pow(1.f - p, ed);
                    if (p >= 1.f)
                    {
                        if (lp) { in.stage = 0; in.t = 0.f; }
                        else    in.stage = 4;
                    }
                    break;
                }
                default:
                    in.level = sSus;
                    break;
            }
        }
        else
        {
            in.level = 0.f;
        }

        in.last = in.level;
        if (out != nullptr)
            out[i] = in.level;
    }
}

void EnvSource::render(float* out, int n)
{
    advance(globalInst, out, n);
    if (! enabled.load())
        std::fill_n(out, n, 0.f);
}

void EnvSource::renderInstance(int i, int n)
{
    advance(inst[(size_t) juce::limit(i, 0, kEnvInstances - 1)], nullptr, n);
}

// ---------------------------------------------------------------------------
// Step sequencer
// ---------------------------------------------------------------------------
juce::ValueTree SeqSource::makeDefault()
{
    juce::ValueTree t("SEQ");
    t.setProperty("enabled", false, nullptr);
    t.setProperty("numSteps", 16, nullptr);
    t.setProperty("rate", 4.0, nullptr);
    t.setProperty("sync", true, nullptr);
    t.setProperty("div", 9, nullptr);
    t.setProperty("gate", 0.8, nullptr);
    t.setProperty("slew", 0.1, nullptr);
    t.setProperty("swing", 0.0, nullptr);
    t.setProperty("dir", 0, nullptr);
    t.setProperty("uni", true, nullptr);
    t.setProperty("steps", "0.9 0 0.3 0 0.7 0 0.3 0.2 0.9 0 0.3 0 0.7 0.1 0.3 0.45 "
                           "0.9 0 0.3 0 0.7 0 0.3 0.2 0.9 0 0.3 0 0.7 0.1 0.3 0.45", nullptr);
    return t;
}

void SeqSource::syncFromState()
{
    if (! state.isValid()) return;
    enabled  = bool(state.getProperty("enabled", false));
    numSteps = (int) state.getProperty("numSteps", 16);
    rate     = (float) (double) state.getProperty("rate", 4.0);
    sync     = bool(state.getProperty("sync", true));
    div      = (int) state.getProperty("div", 9);
    gate     = (float) (double) state.getProperty("gate", 0.8);
    slew     = (float) (double) state.getProperty("slew", 0.1);
    swing    = (float) (double) state.getProperty("swing", 0.0);
    dir      = (int) state.getProperty("dir", 0);
    uni      = bool(state.getProperty("uni", true));

    juce::StringArray toks;
    toks.addTokens(state.getProperty("steps", "").toString(), " ", "");
    for (int i = 0; i < 32; ++i)
        steps[(size_t) i] = i < toks.size() ? juce::jlimit(0.f, 1.f, toks[i].getFloatValue()) : 0.f;
}

void SeqSource::setStep(int i, float v)
{
    if (i < 0 || i >= 32) return;
    steps[(size_t) i] = juce::jlimit(0.f, 1.f, v);
    persistSteps();
}

void SeqSource::persistSteps()
{
    if (! state.isValid()) return;
    juce::String s;
    for (int i = 0; i < 32; ++i)
        s += juce::String(steps[(size_t) i].load(), 3) + " ";
    state.setProperty("steps", s.trimEnd(), nullptr);
}

void SeqSource::render(float* out, int n)
{
    if (restartReq.exchange(false))
    {
        t = 0.0; stepStart = 0.0; stepLen = -1.0;
        curStep = 0; pingDir = 1; mem = 0.f;
        cur = steps[0].load();
    }

    if (! enabled.load())
    {
        std::fill_n(out, n, 0.f);
        return;
    }

    const int    N = juce::limit(numSteps.load(), 4, 32);
    const double hz = rateHz(rate.load(), sync.load(), div.load());
    const double stepDur = 1.0 / hz;
    const float  g  = juce::jlimit(0.05f, 1.f, gate.load());
    const float  sl = juce::jlimit(0.f, 1.f, slew.load());
    const float  sw = juce::jlimit(0.f, 0.6f, swing.load());
    const bool   un = uni.load();
    const int    d  = juce::limit(dir.load(), 0, 3);
    const float  coef = sl < 0.001f ? 1.f
        : 1.f - std::exp(-1.f / (juce::jmax(0.0005f, sl * 0.05f) * (float) sampleRate));
    const float inv = 1.f / (float) sampleRate;

    for (int i = 0; i < n; ++i)
    {
        t += inv;
        if (stepLen < 0.0)
        {
            stepLen = stepDur;
            cur = steps[(size_t) juce::limit(curStep, 0, 31)].load();
        }
        while (t >= stepStart + stepLen)
        {
            stepStart += stepLen;
            switch (d)
            {
                case 1: curStep = (curStep - 1 + N) % N; break;
                case 2:
                    curStep += pingDir;
                    if (curStep >= N - 1) { curStep = N - 1; pingDir = -1; }
                    else if (curStep <= 0) { curStep = 0; pingDir = 1; }
                    break;
                case 3: curStep = rnd.nextInt(N); break;
                default: curStep = (curStep + 1) % N; break;
            }
            cur = steps[(size_t) juce::limit(curStep, 0, 31)].load();
            stepLen = stepDur * ((curStep & 1) ? (1.0 + sw) : (1.0 - sw));
            if (stepLen <= 1e-9)
                stepLen = stepDur;
        }

        const float local = (float) ((t - stepStart) / stepLen);
        const float target = (local >= 0.f && local < g) ? cur : 0.f;
        mem += (target - mem) * coef;
        out[i] = un ? mem : mem * 2.f - 1.f;
    }
}

juce::StringArray syncDivisionNames()
{
    return divisionNames();
}

} // namespace f64
