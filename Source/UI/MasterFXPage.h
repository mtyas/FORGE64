#pragma once
#include <JuceHeader.h>
#include "../Engine/AuxBusManager.h"
#include "ModRingKnob.h"
#include <memory>

namespace f64 {

class Forge64Processor;

class MasterFXPage : public juce::Component, private juce::Timer
{
public:
    MasterFXPage(Forge64Processor& processor, ModRingKnob::Services& svcs);
    ~MasterFXPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    Forge64Processor& proc;
    ModRingKnob::Services& services;

    // --- Master Bus Compressor controls ---
    std::unique_ptr<juce::Label> compTitle;
    std::unique_ptr<juce::ToggleButton> compEnableBtn;
    std::unique_ptr<juce::Slider> compThreshSlider, compRatioSlider, compAtkSlider, compRelSlider, compMakeupSlider;
    std::unique_ptr<juce::Label> compThreshLabel, compRatioLabel, compAtkLabel, compRelLabel, compMakeupLabel;

    // --- Master EQ controls ---
    std::unique_ptr<juce::Label> eqTitle;
    std::unique_ptr<juce::ToggleButton> eqEnableBtn;
    std::unique_ptr<juce::Slider> eqLowSlider, eqLowMidSlider, eqHiMidSlider, eqHighSlider;
    std::unique_ptr<juce::Label> eqLowLabel, eqLowMidLabel, eqHiMidLabel, eqHighLabel;

    // --- Master Drive & Limiter controls ---
    std::unique_ptr<juce::Label> masterTitle;
    std::unique_ptr<juce::ToggleButton> driveEnableBtn, limiterEnableBtn;
    std::unique_ptr<juce::Slider> driveSlider, ceilingSlider;
    std::unique_ptr<juce::Label> driveLabel, ceilingLabel;
};

} // namespace f64
