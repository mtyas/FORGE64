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
    std::atomic<bool>    scriptOn { false };
    std::atomic<float>   gainTrim { 1.f };
};

class PadGrid : private juce::ValueTree::Listener
{
public:
    PadGrid(juce::ValueTree padsTree, SampleManager& sm);
    ~PadGrid() override;

    static juce::ValueTree makeDefaultTree();

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
