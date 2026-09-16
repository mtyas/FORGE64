#pragma once
#include <JuceHeader.h>
#include "ModSources.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace f64 {

// ---------------------------------------------------------------------------
// The modulation matrix: owns all 53 sources (10 LFO, 10 random, 10 env,
// 10 step-seq, 5 MIDI/performance, 8 macros) and the connection list.
//
// Threading: sources + connection cache are written on the message thread and
// read on the audio thread through atomics / a spinlock-protected snapshot.
class ModMatrix : private juce::ValueTree::Listener
{
public:
    struct Connection
    {
        int id = 0;
        int slot = 0;
        std::string dest;
        float amount = 0.5f;
        bool invert = false, muted = false;
        float curve = 0.f;
    };

    ModMatrix(juce::ValueTree srcTreeIn, juce::ValueTree matTreeIn,
              juce::AudioProcessorValueTreeState& apvtsIn);
    ~ModMatrix() override;

    static void fillDefaultTrees(juce::ValueTree& srcOut, juce::ValueTree& matOut);

    // ---- audio thread ----
    void prepare(double sr, int maxBlock);
    void setTempo(double bpm, bool isPlaying);
    void handleMidiMessage(const juce::MidiMessage& m);
    void renderSources(int n);
    void computeOffsets();
    float offsetFor(const std::string& dest) const;

    struct VoiceMods { float amp = 0.f, pitch = 0.f, pan = 0.f; };
    void triggerVoice(int voiceId);
    void releaseVoice(int voiceId) { voiceEnvs[(size_t) voiceId].clear(); }
    void renderVoice(int voiceId, VoiceMods& out, int n);

    // ---- message thread / UI ----
    int  addConnection(int slot, juce::StringRef dest, float amount = 0.5f);
    void removeConnection(int id);
    juce::ValueTree connectionById(int id) const;
    int  connectionCount() const { return matTree.getNumChildren(); }
    void setSourceParam(int slot, juce::StringRef key, juce::var value);
    void setSeqStep(int slot, int step, float v);
    void retriggerSource(int slot);
    void syncAllFromTrees();
    ModSource* sourceAt(int slot) { return sources[(size_t) slot].get(); }
    juce::ValueTree sourceState(int slot) const { return sources[(size_t) slot]->state; }

    struct Ring { int slot = 0; int id = 0; float amount = 0.f; float now = 0.f; float curve = 0.f; };
    std::vector<Ring> ringsFor(juce::StringRef dest) const;
    bool uiActive() const { return anyEnabled.load() && connectionCount() > 0; }

    static float shapeCurve(float v, float c);

private:
    struct EnvVoiceConn { int destIdx; float amount; bool invert; float curve; };
    struct Cache
    {
        std::vector<Connection> conns;
        std::array<std::vector<EnvVoiceConn>, kNumEnv> envVoice;
    };
    void rebuildCache();
    std::shared_ptr<const Cache> cacheSnapshot() const;

    void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override;

    juce::ValueTree srcTree, matTree;
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<ModSource>> sources;
    std::vector<std::vector<float>> srcBuf;
    std::vector<std::atomic<float>> srcAvg = std::vector<std::atomic<float>>((size_t) kNumSlots);
    std::array<std::atomic<float>*, kNumMacros> macroParams {};

    mutable juce::SpinLock cacheLock;
    std::shared_ptr<const Cache> cache;
    std::shared_ptr<const Cache> cacheLive;                    // audio thread only
    std::unordered_map<std::string, float> destOffsets;        // audio thread only

    struct VoiceEnvRef { int srcIdx; int inst; };
    std::array<std::vector<VoiceEnvRef>, kMaxVoices> voiceEnvs; // audio thread only
    std::array<int, kNumEnv> rrInst { 0 };

    double tempo = 120.0;
    bool transportPlaying = false;
    std::atomic<bool> anyEnabled { false };
    std::atomic<float> midiLatch[kNumMidiSrc] {};
};

} // namespace f64
