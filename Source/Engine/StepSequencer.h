#pragma once
#include <JuceHeader.h>
#include "PadDefs.h"
#include <array>
#include <vector>
#include <atomic>
#include <mutex>

namespace f64 {

enum StepLockFlags : uint64_t
{
    LOCK_FLAG_PITCH    = 1ULL << 0,
    LOCK_FLAG_DECAY    = 1ULL << 1,
    LOCK_FLAG_DRIVE    = 1ULL << 2,
    LOCK_FLAG_LEVEL    = 1ULL << 3,
    LOCK_FLAG_PAN      = 1ULL << 4,
    LOCK_FLAG_TONE     = 1ULL << 5, // FX1
    LOCK_FLAG_P2       = 1ULL << 6,
    LOCK_FLAG_P3       = 1ULL << 7,
    LOCK_FLAG_P4       = 1ULL << 8,
    LOCK_FLAG_P5       = 1ULL << 9,
    LOCK_FLAG_SENDA    = 1ULL << 10,
    LOCK_FLAG_SENDB    = 1ULL << 11,
    LOCK_FLAG_SENDC    = 1ULL << 12,
    LOCK_FLAG_SENDD    = 1ULL << 13,
    LOCK_FLAG_MODAMT   = 1ULL << 14,
    LOCK_FLAG_VCF_TYPE = 1ULL << 15,
    LOCK_FLAG_VCF_CUT  = 1ULL << 16,
    LOCK_FLAG_VCF_RES  = 1ULL << 17,
    LOCK_FLAG_VCF_ENV  = 1ULL << 18,
    LOCK_FLAG_EQ_LF    = 1ULL << 19,
    LOCK_FLAG_EQ_LG    = 1ULL << 20,
    LOCK_FLAG_EQ_MF    = 1ULL << 21,
    LOCK_FLAG_EQ_MG    = 1ULL << 22,
    LOCK_FLAG_EQ_HF    = 1ULL << 23,
    LOCK_FLAG_EQ_HG    = 1ULL << 24,
    LOCK_FLAG_COMP_THR = 1ULL << 25,
    LOCK_FLAG_COMP_RAT = 1ULL << 26,
    LOCK_FLAG_COMP_ATK = 1ULL << 27,
    LOCK_FLAG_COMP_REL = 1ULL << 28,
    LOCK_FLAG_IFX_TYPE = 1ULL << 29,
    LOCK_FLAG_IFX1     = 1ULL << 30,
    LOCK_FLAG_IFX2     = 1ULL << 31,
    LOCK_FLAG_IFX3     = 1ULL << 32,
    LOCK_FLAG_IFX4         = 1ULL << 33,
    LOCK_FLAG_PAD_OVERRIDE = 1ULL << 34,
    LOCK_FLAG_COMP_MG      = 1ULL << 35,
    LOCK_FLAG_VCF_DRIVE    = 1ULL << 36,

    // Aliases
    LOCK_FLAG_SEND_A       = 1ULL << 10,
    LOCK_FLAG_SEND_B       = 1ULL << 11,
    LOCK_FLAG_SEND_C       = 1ULL << 12,
    LOCK_FLAG_SEND_D       = 1ULL << 13,
};

struct StepData
{
    bool  active = false;
    float velocity = 0.85f;
    int   padOverride = -1;       // -1 = use track default, 0..63 = override pad
    float probability = 1.0f;     // 0.0 .. 1.0
    int   ratchet = 1;            // 1, 2, 3, 4 subdivisions
    float microtiming = 0.0f;     // -0.5 .. +0.5 fraction of step

    // Parameter Locks (P-Locks)
    bool     hasLocks = false;
    uint64_t lockMask = 0;
    float pLockPitch = 0.0f;      // Semitones (-24 .. +24)
    float pLockDecay = 1.0f;      // Decay factor (0.05 .. 5.0)
    float pLockTone  = 0.5f;      // FX1 (Punch / Tone / Cutoff)
    float pLockDrive = 0.0f;      // Drive (0.0 .. 1.0)
    float pLockSendA = 0.0f;      // Send A
    float pLockSendB = 0.0f;      // Send B
    float pLockLevel = 1.0f;      // Level factor
    float pLockPan   = 0.0f;      // Pan (-1.0 .. +1.0)
    float pLockP2    = 0.5f;      // FX2
    float pLockP3    = 0.5f;      // FX3
    float pLockP4    = 0.5f;      // FX4
    float pLockP5    = 0.5f;      // FX5
    float pLockModAmt = 1.0f;     // Modulation depth factor (0.0 .. 2.0)

    // VCF Filter Locks
    int   pLockVcfType = 0;       // 0: Bypass, 1: LP, 2: HP, 3: BP, 4: Notch
    float pLockVcfCut  = 20000.f; // Cutoff Hz
    float pLockVcfRes  = 0.707f;  // Resonance
    float pLockVcfEnv  = 0.0f;    // Env Amount (-1.0 .. +1.0)
    float pLockVcfDrive = 0.f;

    // 3-Band Parametric EQ Locks
    float pLockEqLF = 200.f;      // Lo Freq Hz
    float pLockEqLG = 0.f;        // Lo Gain dB
    float pLockEqMF = 1000.f;     // Mid Freq Hz
    float pLockEqMG = 0.f;        // Mid Gain dB
    float pLockEqHF = 8000.f;     // Hi Freq Hz
    float pLockEqHG = 0.f;        // Hi Gain dB

    // Compressor Locks
    float pLockCThr = 0.f;        // Threshold dB (-60 .. 0)
    float pLockCRat = 1.f;        // Ratio (1 .. 20)
    float pLockCAtk = 5.f;        // Attack ms (0.1 .. 100)
    float pLockCRel = 100.f;      // Release ms (10 .. 1000)
    float pLockCMg  = 0.f;        // Make-Up Gain dB (0 .. 24)

    // Dedicated Insert Multi-FX Locks
    int   pLockIfxType = 0;       // 0: Off, 1: Flanger, 2: Chorus, 3: Crusher, 4: Phaser
    float pLockIfx1 = 0.5f;
    float pLockIfx2 = 0.5f;
    float pLockIfx3 = 0.5f;
    float pLockIfx4 = 0.5f;

    // Aux Sends C and D Locks
    float pLockSendC = 0.0f;
    float pLockSendD = 0.0f;

    void clearLocks()
    {
        hasLocks = false;
        lockMask = 0;
        ratchet = 1;
        microtiming = 0.0f;
        probability = 1.0f;
        pLockPitch = 0.0f;
        pLockDecay = 1.0f;
        pLockTone  = 0.5f;
        pLockDrive = 0.0f;
        pLockSendA = 0.0f;
        pLockSendB = 0.0f;
        pLockLevel = 1.0f;
        pLockPan   = 0.0f;
        pLockP2    = 0.5f;
        pLockP3    = 0.5f;
        pLockP4    = 0.5f;
        pLockP5    = 0.5f;
        pLockModAmt = 1.0f;
        pLockVcfType = 0;
        pLockVcfCut  = 20000.f;
        pLockVcfRes  = 0.707f;
        pLockVcfEnv  = 0.0f;
        pLockVcfDrive = 0.f;
        pLockEqLF = 200.f;
        pLockEqLG = 0.f;
        pLockEqMF = 1000.f;
        pLockEqMG = 0.f;
        pLockEqHF = 8000.f;
        pLockEqHG = 0.f;
        pLockCThr = 0.f;
        pLockCRat = 1.f;
        pLockCAtk = 5.f;
        pLockCRel = 100.f;
        pLockCMg  = 0.f;
        pLockIfxType = 0;
        pLockIfx1 = 0.5f;
        pLockIfx2 = 0.5f;
        pLockIfx3 = 0.5f;
        pLockIfx4 = 0.5f;
        pLockSendC = 0.0f;
        pLockSendD = 0.0f;
    }

    void resetStep()
    {
        active = false;
        velocity = 0.85f;
        padOverride = -1;
        clearLocks();
    }
};

struct TrackData
{
    juce::String name = "Track 1";
    int   defaultPad = 0;         // 0..63
    int   stepCount = 16;         // Polymetric length: 1 to 64 steps
    float speedMultiplier = 1.0f; // Multipliers/dividers: 0.125, 0.25, 0.33, 0.5, 0.67, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0
    float swing = 0.0f;           // 0.0 to 0.75
    bool  mute = false;
    bool  solo = false;
    std::array<StepData, 64> steps;
};

struct PatternData
{
    juce::String name = "Pattern 1";
    std::array<TrackData, 8> tracks;
};

struct SongBlock
{
    int patternIndex = 0;         // 0..15
    int repeats = 1;              // 1..16
};

class StepSequencer
{
public:
    enum PlayMode { MODE_PATTERN = 0, MODE_SONG = 1 };

    struct TriggerEvent
    {
        int   pad = 0;
        float vel = 0.85f;
        int   pos = 0;            // sample position in block
        bool  hasLocks = false;
        uint64_t lockMask = 0;
        float pitch = 0.f;
        float decay = 1.f;
        float tone = 0.5f;
        float drive = 0.f;
        float sendA = 0.f;
        float sendB = 0.f;
        float level = 1.f;
        float pan = 0.f;
        float p2 = 0.5f;
        float p3 = 0.5f;
        float p4 = 0.5f;
        float p5 = 0.5f;
        float modAmt = 1.f;

        // VCF Filter
        int   vcfType = 0;
        float vcfCut  = 20000.f;
        float vcfRes  = 0.707f;
        float vcfEnv  = 0.f;
        float vcfDrive = 0.f;

        // EQ
        float eqLF = 200.f;
        float eqLG = 0.f;
        float eqMF = 1000.f;
        float eqMG = 0.f;
        float eqHF = 8000.f;
        float eqHG = 0.f;

        // Compressor
        float cThr = 0.f;
        float cRat = 1.f;
        float cAtk = 5.f;
        float cRel = 100.f;
        float cMg  = 0.f;

        // Insert Multi-FX
        int   ifxType = 0;
        float ifx1 = 0.5f;
        float ifx2 = 0.5f;
        float ifx3 = 0.5f;
        float ifx4 = 0.5f;

        // Sends C / D
        float sendC = 0.f;
        float sendD = 0.f;
    };

    StepSequencer();
    ~StepSequencer();

    void prepare(double sampleRate);
    void reset();

    // Call from audio process block
    void process(int numSamples, double bpm, bool hostPlaying, std::vector<TriggerEvent>& outEvents);

    // Transport controls
    void setPlaying(bool play) { isInternalPlaying.store(play); if (! play && ! isHostPlaying.load()) resetPlayback(); }
    bool isPlaying() const { return isInternalPlaying.load() || isHostPlaying.load(); }
    bool isInternalPlayingActive() const { return isInternalPlaying.load(); }
    bool isHostPlayingActive() const { return isHostPlaying.load(); }
    void setPlayMode(PlayMode mode) { playMode.store(mode); }
    PlayMode getPlayMode() const { return playMode.load(); }

    void setSelectedPattern(int idx);
    int  selectedPatternIndex() const { return currentPatternIdx.load(); }

    void setSelectedTrack(int idx) { selectedTrackIdx = clampRange(idx, 0, 7); }
    int  selectedTrackIndex() const { return selectedTrackIdx; }

    void setPage(int p) { currentPage = clampRange(p, 0, 3); }
    int  getPage() const { return currentPage; } // 0: 1-16, 1: 17-32, 2: 33-48, 3: 49-64

    PatternData& currentPattern();
    const PatternData& currentPattern() const;
    PatternData& pattern(int idx);
    const PatternData& pattern(int idx) const;

    // Track getters/setters (thread-safe for UI)
    void setTrackLength(int trackIdx, int length);
    void setTrackPad(int trackIdx, int pad);
    void setTrackSpeed(int trackIdx, float speedMultiplier);
    float getTrackSpeed(int trackIdx) const;
    void setTrackSwing(int trackIdx, float swing);
    void setPatternSwing(float swing);
    void setTrackMute(int trackIdx, bool mute);
    void setTrackSolo(int trackIdx, bool solo);
    void setStepActive(int trackIdx, int stepIdx, bool active);
    void setStepActiveWithPad(int trackIdx, int stepIdx, bool active, int padOverride);
    void copyStep(int trackIdx, int srcStepIdx, int dstStepIdx);
    void setStepVelocity(int trackIdx, int stepIdx, float vel);
    void setStepProbability(int trackIdx, int stepIdx, float prob);
    void setStepMicrotiming(int trackIdx, int stepIdx, float micro);
    void setStepPitch(int trackIdx, int stepIdx, float pitchSemi);
    void setStepDecay(int trackIdx, int stepIdx, float decayFactor);
    void setStepDrive(int trackIdx, int stepIdx, float driveAmt);
    void setStepLevel(int trackIdx, int stepIdx, float levelAmt);
    void setStepPan(int trackIdx, int stepIdx, float panAmt);
    void setStepData(int trackIdx, int stepIdx, const StepData& data);

    // Song mode blocks
    std::vector<SongBlock> getSongSequence() const;
    void setSongSequence(const std::vector<SongBlock>& blocks);
    void addSongBlock(int patternIdx, int repeats);
    void removeSongBlock(int blockIdx);
    void clearSongSequence();

    // Pattern access & Clear (heap copies to prevent thread stack overflow)
    std::unique_ptr<PatternData> getPatternCopy(int idx) const;
    void setPattern(int idx, const PatternData& data);
    std::unique_ptr<std::array<PatternData, 16>> getAllPatternsCopy() const;
    void setAllPatterns(const std::array<PatternData, 16>& data);

    // Clipboard & Presets
    void copyPattern();
    void pastePattern();
    void clearCurrentPattern();
    void clearAllPatterns();
    void clearTrack(int trackIdx);
    void randomizeCurrentTrack();
    void randomizeAllTracks();
    void loadFactoryPreset(int presetIdx);

    // Page-level Clipboard & Duplication
    bool hasPageInClipboard() const { return hasPageClipboard; }
    void duplicateTrackLoop(int trackIdx, int multiplier);
    void duplicateAllTracksLoop(int multiplier);
    void copyPage(int trackIdx, int pageIdx);
    void pastePage(int trackIdx, int pageIdx);
    void duplicatePageToNext(int trackIdx, int pageIdx);
    void duplicatePageToAll(int trackIdx, int pageIdx);
    void clearPage(int trackIdx, int pageIdx);

    void copyAllTracksPage(int pageIdx);
    void pasteAllTracksPage(int pageIdx);
    void duplicateAllTracksPageToNext(int pageIdx);
    void duplicateAllTracksPageToAll(int pageIdx);
    void clearAllTracksPage(int pageIdx);

    // Transport synchronization with DAW / UI
    void resetPlayback();

    // Playhead tracking for UI
    int getTrackPlayhead(int trackIdx) const { return (trackIdx >= 0 && trackIdx < 8) ? trackStepIndices[(size_t) trackIdx].load() : 0; }
    int getSongBlockIndex() const { return currentSongBlockIdx.load(); }

    // Pattern versioning for safe lock-free UI cache invalidation
    void bumpPatternVersion() { patternVersion.fetch_add(1, std::memory_order_relaxed); }
    uint32_t getPatternVersion() const { return patternVersion.load(std::memory_order_relaxed); }

    // Pattern & State serialization
    juce::ValueTree serializePattern(int patternIndex) const;
    void deserializePattern(int patternIndex, const juce::ValueTree& patternTree);
    juce::ValueTree serialize() const;
    void deserialize(const juce::ValueTree& tree);

private:
    struct PendingTrigger
    {
        int samplesRemaining = 0;
        TriggerEvent event;
    };

    void initDefaultPatterns();

    mutable std::mutex seqMutex;
    std::atomic<uint32_t> patternVersion { 0 };
    std::array<PatternData, 16> patterns;
    PatternData clipboardPattern;
    bool hasClipboard = false;

    std::array<StepData, 16> pageClipboard {};
    bool hasPageClipboard = false;
    std::array<std::array<StepData, 16>, 8> allTracksPageClipboard {};
    bool hasAllTracksPageClipboard = false;

    std::vector<SongBlock> songSequence;
    std::vector<PendingTrigger> pendingTriggers;

    std::atomic<bool> isInternalPlaying { false };
    std::atomic<bool> isHostPlaying { false };
    std::atomic<PlayMode> playMode { MODE_PATTERN };
    std::atomic<int> currentPatternIdx { 0 };
    int selectedTrackIdx = 0;
    int currentPage = 0;

    double sampleRate = 48000.0;
    double samplesPer16th = 6000.0;
    double clockAccumulator = 0.0;
    int64_t global16thCounter = 0;

    std::array<double, 8> trackSamplesRemaining {};
    std::array<int, 8> trackCurrentSteps {};

    std::atomic<int> currentSongBlockIdx { 0 };
    int currentBlockRepeatsCount = 0;
    int currentPatternStep = 0;

    std::array<std::atomic<int>, 8> trackStepIndices;
    juce::Random rng;
};

// ---------------------------------------------------------------------------
// Sequencer Undoable Actions
// ---------------------------------------------------------------------------
class SequencerPatternAction : public juce::UndoableAction
{
public:
    SequencerPatternAction(StepSequencer& s, int patternIndex,
                           std::unique_ptr<PatternData> before,
                           std::unique_ptr<PatternData> after)
        : seq(s), patIdx(patternIndex), beforeState(std::move(before)), afterState(std::move(after))
    {
    }

    bool perform() override
    {
        if (afterState)
            seq.setPattern(patIdx, *afterState);
        return true;
    }

    bool undo() override
    {
        if (beforeState)
            seq.setPattern(patIdx, *beforeState);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    StepSequencer& seq;
    int patIdx;
    std::unique_ptr<PatternData> beforeState;
    std::unique_ptr<PatternData> afterState;
};

class SequencerAllPatternsAction : public juce::UndoableAction
{
public:
    SequencerAllPatternsAction(StepSequencer& s,
                               std::unique_ptr<std::array<PatternData, 16>> before,
                               std::unique_ptr<std::array<PatternData, 16>> after)
        : seq(s), beforeState(std::move(before)), afterState(std::move(after))
    {
    }

    bool perform() override
    {
        if (afterState)
            seq.setAllPatterns(*afterState);
        return true;
    }

    bool undo() override
    {
        if (beforeState)
            seq.setAllPatterns(*beforeState);
        return true;
    }

    int getSizeInUnits() override { return 16; }

private:
    StepSequencer& seq;
    std::unique_ptr<std::array<PatternData, 16>> beforeState;
    std::unique_ptr<std::array<PatternData, 16>> afterState;
};

} // namespace f64
