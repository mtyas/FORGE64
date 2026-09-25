#pragma once
#include <JuceHeader.h>
#include "PadDefs.h"
#include <array>
#include <vector>
#include <atomic>
#include <mutex>

namespace f64 {

struct StepData
{
    bool  active = false;
    float velocity = 0.85f;
    int   padOverride = -1;       // -1 = use track default, 0..63 = override pad
    float probability = 1.0f;     // 0.0 .. 1.0
    int   ratchet = 1;            // 1, 2, 3, 4 subdivisions
    float microtiming = 0.0f;     // -0.5 .. +0.5 fraction of step

    // Parameter Locks (P-Locks)
    bool  hasLocks = false;
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

    // Dedicated Insert Multi-FX Locks
    int   pLockIfxType = 0;       // 0: Off, 1: Flanger, 2: Chorus, 3: Crusher, 4: Phaser
    float pLockIfx1 = 0.5f;
    float pLockIfx2 = 0.5f;
    float pLockIfx3 = 0.5f;
    float pLockIfx4 = 0.5f;

    // Aux Sends C and D Locks
    float pLockSendC = 0.0f;
    float pLockSendD = 0.0f;
};

struct TrackData
{
    juce::String name = "Track 1";
    int   defaultPad = 0;         // 0..63
    int   stepCount = 16;         // Polymetric length: 1 to 64 steps
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
    void setTrackSwing(int trackIdx, float swing);
    void setPatternSwing(float swing);
    void setTrackMute(int trackIdx, bool mute);
    void setTrackSolo(int trackIdx, bool solo);
    void setStepActive(int trackIdx, int stepIdx, bool active);
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

    // Clipboard & Presets
    void copyPattern();
    void pastePattern();
    void clearCurrentPattern();
    void randomizeCurrentTrack();
    void randomizeAllTracks();
    void loadFactoryPreset(int presetIdx);

    // Playhead tracking for UI
    int getTrackPlayhead(int trackIdx) const { return (trackIdx >= 0 && trackIdx < 8) ? trackStepIndices[(size_t) trackIdx].load() : 0; }
    int getSongBlockIndex() const { return currentSongBlockIdx.load(); }

    // State serialization
    juce::ValueTree serialize() const;
    void deserialize(const juce::ValueTree& tree);

private:
    struct PendingTrigger
    {
        int samplesRemaining = 0;
        TriggerEvent event;
    };

    void initDefaultPatterns();
    void resetPlayback();

    mutable std::mutex seqMutex;
    std::array<PatternData, 16> patterns;
    PatternData clipboardPattern;
    bool hasClipboard = false;

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

    std::atomic<int> currentSongBlockIdx { 0 };
    int currentBlockRepeatsCount = 0;
    int currentPatternStep = 0;

    std::array<std::atomic<int>, 8> trackStepIndices;
    juce::Random rng;
};

} // namespace f64
