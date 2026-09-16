#include "VoicePool.h"
#include <cmath>

namespace f64 {

void VoicePool::prepare(double sr, int /*maxBlock*/)
{
    sampleRate = sr;
    for (int i = 0; i < kMaxVoices; ++i)
        voices[i].voiceId = i;
}

int VoicePool::activeCount() const
{
    int c = 0;
    for (const auto& v : voices)
        c += v.active ? 1 : 0;
    return c;
}

void VoicePool::allNotesOff()
{
    for (auto& v : voices)
        if (v.active)
            killVoice(v, sampleRate, 10.f);
}

void VoicePool::killVoice(Voice& v, double sr, float fadeMs)
{
    v.choked = true;
    v.sustain = false;
    v.fadeStep = (float) (1.0 / (juce::jmax(0.5, (double) fadeMs) * 0.001 * sr));
}

void VoicePool::deactivate(Voice& v)
{
    const int pad = v.pad;
    v.active = false;
    v.sample = nullptr;
    v.pad = -1;
    if (pad >= 0)
    {
        bool any = false;
        for (const auto& o : voices)
            if (o.active && o.pad == pad)
            {
                any = true;
                break;
            }
        if (! any)
            activeMask &= ~(1ull << pad);
    }
}

void VoicePool::applyEvent(const VoiceEvent& e, double sr, const PadParams& pp, int pad)
{
    if (e.isOff)
    {
        for (auto& v : voices)
            if (v.active && v.pad == pad && v.sustain && v.note == e.note && v.chan == e.chan)
                killVoice(v, sr, 30.f);
        return;
    }

    // Choke groups: triggering any pad in a group kills every voice in it (all banks).
    if (pp.choke > 0)
        for (auto& v : voices)
            if (v.active && v.choke == pp.choke)
                killVoice(v, sr, 5.f);

    auto& rt = grid->runtime(pad);
    SampleManager::Ptr smp;
    {
        const juce::ScopedLock sl(rt.lock);
        smp = rt.sample;
    }
    if (! smp && (pp.srcType == 0 || ! rt.scriptOn.load()))
        return; // nothing to play

    Voice* t = nullptr;
    int oldestIdx = -1, oldestSame = -1, sameCount = 0;
    uint64_t oldestTick = UINT64_MAX, oldestSameTick = UINT64_MAX;

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& v = voices[i];
        if (! v.active)
        {
            if (t == nullptr)
                t = &v;
            continue;
        }
        if (v.tick < oldestTick)
        {
            oldestTick = v.tick;
            oldestIdx = i;
        }
        if (v.pad == pad)
        {
            ++sameCount;
            if (v.tick < oldestSameTick)
            {
                oldestSameTick = v.tick;
                oldestSame = i;
            }
        }
    }

    if (sameCount >= 8 && oldestSame >= 0)
        t = &voices[oldestSame];        // per-pad polyphony cap: steal from same pad
    else if (t == nullptr && oldestIdx >= 0)
        t = &voices[oldestIdx];         // global steal-oldest
    if (t == nullptr)
        return;

    t->sample = nullptr;
    t->active = true;
    t->pad = pad;
    t->choke = pp.choke;
    t->note = e.note;
    t->chan = e.chan;
    t->sample = smp;
    t->pos = 0.0;
    t->baseRate = smp ? smp->sampleRate / sr : 1.0;
    t->vel = juce::jmax(0.002f, e.vel);
    t->env = 0.f;
    t->inAttack = true;
    t->fade = 1.f;
    t->fadeStep = 0.f;
    t->choked = false;
    t->sustain = (pp.mode == 1) && smp != nullptr;
    t->tick = tickCounter++;
    activeMask |= (1ull << pad);

    if (mod != nullptr)
        mod->triggerVoice(t->voiceId);

    rt.lastHitStamp = clock;
    rt.lastVel = e.vel;
}

void VoicePool::renderSegment(Voice& v, float* L, float* R, int from, int to,
                              double sr, const PadParams& pp)
{
    const int n = to - from;
    if (n <= 0)
        return;

    ModMatrix::VoiceMods vm;
    if (mod != nullptr)
        mod->renderVoice(v.voiceId, vm, n);

    const double pitchSt = (double) pp.tune
        + (double) vm.pitch * 24.0
        + (pp.mode == 1 ? (double) (v.note - pp.mnote) : 0.0);
    const double rate = v.baseRate * std::pow(2.0, juce::limitRange(pitchSt, -60.0, 60.0) / 12.0);

    const float panLim = juce::limitRange(pp.pan + vm.pan, -1.f, 1.f);
    const float gainL = std::cos((panLim + 1.f) * 0.25f * juce::MathConstants<float>::pi);
    const float gainR = std::sin((panLim + 1.f) * 0.25f * juce::MathConstants<float>::pi);

    const float atkCoef = 1.f - std::exp(-1.f / (0.0015f * (float) sr));
    const float decCoef = std::exp(-1.f / (juce::jmax(0.005f, pp.decay) * (float) sr));
    const float ampMod = juce::jmax(0.f, 1.f + vm.amp);

    const float* sL = nullptr;
    const float* sR = nullptr;
    size_t frames = 0;
    if (v.sample)
    {
        sL = v.sample->buffer.getReadPointer(0);
        sR = v.sample->buffer.getNumChannels() > 1 ? v.sample->buffer.getReadPointer(1) : sL;
        frames = (size_t) v.sample->buffer.getNumSamples();
    }

    bool ended = false;
    for (int i = from; i < to; ++i)
    {
        if (v.inAttack)
        {
            v.env += (v.vel - v.env) * atkCoef;
            if (v.env >= v.vel * 0.995f)
            {
                v.env = v.vel;
                v.inAttack = false;
            }
        }
        else if (! (v.sustain && ! v.choked))
        {
            v.env *= decCoef;
        }

        if (v.choked)
            v.fade = juce::jmax(0.f, v.fade - v.fadeStep);

        float a = 0.f, b = 0.f;
        if (sL != nullptr && frames > 1)
        {
            const size_t i0 = (size_t) v.pos;
            if (i0 + 1 < frames)
            {
                const float f = (float) (v.pos - (double) i0);
                a = sL[i0] + f * (sL[i0 + 1] - sL[i0]);
                b = sR[i0] + f * (sR[i0 + 1] - sR[i0]);
            }
            else if (v.sustain)
            {
                v.pos = 0.0; // loop while key held (chromatic mode)
            }
            else
            {
                ended = true;
            }
            v.pos += rate;
        }

        const float amp = v.env * v.fade * ampMod;
        L[i] += a * amp * gainL;
        R[i] += b * amp * gainR;
    }

    if (v.fade <= 0.f)
        deactivate(v);
    else if (! v.inAttack && ! (v.sustain && ! v.choked) && (v.env < 1e-5f || (ended && v.env < 1e-4f)))
        deactivate(v);
}

bool VoicePool::renderPad(int pad, float* L, float* R, int n, double sr,
                          const PadParams& pp, const std::vector<TimedEvent>& events)
{
    bool any = false;
    int cursor = 0;
    size_t ei = 0;

    while (cursor < n)
    {
        while (ei < events.size() && events[ei].pos <= cursor)
            applyEvent(events[ei++].ev, sr, pp, pad);

        int next = n;
        if (ei < events.size())
            next = juce::jmax(cursor + 1, juce::jmin(n, events[ei].pos));

        if (padActive(pad))
        {
            for (auto& v : voices)
                if (v.active && v.pad == pad)
                {
                    renderSegment(v, L, R, cursor, next, sr, pp);
                    any = true;
                }
        }
        cursor = next;
    }

    while (ei < events.size())
        applyEvent(events[ei++].ev, sr, pp, pad);

    return any;
}

} // namespace f64
