#pragma once
#include <JuceHeader.h>
#include "Engine/PadDefs.h"
#include "Engine/SampleManager.h"
#include "Engine/PadGrid.h"
#include "Engine/VoicePool.h"
#include "Engine/PadChain.h"
#include "Engine/GlobalFX.h"
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

    std::atomic<int>& uiBank() { return currentBank; }
    int64_t getClock() const { return clock.load(); }

    enum GlobalParam { GI_Master = 0, GI_RevSize, GI_RevDamp, GI_DlyTime, GI_DlyFb, GI_Count };
    float globalEff(int idx) const;

private:
    struct RawMidiEv { int pos = 0; float vel = 0.f; int note = 0; int chan = 1; bool off = false; };

    static BusesProperties makeBuses();
    static juce::AudioProcessorParameters makeParams();
    void fillPadParams(int pad, PadParams& out);
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

    std::array<PadParams, kNumPads> eff;
    std::array<std::array<juce::RangedAudioParameter*, kNumPadParams>, kNumPads> padPtrs {};
    std::array<std::array<std::string, kNumPadParams>, kNumPads> padIds;
    std::array<juce::RangedAudioParameter*, GI_Count> globalPtrs {};

    std::array<int, kNumBuses> busOffset {};
    std::array<bool, kNumBuses> busActive {};

    juce::AudioBuffer<float> busScratch, auxA, auxB;
    std::vector<float> scratchL, scratchR;
    std::array<std::vector<VoicePool::TimedEvent>, kNumPads> padEvents;
    std::array<std::vector<int>, 128> noteMap;
    std::array<int, 17> chromMap {};
    std::vector<RawMidiEv> rawEvents;

    std::atomic<int64_t> clock { 0 };
    std::atomic<int> currentBank { 0 };
};

} // namespace f64
