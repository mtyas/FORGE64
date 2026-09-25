#pragma once
#include <JuceHeader.h>
#include "Engine/PadDefs.h"
#include "Engine/SampleManager.h"
#include "Engine/PadGrid.h"
#include "Engine/VoicePool.h"
#include "Engine/PadChain.h"
#include "Engine/GlobalFX.h"
#include "Engine/AuxBusManager.h"
#include "Engine/MidiLearn.h"
#include "Engine/StepSequencer.h"
#include "Modulation/ModMatrix.h"
#include "Scripting/LuaEngine.h"
#include "Presets/PresetManager.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace f64 {

class Forge64Processor : public juce::AudioProcessor
{
public:
    Forge64Processor();
    ~Forge64Processor() override;

    void prepareToPlay(double sampleRate, int maximumExpectedBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::ValueTree& getKit() { return kitRoot; }
    PadGrid& grid() { return *gridPtr; }
    ModMatrix& mods() { return *modPtr; }
    LuaEngine& lua() { return luaEngine; }
    SampleManager& samples() { return sampleManager; }
    PresetManager& presets() { return *presetPtr; }
    AuxBusManager& getAuxManager() { return auxManager; }
    MidiLearnManager& getMidiLearn() { return midiLearn; }
    StepSequencer& getSequencer() { return sequencer; }
    juce::UndoManager& getUndoManager() { return undoManager; }

    int lastUIWidth = 1200;
    int lastUIHeight = 800;

    std::atomic<int>& uiBank() { return currentBank; }
    int64_t getClock() const { return clock.load(); }

    enum GlobalParam { GI_Master = 0, GI_RevSize, GI_RevDamp, GI_DlyTime, GI_DlyFb, GI_Count };
    float globalEff(int idx) const;

    void triggerAudition(int pad, float velocity = 0.9f);
    void triggerStepAudition(int pad, float velocity, const StepData& stepData);
    void allNotesOff() { voices.allNotesOff(); }
    int getLastTriggeredPad() const { return lastTriggeredPad.load(); }

    float getMasterPeakL() const { return masterPeakL.load(); }
    float getMasterPeakR() const { return masterPeakR.load(); }

private:
    std::atomic<float> masterPeakL { 0.f }, masterPeakR { 0.f };
    std::atomic<int> lastTriggeredPad { -1 };
    struct AuditionTrigger
    {
        int   pad = 0;
        float vel = 0.9f;
        bool  hasLocks = false;
        float pitch = 0.f, decay = 1.f, drive = 0.f, tone = 0.5f;
        float p2 = 0.5f, p3 = 0.5f, p4 = 0.5f, p5 = 0.5f;
        float sendA = 0.f, sendB = 0.f, level = 1.f, pan = 0.f, modAmt = 1.f;

        // VCF
        int   vcfType = 0;
        float vcfCut = 20000.f, vcfRes = 0.707f, vcfEnv = 0.f;

        // EQ
        float eqLF = 200.f, eqLG = 0.f, eqMF = 1000.f, eqMG = 0.f, eqHF = 8000.f, eqHG = 0.f;

        // Compressor
        float cThr = 0.f, cRat = 1.f, cAtk = 5.f, cRel = 100.f;

        // Insert Multi-FX
        int   ifxType = 0;
        float ifx1 = 0.5f, ifx2 = 0.5f, ifx3 = 0.5f, ifx4 = 0.5f;

        // Sends C / D
        float sendC = 0.f, sendD = 0.f;
    };
    juce::SpinLock auditionLock;
    std::vector<AuditionTrigger> auditionQueue;
    struct RawMidiEv { int pos = 0; float vel = 0.f; int note = 0; int chan = 1; bool off = false; };

    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParams();
    void fillPadParams(int pad, PadParams& out, float modScale = 1.0f);
    void onPresetLoaded();

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree kitRoot;
    SampleManager sampleManager;
    std::unique_ptr<PadGrid> gridPtr;
    std::unique_ptr<ModMatrix> modPtr;
    std::unique_ptr<PresetManager> presetPtr;
    LuaEngine luaEngine;
    VoicePool voices;
    std::unique_ptr<PadChain[]> chains { new PadChain[kNumPads] };
    GlobalFX gfx;
    AuxBusManager auxManager;
    MidiLearnManager midiLearn;
    StepSequencer sequencer;
    juce::UndoManager undoManager;

    std::array<PadParams, kNumPads> eff;
    std::array<std::array<juce::RangedAudioParameter*, kNumPadParams>, kNumPads> padPtrs {};
    std::array<std::array<std::string, kNumPadParams>, kNumPads> padIds;
    std::array<juce::RangedAudioParameter*, GI_Count> globalPtrs {};

    std::array<int, kNumBuses> busOffset {};
    std::array<bool, kNumBuses> busActive {};

    juce::AudioBuffer<float> busScratch, auxA, auxB, auxC, auxD;
    std::vector<float> scratchL, scratchR;
    std::array<std::vector<VoicePool::TimedEvent>, kNumPads> padEvents;
    std::array<int, kNumPads> padTailHold {};
    float dynamicSummingGain = 1.0f;
    std::array<std::vector<int>, 128> noteMap;
    std::array<int, 17> chromMap {};
    std::vector<RawMidiEv> rawEvents;

    std::atomic<int64_t> clock { 0 };
    std::atomic<int> currentBank { 0 };
};

} // namespace f64
