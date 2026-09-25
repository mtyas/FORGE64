#include "StepSequencer.h"
#include <cmath>

namespace f64 {

StepSequencer::StepSequencer()
{
    for (size_t i = 0; i < 8; ++i)
        trackStepIndices[i].store(0);

    initDefaultPatterns();
}

StepSequencer::~StepSequencer() = default;

void StepSequencer::prepare(double sr)
{
    sampleRate = sr;
    reset();
}

void StepSequencer::reset()
{
    clockAccumulator = 0.0;
    global16thCounter = 0;
    currentPatternStep = 0;
    currentSongBlockIdx.store(0);
    currentBlockRepeatsCount = 0;
    for (size_t i = 0; i < 8; ++i)
        trackStepIndices[i].store(0);
    pendingTriggers.clear();
}

void StepSequencer::resetPlayback()
{
    reset();
}

PatternData& StepSequencer::currentPattern()
{
    const int idx = clampRange(currentPatternIdx.load(), 0, 15);
    return patterns[(size_t) idx];
}

const PatternData& StepSequencer::currentPattern() const
{
    const int idx = clampRange(currentPatternIdx.load(), 0, 15);
    return patterns[(size_t) idx];
}

PatternData& StepSequencer::pattern(int idx)
{
    return patterns[(size_t) clampRange(idx, 0, 15)];
}

const PatternData& StepSequencer::pattern(int idx) const
{
    return patterns[(size_t) clampRange(idx, 0, 15)];
}

void StepSequencer::setSelectedPattern(int idx)
{
    currentPatternIdx.store(clampRange(idx, 0, 15));
}

void StepSequencer::setTrackLength(int trackIdx, int length)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8)
    {
        auto& t = currentPattern().tracks[(size_t) trackIdx];
        t.stepCount = clampRange(length, 1, 64);
    }
}

void StepSequencer::setTrackPad(int trackIdx, int pad)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8)
        currentPattern().tracks[(size_t) trackIdx].defaultPad = clampRange(pad, 0, kNumPads - 1);
}

void StepSequencer::setTrackSwing(int trackIdx, float swing)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8)
        currentPattern().tracks[(size_t) trackIdx].swing = clampRange(swing, 0.0f, 0.75f);
}

void StepSequencer::setPatternSwing(float swing)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    const float s = clampRange(swing, 0.0f, 0.75f);
    for (auto& trk : currentPattern().tracks)
        trk.swing = s;
}

void StepSequencer::setTrackMute(int trackIdx, bool mute)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8)
        currentPattern().tracks[(size_t) trackIdx].mute = mute;
}

void StepSequencer::setTrackSolo(int trackIdx, bool solo)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8)
        currentPattern().tracks[(size_t) trackIdx].solo = solo;
}

void StepSequencer::setStepActive(int trackIdx, int stepIdx, bool active)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
        currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx].active = active;
}

void StepSequencer::setStepVelocity(int trackIdx, int stepIdx, float vel)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
        currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx].velocity = clampRange(vel, 0.05f, 1.0f);
}

void StepSequencer::setStepProbability(int trackIdx, int stepIdx, float prob)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
        currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx].probability = clampRange(prob, 0.0f, 1.0f);
}

void StepSequencer::setStepMicrotiming(int trackIdx, int stepIdx, float micro)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
        currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx].microtiming = clampRange(micro, -0.5f, 0.5f);
}

void StepSequencer::setStepPitch(int trackIdx, int stepIdx, float pitchSemi)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
    {
        auto& s = currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
        s.pLockPitch = clampRange(pitchSemi, -24.0f, 24.0f);
        s.hasLocks = true;
    }
}

void StepSequencer::setStepDecay(int trackIdx, int stepIdx, float decayFactor)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
    {
        auto& s = currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
        s.pLockDecay = clampRange(decayFactor, 0.05f, 5.0f);
        s.hasLocks = true;
    }
}

void StepSequencer::setStepDrive(int trackIdx, int stepIdx, float driveAmt)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
    {
        auto& s = currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
        s.pLockDrive = clampRange(driveAmt, 0.0f, 1.0f);
        s.hasLocks = true;
    }
}

void StepSequencer::setStepLevel(int trackIdx, int stepIdx, float levelAmt)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
    {
        auto& s = currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
        s.pLockLevel = clampRange(levelAmt, 0.0f, 1.5f);
        s.hasLocks = true;
    }
}

void StepSequencer::setStepPan(int trackIdx, int stepIdx, float panAmt)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
    {
        auto& s = currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
        s.pLockPan = clampRange(panAmt, -1.0f, 1.0f);
        s.hasLocks = true;
    }
}

void StepSequencer::setStepData(int trackIdx, int stepIdx, const StepData& data)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (trackIdx >= 0 && trackIdx < 8 && stepIdx >= 0 && stepIdx < 64)
        currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx] = data;
}

std::vector<SongBlock> StepSequencer::getSongSequence() const
{
    std::lock_guard<std::mutex> lock(seqMutex);
    return songSequence;
}

void StepSequencer::setSongSequence(const std::vector<SongBlock>& blocks)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    songSequence = blocks;
}

void StepSequencer::addSongBlock(int patternIdx, int repeats)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    songSequence.push_back({ clampRange(patternIdx, 0, 15), clampRange(repeats, 1, 16) });
}

void StepSequencer::removeSongBlock(int blockIdx)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (blockIdx >= 0 && blockIdx < (int) songSequence.size())
        songSequence.erase(songSequence.begin() + blockIdx);
}

void StepSequencer::clearSongSequence()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    songSequence.clear();
}

void StepSequencer::copyPattern()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    clipboardPattern = currentPattern();
    hasClipboard = true;
}

void StepSequencer::pastePattern()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (hasClipboard)
        currentPattern() = clipboardPattern;
}

void StepSequencer::clearCurrentPattern()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    for (auto& t : currentPattern().tracks)
    {
        for (auto& s : t.steps)
        {
            s.active = false;
            s.hasLocks = false;
        }
    }
}

void StepSequencer::randomizeCurrentTrack()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (selectedTrackIdx >= 0 && selectedTrackIdx < 8)
    {
        auto& t = currentPattern().tracks[(size_t) selectedTrackIdx];
        const int len = t.stepCount;
        for (int i = 0; i < len; ++i)
        {
            t.steps[(size_t) i].active = (rng.nextFloat() > 0.65f);
            t.steps[(size_t) i].velocity = 0.5f + rng.nextFloat() * 0.45f;
            t.steps[(size_t) i].hasLocks = (rng.nextFloat() > 0.8f);
            if (t.steps[(size_t) i].hasLocks)
            {
                t.steps[(size_t) i].pLockPitch = (float) rng.nextInt({ -7, 8 });
                t.steps[(size_t) i].pLockDecay = 0.5f + rng.nextFloat() * 1.5f;
            }
        }
    }
}

void StepSequencer::randomizeAllTracks()
{
    std::lock_guard<std::mutex> lock(seqMutex);
    for (int t = 0; t < 8; ++t)
    {
        auto& trk = currentPattern().tracks[(size_t) t];
        const int len = trk.stepCount;
        for (int i = 0; i < len; ++i)
        {
            trk.steps[(size_t) i].active = (rng.nextFloat() > 0.65f);
            trk.steps[(size_t) i].velocity = 0.5f + rng.nextFloat() * 0.45f;
            trk.steps[(size_t) i].hasLocks = (rng.nextFloat() > 0.8f);
            if (trk.steps[(size_t) i].hasLocks)
            {
                trk.steps[(size_t) i].pLockPitch = (float) rng.nextInt({ -7, 8 });
                trk.steps[(size_t) i].pLockDecay = 0.5f + rng.nextFloat() * 1.5f;
            }
        }
    }
}

void StepSequencer::process(int numSamples, double bpm, bool hostPlaying, std::vector<TriggerEvent>& outEvents)
{
    isHostPlaying.store(hostPlaying);
    const bool active = hostPlaying || isInternalPlaying.load();
    if (! active)
    {
        std::lock_guard<std::mutex> lock(seqMutex);
        pendingTriggers.clear();
        return;
    }

    const double safeBpm = juce::jmax(20.0, bpm);
    samplesPer16th = (60.0 / safeBpm) * 0.25 * sampleRate;

    std::lock_guard<std::mutex> lock(seqMutex);

    // 1. Dispatch pending delayed triggers that matured in this audio block
    for (auto it = pendingTriggers.begin(); it != pendingTriggers.end(); )
    {
        if (it->samplesRemaining < numSamples)
        {
            auto ev = it->event;
            ev.pos = juce::jlimit(0, numSamples - 1, it->samplesRemaining);
            outEvents.push_back(ev);
            it = pendingTriggers.erase(it);
        }
        else
        {
            it->samplesRemaining -= numSamples;
            ++it;
        }
    }

    int activePatIdx = currentPatternIdx.load();
    if (playMode.load() == MODE_SONG && ! songSequence.empty())
    {
        const int blockIdx = clampRange(currentSongBlockIdx.load(), 0, (int) songSequence.size() - 1);
        activePatIdx = songSequence[(size_t) blockIdx].patternIndex;
    }
    const auto& pat = patterns[(size_t) clampRange(activePatIdx, 0, 15)];

    bool anySolo = false;
    for (int t = 0; t < 8; ++t)
        if (pat.tracks[(size_t) t].solo) anySolo = true;

    int sampleOffset = 0;
    while (sampleOffset < numSamples)
    {
        const double samplesUntilNext16th = samplesPer16th - clockAccumulator;
        const int samplesToProcess = juce::jmin(numSamples - sampleOffset, (int) std::ceil(samplesUntilNext16th));

        clockAccumulator += samplesToProcess;
        sampleOffset += samplesToProcess;

        if (clockAccumulator >= samplesPer16th)
        {
            clockAccumulator -= samplesPer16th;
            const int blockPos = juce::jlimit(0, numSamples - 1, sampleOffset - 1);

            // Iterate through the 8 polymetric tracks
            for (int tIdx = 0; tIdx < 8; ++tIdx)
            {
                const auto& trk = pat.tracks[(size_t) tIdx];
                if (trk.mute || (anySolo && ! trk.solo))
                    continue;

                const int len = juce::jmax(1, trk.stepCount);
                const int stepIdx = (int) (global16thCounter % (int64_t) len);
                trackStepIndices[(size_t) tIdx].store(stepIdx);

                const auto& step = trk.steps[(size_t) stepIdx];

                auto scheduleStepTriggers = [&](const StepData& s, int defaultPad, int startOffset)
                {
                    const int padToTrigger = (s.padOverride >= 0 && s.padOverride < kNumPads) ? s.padOverride : defaultPad;
                    const int rCount = juce::jlimit(1, 4, s.ratchet);
                    const int subStepLen = (int) (samplesPer16th / (double) rCount);

                    for (int r = 0; r < rCount; ++r)
                    {
                        const int rTargetPos = startOffset + r * subStepLen;
                        TriggerEvent ev;
                        ev.pad = padToTrigger;
                        ev.vel = clampRange(s.velocity, 0.05f, 1.0f);
                        ev.hasLocks = s.hasLocks;
                        if (s.hasLocks)
                        {
                            ev.pitch = s.pLockPitch;
                            ev.decay = s.pLockDecay;
                            ev.tone  = s.pLockTone;
                            ev.drive = s.pLockDrive;
                            ev.sendA = s.pLockSendA;
                            ev.sendB = s.pLockSendB;
                            ev.level = s.pLockLevel;
                            ev.pan   = s.pLockPan;
                            ev.p2    = s.pLockP2;
                            ev.p3    = s.pLockP3;
                            ev.p4    = s.pLockP4;
                            ev.p5    = s.pLockP5;
                            ev.modAmt = s.pLockModAmt;
                            ev.vcfType = s.pLockVcfType;
                            ev.vcfCut  = s.pLockVcfCut;
                            ev.vcfRes  = s.pLockVcfRes;
                            ev.vcfEnv  = s.pLockVcfEnv;
                            ev.eqLF    = s.pLockEqLF;
                            ev.eqLG    = s.pLockEqLG;
                            ev.eqMF    = s.pLockEqMF;
                            ev.eqMG    = s.pLockEqMG;
                            ev.eqHF    = s.pLockEqHF;
                            ev.eqHG    = s.pLockEqHG;
                            ev.cThr    = s.pLockCThr;
                            ev.cRat    = s.pLockCRat;
                            ev.cAtk    = s.pLockCAtk;
                            ev.cRel    = s.pLockCRel;
                            ev.ifxType = s.pLockIfxType;
                            ev.ifx1    = s.pLockIfx1;
                            ev.ifx2    = s.pLockIfx2;
                            ev.ifx3    = s.pLockIfx3;
                            ev.ifx4    = s.pLockIfx4;
                            ev.sendC   = s.pLockSendC;
                            ev.sendD   = s.pLockSendD;
                        }

                        if (rTargetPos < numSamples)
                        {
                            ev.pos = juce::jlimit(0, numSamples - 1, rTargetPos);
                            outEvents.push_back(ev);
                        }
                        else
                        {
                            PendingTrigger pt;
                            pt.samplesRemaining = rTargetPos - numSamples;
                            pt.event = ev;
                            pendingTriggers.push_back(pt);
                        }
                    }
                };

                // 1. Current step trigger (for steps on grid or with positive microtiming)
                // If microtiming < -0.001f, this step was already triggered early during the previous 16th interval.
                const bool shouldTriggerNow = step.active && (step.microtiming >= -0.001f || global16thCounter == 0);
                if (shouldTriggerNow)
                {
                    if (step.probability >= 0.999f || rng.nextFloat() <= step.probability)
                    {
                        int swingOffset = ((stepIdx % 2 != 0) && trk.swing > 0.001f)
                                          ? (int) (trk.swing * samplesPer16th * 0.5) : 0;
                        const int mOffset = (step.microtiming >= -0.001f) ? (int) (step.microtiming * samplesPer16th) : 0;
                        scheduleStepTriggers(step, trk.defaultPad, blockPos + mOffset + swingOffset);
                    }
                }

                // 2. Look-ahead for next step with negative microtiming (rushed / early hit)
                const int nextStepIdx = (int) ((global16thCounter + 1) % (int64_t) len);
                const auto& nextStep = trk.steps[(size_t) nextStepIdx];
                if (nextStep.active && nextStep.microtiming < -0.001f)
                {
                    if (nextStep.probability >= 0.999f || rng.nextFloat() <= nextStep.probability)
                    {
                        int nextSwingOffset = ((nextStepIdx % 2 != 0) && trk.swing > 0.001f)
                                              ? (int) (trk.swing * samplesPer16th * 0.5) : 0;
                        const int earlyOffset = blockPos + (int) ((1.0f + nextStep.microtiming) * samplesPer16th) + nextSwingOffset;
                        scheduleStepTriggers(nextStep, trk.defaultPad, earlyOffset);
                    }
                }
            }

            ++global16thCounter;
            ++currentPatternStep;

            // Song mode advancement: advance after 16 or 64 steps
            if (playMode.load() == MODE_SONG && ! songSequence.empty())
            {
                if (currentPatternStep >= 16)
                {
                    currentPatternStep = 0;
                    ++currentBlockRepeatsCount;
                    const int curBlock = currentSongBlockIdx.load();
                    if (curBlock < (int) songSequence.size() && currentBlockRepeatsCount >= songSequence[(size_t) curBlock].repeats)
                    {
                        currentBlockRepeatsCount = 0;
                        const int nextBlock = (curBlock + 1) % (int) songSequence.size();
                        currentSongBlockIdx.store(nextBlock);
                    }
                }
            }
        }
    }

    if (outEvents.size() > 1)
    {
        std::sort(outEvents.begin(), outEvents.end(), [](const TriggerEvent& a, const TriggerEvent& b) {
            return a.pos < b.pos;
        });
    }
}

void StepSequencer::initDefaultPatterns()
{
    // Default pad assignments: 0: Kick, 1: Snare, 2: Closed Hat, 3: Open Hat, 4: Clap, 5: Low Tom, 6: High Tom, 7: Crash
    static const int defPads[8] = { 0, 1, 2, 3, 4, 5, 6, 8 };
    static const char* defNames[8] = { "Kick", "Snare", "Closed Hat", "Open Hat", "Clap", "Tom Low", "Tom Hi", "Crash" };

    for (int p = 0; p < 16; ++p)
    {
        patterns[(size_t) p].name = "Pattern " + juce::String(p + 1);
        for (int t = 0; t < 8; ++t)
        {
            auto& trk = patterns[(size_t) p].tracks[(size_t) t];
            trk.name = defNames[t];
            trk.defaultPad = defPads[t];
            trk.stepCount = 16;
            trk.swing = 0.f;
            trk.mute = false;
            trk.solo = false;
            for (auto& s : trk.steps)
            {
                s.active = false;
                s.velocity = 0.85f;
                s.padOverride = -1;
                s.probability = 1.0f;
                s.ratchet = 1;
                s.microtiming = 0.0f;
                s.hasLocks = false;
            }
        }
    }

    // Sequencer initializes completely clean and empty by default
    clearCurrentPattern();
    songSequence.clear();
}

void StepSequencer::loadFactoryPreset(int presetIdx)
{
    auto& pat = currentPattern();
    clearCurrentPattern();

    if (presetIdx == 0) // 4-on-the-floor
    {
        pat.name = "4-on-the-Floor";
        // Kick on 1, 5, 9, 13
        for (int s : { 0, 4, 8, 12 }) pat.tracks[0].steps[(size_t) s].active = true;
        // Snare/Clap on 4, 12
        for (int s : { 4, 12 }) pat.tracks[1].steps[(size_t) s].active = true;
        // Closed hat on every 8th
        for (int s = 0; s < 16; s += 2) pat.tracks[2].steps[(size_t) s].active = true;
        // Open hat on offbeats
        for (int s : { 2, 6, 10, 14 }) pat.tracks[3].steps[(size_t) s].active = true;
    }
    else if (presetIdx == 1) // Trap 808
    {
        pat.name = "Trap 808";
        // Kick
        for (int s : { 0, 6, 10 }) pat.tracks[0].steps[(size_t) s].active = true;
        // Snare on 8
        pat.tracks[1].steps[8].active = true;
        // Hi-Hat with ratchets
        for (int s = 0; s < 16; ++s)
        {
            pat.tracks[2].steps[(size_t) s].active = true;
            if (s == 6 || s == 14)
            {
                pat.tracks[2].steps[(size_t) s].ratchet = 3;
                pat.tracks[2].steps[(size_t) s].hasLocks = true;
                pat.tracks[2].steps[(size_t) s].pLockPitch = 4.0f;
            }
        }
        // Clap on 8
        pat.tracks[4].steps[8].active = true;
    }
    else if (presetIdx == 2) // Polymetric 5 vs 7 vs 16
    {
        pat.name = "Polymetric 5/7/16";
        pat.tracks[0].stepCount = 16; // Kick 4-on-floor
        for (int s : { 0, 4, 8, 12 }) pat.tracks[0].steps[(size_t) s].active = true;

        pat.tracks[1].stepCount = 5; // Snare loops every 5 steps!
        pat.tracks[1].steps[3].active = true;

        pat.tracks[2].stepCount = 7; // Hat loops every 7 steps!
        for (int s : { 0, 2, 4, 6 }) pat.tracks[2].steps[(size_t) s].active = true;

        pat.tracks[5].stepCount = 3; // Tom loops every 3 steps!
        pat.tracks[5].steps[1].active = true;
    }
    else if (presetIdx == 3) // Breakbeat
    {
        pat.name = "Breakbeat Funk";
        for (int s : { 0, 6, 10, 11 }) pat.tracks[0].steps[(size_t) s].active = true;
        for (int s : { 4, 12, 15 }) pat.tracks[1].steps[(size_t) s].active = true;
        for (int s = 0; s < 16; s += 2) pat.tracks[2].steps[(size_t) s].active = true;
        pat.tracks[2].steps[7].active = true;
        pat.tracks[2].steps[15].active = true;
    }
    else if (presetIdx == 4) // Latin Clave
    {
        pat.name = "Afro Clave";
        // Son Clave 3-2: 0, 3, 6, 10, 12
        for (int s : { 0, 3, 6, 10, 12 }) pat.tracks[4].steps[(size_t) s].active = true;
        for (int s : { 0, 8 }) pat.tracks[0].steps[(size_t) s].active = true;
        for (int s : { 2, 5, 7, 9, 13, 14 }) pat.tracks[5].steps[(size_t) s].active = true;
    }
}

juce::ValueTree StepSequencer::serialize() const
{
    std::lock_guard<std::mutex> lock(seqMutex);
    juce::ValueTree tree("SEQUENCER");
    tree.setProperty("curPat", currentPatternIdx.load(), nullptr);
    tree.setProperty("mode", (int) playMode.load(), nullptr);

    juce::ValueTree patsTree("PATTERNS");
    for (size_t p = 0; p < 16; ++p)
    {
        juce::ValueTree pTree("PATTERN");
        pTree.setProperty("idx", (int) p, nullptr);
        pTree.setProperty("name", patterns[p].name, nullptr);

        for (size_t t = 0; t < 8; ++t)
        {
            const auto& trk = patterns[p].tracks[t];
            juce::ValueTree tTree("TRACK");
            tTree.setProperty("idx", (int) t, nullptr);
            tTree.setProperty("name", trk.name, nullptr);
            tTree.setProperty("pad", trk.defaultPad, nullptr);
            tTree.setProperty("len", trk.stepCount, nullptr);
            tTree.setProperty("swing", trk.swing, nullptr);
            tTree.setProperty("mute", trk.mute, nullptr);
            tTree.setProperty("solo", trk.solo, nullptr);

            for (size_t s = 0; s < (size_t) trk.stepCount; ++s)
            {
                const auto& step = trk.steps[s];
                if (step.active || step.hasLocks)
                {
                    juce::ValueTree sTree("STEP");
                    sTree.setProperty("idx", (int) s, nullptr);
                    sTree.setProperty("act", step.active, nullptr);
                    sTree.setProperty("vel", step.velocity, nullptr);
                    sTree.setProperty("pad", step.padOverride, nullptr);
                    sTree.setProperty("prob", step.probability, nullptr);
                    sTree.setProperty("ratch", step.ratchet, nullptr);
                    sTree.setProperty("mtime", step.microtiming, nullptr);
                    if (step.hasLocks)
                    {
                        sTree.setProperty("hl", true, nullptr);
                        sTree.setProperty("lp", step.pLockPitch, nullptr);
                        sTree.setProperty("ld", step.pLockDecay, nullptr);
                        sTree.setProperty("lt", step.pLockTone, nullptr);
                        sTree.setProperty("ldrv", step.pLockDrive, nullptr);
                        sTree.setProperty("lsa", step.pLockSendA, nullptr);
                        sTree.setProperty("lsb", step.pLockSendB, nullptr);
                        sTree.setProperty("lvl", step.pLockLevel, nullptr);
                        sTree.setProperty("lpan", step.pLockPan, nullptr);
                        sTree.setProperty("lp2", step.pLockP2, nullptr);
                        sTree.setProperty("lp3", step.pLockP3, nullptr);
                        sTree.setProperty("lp4", step.pLockP4, nullptr);
                        sTree.setProperty("lp5", step.pLockP5, nullptr);
                        sTree.setProperty("lmod", step.pLockModAmt, nullptr);

                        sTree.setProperty("lvt", step.pLockVcfType, nullptr);
                        sTree.setProperty("lvc", step.pLockVcfCut, nullptr);
                        sTree.setProperty("lvr", step.pLockVcfRes, nullptr);
                        sTree.setProperty("lve", step.pLockVcfEnv, nullptr);
                        sTree.setProperty("lelf", step.pLockEqLF, nullptr);
                        sTree.setProperty("lelg", step.pLockEqLG, nullptr);
                        sTree.setProperty("lemf", step.pLockEqMF, nullptr);
                        sTree.setProperty("lemg", step.pLockEqMG, nullptr);
                        sTree.setProperty("lehf", step.pLockEqHF, nullptr);
                        sTree.setProperty("lehg", step.pLockEqHG, nullptr);
                        sTree.setProperty("lcthr", step.pLockCThr, nullptr);
                        sTree.setProperty("lcrat", step.pLockCRat, nullptr);
                        sTree.setProperty("lcatk", step.pLockCAtk, nullptr);
                        sTree.setProperty("lcrel", step.pLockCRel, nullptr);
                        sTree.setProperty("lift", step.pLockIfxType, nullptr);
                        sTree.setProperty("lif1", step.pLockIfx1, nullptr);
                        sTree.setProperty("lif2", step.pLockIfx2, nullptr);
                        sTree.setProperty("lif3", step.pLockIfx3, nullptr);
                        sTree.setProperty("lif4", step.pLockIfx4, nullptr);
                        sTree.setProperty("lsc", step.pLockSendC, nullptr);
                        sTree.setProperty("lsd", step.pLockSendD, nullptr);
                    }
                    tTree.appendChild(sTree, nullptr);
                }
            }
            pTree.appendChild(tTree, nullptr);
        }
        patsTree.appendChild(pTree, nullptr);
    }
    tree.appendChild(patsTree, nullptr);

    juce::ValueTree songTree("SONG");
    for (size_t b = 0; b < songSequence.size(); ++b)
    {
        juce::ValueTree bTree("BLOCK");
        bTree.setProperty("pat", songSequence[b].patternIndex, nullptr);
        bTree.setProperty("rep", songSequence[b].repeats, nullptr);
        songTree.appendChild(bTree, nullptr);
    }
    tree.appendChild(songTree, nullptr);

    return tree;
}

void StepSequencer::deserialize(const juce::ValueTree& tree)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    if (! tree.isValid()) return;

    currentPatternIdx.store(tree.getProperty("curPat", 0));
    playMode.store((PlayMode) (int) tree.getProperty("mode", 0));

    auto patsTree = tree.getChildWithName("PATTERNS");
    if (patsTree.isValid())
    {
        for (int p = 0; p < patsTree.getNumChildren(); ++p)
        {
            auto pTree = patsTree.getChild(p);
            const int pIdx = pTree.getProperty("idx", p);
            if (pIdx >= 0 && pIdx < 16)
            {
                patterns[(size_t) pIdx].name = pTree.getProperty("name", "Pattern " + juce::String(pIdx + 1)).toString();
                for (int t = 0; t < pTree.getNumChildren(); ++t)
                {
                    auto tTree = pTree.getChild(t);
                    const int tIdx = tTree.getProperty("idx", t);
                    if (tIdx >= 0 && tIdx < 8)
                    {
                        auto& trk = patterns[(size_t) pIdx].tracks[(size_t) tIdx];
                        trk.name = tTree.getProperty("name", trk.name).toString();
                        trk.defaultPad = tTree.getProperty("pad", trk.defaultPad);
                        trk.stepCount = clampRange((int) tTree.getProperty("len", 16), 1, 64);
                        trk.swing = tTree.getProperty("swing", 0.f);
                        trk.mute = tTree.getProperty("mute", false);
                        trk.solo = tTree.getProperty("solo", false);

                        for (auto& s : trk.steps)
                        {
                            s.active = false;
                            s.hasLocks = false;
                        }

                        for (int s = 0; s < tTree.getNumChildren(); ++s)
                        {
                            auto sTree = tTree.getChild(s);
                            const int sIdx = sTree.getProperty("idx", s);
                            if (sIdx >= 0 && sIdx < 64)
                            {
                                auto& step = trk.steps[(size_t) sIdx];
                                step.active = sTree.getProperty("act", true);
                                step.velocity = sTree.getProperty("vel", 0.85f);
                                step.padOverride = sTree.getProperty("pad", -1);
                                step.probability = sTree.getProperty("prob", 1.0f);
                                step.ratchet = sTree.getProperty("ratch", 1);
                                step.microtiming = sTree.getProperty("mtime", 0.0f);
                                step.hasLocks = sTree.getProperty("hl", false);
                                if (step.hasLocks)
                                {
                                    step.pLockPitch = sTree.getProperty("lp", 0.0f);
                                    step.pLockDecay = sTree.getProperty("ld", 1.0f);
                                    step.pLockTone  = sTree.getProperty("lt", 0.5f);
                                    step.pLockDrive = sTree.getProperty("ldrv", 0.0f);
                                    step.pLockSendA = sTree.getProperty("lsa", 0.0f);
                                    step.pLockSendB = sTree.getProperty("lsb", 0.0f);
                                    step.pLockLevel = sTree.getProperty("lvl", 1.0f);
                                    step.pLockPan   = sTree.getProperty("lpan", 0.0f);
                                    step.pLockP2    = sTree.getProperty("lp2", 0.5f);
                                    step.pLockP3    = sTree.getProperty("lp3", 0.5f);
                                    step.pLockP4    = sTree.getProperty("lp4", 0.5f);
                                    step.pLockP5    = sTree.getProperty("lp5", 0.5f);
                                    step.pLockModAmt = sTree.getProperty("lmod", 1.0f);

                                    step.pLockVcfType = sTree.getProperty("lvt", 0);
                                    step.pLockVcfCut  = sTree.getProperty("lvc", 20000.f);
                                    step.pLockVcfRes  = sTree.getProperty("lvr", 0.707f);
                                    step.pLockVcfEnv  = sTree.getProperty("lve", 0.0f);
                                    step.pLockEqLF    = sTree.getProperty("lelf", 200.f);
                                    step.pLockEqLG    = sTree.getProperty("lelg", 0.0f);
                                    step.pLockEqMF    = sTree.getProperty("lemf", 1000.f);
                                    step.pLockEqMG    = sTree.getProperty("lemg", 0.0f);
                                    step.pLockEqHF    = sTree.getProperty("lehf", 8000.f);
                                    step.pLockEqHG    = sTree.getProperty("lehg", 0.0f);
                                    step.pLockCThr    = sTree.getProperty("lcthr", 0.0f);
                                    step.pLockCRat    = sTree.getProperty("lcrat", 1.0f);
                                    step.pLockCAtk    = sTree.getProperty("lcatk", 5.0f);
                                    step.pLockCRel    = sTree.getProperty("lcrel", 100.0f);
                                    step.pLockIfxType = sTree.getProperty("lift", 0);
                                    step.pLockIfx1    = sTree.getProperty("lif1", 0.5f);
                                    step.pLockIfx2    = sTree.getProperty("lif2", 0.5f);
                                    step.pLockIfx3    = sTree.getProperty("lif3", 0.5f);
                                    step.pLockIfx4    = sTree.getProperty("lif4", 0.5f);
                                    step.pLockSendC   = sTree.getProperty("lsc", 0.0f);
                                    step.pLockSendD   = sTree.getProperty("lsd", 0.0f);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    auto songTree = tree.getChildWithName("SONG");
    if (songTree.isValid())
    {
        songSequence.clear();
        for (int b = 0; b < songTree.getNumChildren(); ++b)
        {
            auto bTree = songTree.getChild(b);
            songSequence.push_back({
                clampRange((int) bTree.getProperty("pat", 0), 0, 15),
                clampRange((int) bTree.getProperty("rep", 1), 1, 16)
            });
        }
    }
}

} // namespace f64
