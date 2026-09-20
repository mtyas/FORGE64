#pragma once
#include <JuceHeader.h>
#include "../Engine/AuxBusManager.h"
#include "ModRingKnob.h"
#include <array>
#include <memory>

namespace f64 {

class Forge64Processor;

class AuxEffectsPage : public juce::Component, private juce::Timer
{
public:
    AuxEffectsPage(Forge64Processor& processor, ModRingKnob::Services& svcs);
    ~AuxEffectsPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    struct AuxStrip : public juce::Component
    {
        AuxStrip(AuxEffectsPage& owner, int auxIndex);
        void resized() override;
        void paint(juce::Graphics& g) override;
        void updateControlLabels(int fxType);

        AuxEffectsPage& page;
        int idx;

        std::unique_ptr<juce::Label> titleLabel;
        std::unique_ptr<juce::ToggleButton> enableBtn;
        std::unique_ptr<juce::ComboBox> fxCombo;

        std::unique_ptr<juce::Slider> p1Slider, p2Slider, p3Slider, p4Slider;
        std::unique_ptr<juce::Label> p1Label, p2Label, p3Label, p4Label;

        std::unique_ptr<juce::Slider> returnSlider, panSlider;
        std::unique_ptr<juce::Label> returnLabel, panLabel;
    };

    Forge64Processor& proc;
    ModRingKnob::Services& services;

    std::array<std::unique_ptr<AuxStrip>, 4> strips;
};

} // namespace f64
