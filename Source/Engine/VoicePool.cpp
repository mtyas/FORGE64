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
            if (v.active && v.pad == pad && v.note == e.note && v.chan == e.chan)
            {
                if (v.srcType == SRC_SAMPLE)
                {
                    v.inRelease = true;
                    v.stage = 3;
                }
                else if (v.sustain)
                {
                    killVoice(v, sr, 30.f);
                }
            }
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

    t->sample = smp;
    t->srcType = pp.srcType;
    if (pp.srcType == SRC_SAMPLE && ! smp)
    {
        // Fallback drum synth based on pad index
        static const int defaultTypes[16] = {
            SRC_KICK, SRC_SNARE, SRC_HAT_CLOSED, SRC_HAT_OPEN,
            SRC_CLAP, SRC_TOM, SRC_TOM, SRC_TOM,
            SRC_CRASH, SRC_RIDE, SRC_RIM, SRC_BELL,
            SRC_CONGA, SRC_CONGA, SRC_BELL, SRC_KICK
        };
        t->srcType = defaultTypes[pad % 16];
    }
    t->synth.reset();
    t->active = true;
    t->pad = pad;
    t->choke = pp.choke;
    t->note = e.note;
    t->chan = e.chan;

    if (smp != nullptr && smp->buffer.getNumSamples() > 0)
    {
        const double totalFrames = (double) smp->buffer.getNumSamples();
        const float s0 = juce::jlimit(0.f, 0.999f, rt.sampleStart.load());
        const float s1 = juce::jlimit(s0 + 0.001f, 1.f, rt.sampleEnd.load());
        const float l0 = juce::jlimit(0.f, 0.999f, rt.loopStart.load());
        const float l1 = juce::jlimit(l0 + 0.001f, 1.f, rt.loopEnd.load());
        t->startPos  = s0 * totalFrames;
        t->endPos    = s1 * totalFrames;
        t->loopStart = l0 * totalFrames;
        t->loopEnd   = l1 * totalFrames;
        t->isLooping = rt.loopOn.load();
        t->isReverse = rt.reverseOn.load();
        t->pos = t->isReverse ? (t->endPos - 1.0) : t->startPos;
    }
    else
    {
        t->startPos = 0.0;
        t->endPos = 0.0;
        t->loopStart = 0.0;
        t->loopEnd = 0.0;
        t->pos = 0.0;
        t->isLooping = false;
        t->isReverse = false;
    }

    t->baseRate = smp ? smp->sampleRate / sr : 1.0;
    t->vel = juce::jmax(0.002f, e.vel);
    t->env = 0.f;
    t->stage = 0;
    t->inAttack = true;
    t->inHold = false;
    t->inRelease = false;
    t->holdTimer = 0.f;
    t->ageSec = 0.f;
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

    v.ageSec += (float) n / (float) sr;

    ModMatrix::VoiceMods vm;
    if (mod != nullptr)
        mod->renderVoice(v.voiceId, vm, n);

    const double pitchSt = (double) pp.tune + (double) vm.pitch
        + (pp.mode == 1 ? (double) (v.note - pp.mnote) : 0.0);
    const double rate = v.baseRate * std::pow(2.0, clampRange(pitchSt, -60.0, 60.0) / 12.0);

    const float panLim = clampRange(pp.pan + vm.pan, -1.f, 1.f);
    const float gainL = std::cos((panLim + 1.f) * 0.25f * juce::MathConstants<float>::pi);
    const float gainR = std::sin((panLim + 1.f) * 0.25f * juce::MathConstants<float>::pi);

    const float atkSec = juce::jmax(0.0005f, pp.smplAtk * 2.0f);
    const float decSec = juce::jmax(0.005f,  pp.smplDec * 4.0f);
    const float susLvl = clampRange(pp.smplSus, 0.0f, 1.0f);
    const float relSec = juce::jmax(0.005f,  pp.smplRel * 4.0f);

    const float atkCoef = 1.f - std::exp(-1.f / (atkSec * (float) sr));
    const float decCoef = 1.f - std::exp(-1.f / (decSec * (float) sr));
    const float relCoef = std::exp(-1.f / (relSec * (float) sr));
    const float rawDecCoef = std::exp(-1.f / (juce::jmax(0.005f, pp.decay) * (float) sr));
    const float drumAtkCoef = 1.f - std::exp(-1.f / (0.0015f * (float) sr));
    const float ampMod = juce::jmax(0.f, 1.f + vm.amp);
    const bool isLuaSynth = (v.srcType == SRC_LUA || (grid != nullptr && grid->runtime(v.pad).scriptOn.load()));

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
        if (v.srcType == SRC_SAMPLE)
        {
            if (v.inRelease || v.stage == 3)
            {
                v.env *= relCoef;
                if (v.env < 1e-5f)
                    ended = true;
            }
            else if (v.stage == 0) // Attack
            {
                v.env += (v.vel - v.env) * atkCoef;
                if (v.env >= v.vel * 0.995f)
                {
                    v.env = v.vel;
                    v.stage = 1;
                }
            }
            else if (v.stage == 1) // Decay
            {
                const float target = v.vel * susLvl;
                v.env += (target - v.env) * decCoef;
                if (std::abs(v.env - target) < 0.002f)
                {
                    v.env = target;
                    v.stage = 2;
                }
            }
            else if (v.stage == 2) // Sustain
            {
                v.env = v.vel * susLvl;
                if (! v.sustain && ! v.isLooping && susLvl < 0.001f)
                    ended = true;
            }
        }
        else
        {
            if (v.inAttack)
            {
                v.env += (v.vel - v.env) * drumAtkCoef;
                if (v.env >= v.vel * 0.995f)
                {
                    v.env = v.vel;
                    v.inAttack = false;
                    v.inHold = true;
                    v.holdTimer = 0.f;
                }
            }
            else if (v.inHold)
            {
                v.holdTimer += (float) (1.0 / sr);
                const float holdDur = juce::jmax(0.006f, pp.decay * 0.03f);
                if (v.holdTimer >= holdDur)
                {
                    v.inHold = false;
                    v.inRelease = true;
                }
            }
            else if (! (v.sustain && ! v.choked))
            {
                const float relSec = isLuaSynth ? juce::jmax(0.04f, pp.decay * 0.65f)
                                                : juce::jmax(0.005f, pp.decay);
                const float curDecCoef = std::exp(-1.f / (relSec * (float) sr));
                v.env *= curDecCoef;
            }
        }

        if (v.choked)
            v.fade = juce::jmax(0.f, v.fade - v.fadeStep);

        float a = 0.f, b = 0.f;
        if (v.srcType == SRC_SAMPLE && sL != nullptr && frames > 1)
        {
            const double effectiveStart = juce::jmax(0.0, v.startPos);
            const double effectiveEnd   = juce::jmin((double) frames, juce::jmax(effectiveStart + 2.0, v.endPos));
            const double lStart = juce::jmax(effectiveStart, v.loopStart);
            const double lEnd   = juce::jmin(effectiveEnd, juce::jmax(lStart + 2.0, v.loopEnd));

            const size_t i0 = (size_t) juce::jlimit(0.0, (double) frames - 2.0, v.pos);
            const float f = (float) (v.pos - (double) i0);
            a = sL[i0] + f * (sL[i0 + 1] - sL[i0]);
            b = sR[i0] + f * (sR[i0 + 1] - sR[i0]);

            if (v.isReverse)
            {
                v.pos -= rate;
                if (v.isLooping && v.pos <= lStart)
                {
                    v.pos = lEnd - 1.0;
                }
                else if (v.pos <= effectiveStart)
                {
                    ended = true;
                }
            }
            else
            {
                v.pos += rate;
                if (v.isLooping && v.pos >= lEnd)
                {
                    v.pos = lStart;
                }
                else if (v.pos >= effectiveEnd)
                {
                    if (v.sustain)
                        v.pos = lStart;
                    else
                        ended = true;
                }
            }
        }
        else
        {
            // Procedural drum synthesis
            float sig = 0.f;
            const float tuneVal = (float) pitchSt;
            const float decVal = pp.decay;
            switch (v.srcType)
            {
                case SRC_KICK:
                    sig = DrumSynth::renderKick(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2, pp.fx3, pp.drive);
                    break;
                case SRC_SNARE:
                    sig = DrumSynth::renderSnare(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2, pp.fx3);
                    break;
                case SRC_HAT_CLOSED:
                    sig = DrumSynth::renderHiHat(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2, false);
                    break;
                case SRC_HAT_OPEN:
                    sig = DrumSynth::renderHiHat(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2, true);
                    break;
                case SRC_CLAP:
                    sig = DrumSynth::renderClap(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_TOM:
                    sig = DrumSynth::renderTom(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_CRASH:
                    sig = DrumSynth::renderCrash(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_RIDE:
                    sig = DrumSynth::renderRide(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_RIM:
                    sig = DrumSynth::renderRim(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_BELL:
                    sig = DrumSynth::renderBell(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                case SRC_CONGA:
                    sig = DrumSynth::renderConga(v.synth, sr, tuneVal, decVal, pp.fx1, pp.fx2);
                    break;
                default:
                    sig = DrumSynth::renderTom(v.synth, sr, tuneVal, decVal, 0.4f, 0.4f);
                    break;
            }
            if (v.srcType == SRC_LUA || (grid != nullptr && grid->runtime(v.pad).scriptOn.load()))
            {
                // When Lua DSP generates the audio, avoid mixing DrumSynth
                a = 0.f;
                b = 0.f;
            }
            else
            {
                a = sig;
                b = sig;
            }
            if (v.synth.ampEnv < 1e-4)
                ended = true;
        }

        const float amp = v.env * v.fade * ampMod;
        L[i] += a * amp * gainL;
        R[i] += b * amp * gainR;
    }

    if (v.fade <= 0.f)
        deactivate(v);
    else if (! v.inAttack && ! (v.sustain && ! v.choked))
    {
        if (isLuaSynth)
        {
            const float minLuaLife = 0.08f + pp.decay * 1.5f;
            if (v.env < 1e-4f && v.ageSec >= minLuaLife)
                deactivate(v);
        }
        else if (v.env < 1e-5f || (ended && v.env < 1e-4f) || (v.srcType != SRC_SAMPLE && ended))
        {
            deactivate(v);
        }
    }
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
