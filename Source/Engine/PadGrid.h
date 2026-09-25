#pragma once
#include <JuceHeader.h>
#include "PadDefs.h"
#include "SampleManager.h"
#include <atomic>

namespace f64 {

// Per-pad state that the audio thread touches lives here as atomics;
// everything persistent lives in the PADS ValueTree.
struct PadRuntime
{
    juce::CriticalSection lock;          // guards sample
    SampleManager::Ptr sample;
    std::atomic<int64_t> lastHitStamp { 0 };
    std::atomic<float>   lastVel { 0.f };
    std::atomic<int>     lastNote { 60 };
    std::atomic<bool>    scriptOn { false };
    std::atomic<float>   gainTrim { 1.f };
    std::atomic<bool>    isMuted  { false };
    std::atomic<bool>    isSolo   { false };

    // Sampler boundaries & playback modes
    std::atomic<float>   sampleStart { 0.f };
    std::atomic<float>   sampleEnd   { 1.f };
    std::atomic<float>   loopStart   { 0.f };
    std::atomic<float>   loopEnd     { 1.f };
    std::atomic<bool>    loopOn      { false };
    std::atomic<bool>    reverseOn   { false };

    // Latched Parameter Locks (P-Locks from StepSequencer / MIDI)
    std::atomic<bool>    hasLocks     { false };
    std::atomic<float>   latchedTune  { 0.f };
    std::atomic<float>   latchedDecay { 1.f };
    std::atomic<float>   latchedTone  { 0.5f };
    std::atomic<float>   latchedDrive { 0.f };
    std::atomic<float>   latchedSendA { 0.f };
    std::atomic<float>   latchedSendB { 0.f };
    std::atomic<float>   latchedLevel { 1.f };
    std::atomic<float>   latchedPan   { 0.f };
    std::atomic<float>   latchedP2    { 0.5f };
    std::atomic<float>   latchedP3    { 0.5f };
    std::atomic<float>   latchedP4    { 0.5f };
    std::atomic<float>   latchedP5    { 0.5f };
    std::atomic<float>   latchedModAmt { 1.0f };

    // Latched VCF Filter
    std::atomic<int>     latchedVcfType { 0 };
    std::atomic<float>   latchedVcfCut  { 20000.f };
    std::atomic<float>   latchedVcfRes  { 0.707f };
    std::atomic<float>   latchedVcfEnv  { 0.f };

    // Latched 3-Band Parametric EQ
    std::atomic<float>   latchedEqLF { 200.f };
    std::atomic<float>   latchedEqLG { 0.f };
    std::atomic<float>   latchedEqMF { 1000.f };
    std::atomic<float>   latchedEqMG { 0.f };
    std::atomic<float>   latchedEqHF { 8000.f };
    std::atomic<float>   latchedEqHG { 0.f };

    // Latched Dynamics Compressor
    std::atomic<float>   latchedCThr { 0.f };
    std::atomic<float>   latchedCRat { 1.f };
    std::atomic<float>   latchedCAtk { 5.f };
    std::atomic<float>   latchedCRel { 100.f };

    // Latched Dedicated Insert Multi-FX
    std::atomic<int>     latchedIfxType { 0 };
    std::atomic<float>   latchedIfx1 { 0.5f };
    std::atomic<float>   latchedIfx2 { 0.5f };
    std::atomic<float>   latchedIfx3 { 0.5f };
    std::atomic<float>   latchedIfx4 { 0.5f };

    // Latched Aux Sends C and D
    std::atomic<float>   latchedSendC { 0.f };
    std::atomic<float>   latchedSendD { 0.f };

    std::atomic<bool>    isNewTrigger { false };
};

struct DefaultPadPreset
{
    float tune    = 0.f;
    float decay   = 1.0f;
    float drive   = 0.f;
    float p1      = 0.5f;
    float p2      = 0.5f;
    float p3      = 0.5f;
    float p4      = 0.5f;
    float p5      = 0.5f;
    int   choke   = 0;
    int   vcfType = 0;
    float vcfCut  = 20000.f;
    float vcfRes  = 0.707f;
    float vcfEnv  = 0.f;
};

class PadGrid : private juce::ValueTree::Listener
{
public:
    PadGrid(juce::ValueTree padsTree, SampleManager& sm);
    ~PadGrid() override;

    static juce::ValueTree makeDefaultTree();
    static DefaultPadPreset getDefaultPadPreset(int pad);

    void bindParams(juce::AudioProcessorValueTreeState* a) { apvts = a; }

    juce::ValueTree padState(int pad) const { return tree.getChild(pad); }
    PadRuntime& runtime(int pad) { return rt[(size_t) pad]; }

    void reloadSamples();                               // message thread
    void setSampleFile(int pad, const juce::File& f);   // message thread
    void copyPad(int from, int to);                     // message thread
    void clearPad(int pad);                             // message thread
    void clearSample(int pad);                          // message thread
    void syncAllAtomics();

private:
    void syncAtomics(int pad);
    void valueTreePropertyChanged(juce::ValueTree& t, const juce::Identifier&) override;

    juce::ValueTree tree;
    SampleManager& sm;
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    PadRuntime rt[kNumPads];
};

} // namespace f64
