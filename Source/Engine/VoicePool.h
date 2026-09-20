#pragma once
#include <JuceHeader.h>
#include "PadDefs.h"
#include "PadGrid.h"
#include "SampleManager.h"
#include "../Modulation/ModMatrix.h"
#include <vector>

#include "DrumSynth.h"

namespace f64 {

// Polyphonic sample-playback and synthesized drum voices with choke groups,
// per-voice modulation, one-shot and chromatic (sustained/looping) behaviour.
class VoicePool
{
public:
    struct VoiceEvent { int pad = 0; float vel = 1.f; int note = 60; int chan = 1; bool isOff = false; };
    struct TimedEvent { int pos = 0; VoiceEvent ev; };

    void prepare(double sr, int maxBlock);
    void setDeps(PadGrid* g, ModMatrix* m) { grid = g; mod = m; }
    void setHitClock(int64_t c) { clock = c; }

    // Renders all voices of one pad for a full block, honouring sample-accurate
    // event positions (triggers/note-offs happen between sub-segments).
    bool renderPad(int pad, float* L, float* R, int n, double sr, const PadParams& pp,
                   const std::vector<TimedEvent>& events);

    bool padActive(int pad) const { return (activeMask & (1ull << pad)) != 0; }
    void allNotesOff();
    int  activeCount() const;

private:
    struct Voice
    {
        bool active = false;
        int voiceId = 0, pad = -1, choke = 0, note = 60, chan = 1;
        int srcType = 0;
        DrumSynth::VoiceState synth;
        SampleManager::Ptr sample;
        double pos = 0.0, baseRate = 1.0;
        double startPos = 0.0, endPos = 0.0, loopStart = 0.0, loopEnd = 0.0;
        bool isReverse = false, isLooping = false;
        float vel = 0.f, env = 0.f, fade = 1.f, fadeStep = 0.f;
        float holdTimer = 0.f, ageSec = 0.f;
        int stage = 0;
        bool inAttack = true, inHold = false, inRelease = false, choked = false, sustain = false;
        uint64_t tick = 0;
    };

    void applyEvent(const VoiceEvent& e, double sr, const PadParams& pp, int pad);
    void killVoice(Voice& v, double sr, float fadeMs);
    void renderSegment(Voice& v, float* L, float* R, int from, int to, double sr, const PadParams& pp);
    void deactivate(Voice& v);

    Voice voices[kMaxVoices];
    uint64_t activeMask = 0;
    uint64_t tickCounter = 1;
    int64_t clock = 0;
    PadGrid* grid = nullptr;
    ModMatrix* mod = nullptr;
    double sampleRate = 48000.0;
};

} // namespace f64
