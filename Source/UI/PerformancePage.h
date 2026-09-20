#pragma once
#include <JuceHeader.h>
#include "ModRingKnob.h"
#include "UICommon.h"
#include "../Engine/PadDefs.h"
#include <array>
#include <memory>
#include <vector>

namespace f64 {

class Forge64Processor;

// ===========================================================================
// Interactive XY Expression Pad Component
// ===========================================================================
class XYPadComponent : public juce::Component, private juce::Timer
{
public:
    XYPadComponent(Forge64Processor& processor, const juce::String& title,
                   int defaultXMacro, int defaultYMacro);
    ~XYPadComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void updateFromMouse(const juce::MouseEvent& e);
    void applyPuckToParams();
    void syncPuckFromParams();

    Forge64Processor& proc;
    juce::String padTitle;

    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::Label> xDestLabel;
    std::unique_ptr<juce::ComboBox> xDestCombo;
    std::unique_ptr<juce::Label> yDestLabel;
    std::unique_ptr<juce::ComboBox> yDestCombo;
    std::unique_ptr<juce::ToggleButton> springToggle;
    std::unique_ptr<juce::Label> coordsLabel;

    float puckX = 0.5f;
    float puckY = 0.5f;
    bool isDragging = false;
    bool springToCenter = false;

    juce::Rectangle<float> padArea;
};

// ===========================================================================
// Performance Page (8 Forge Macro Dials + Dual XY Expression Pads)
// ===========================================================================
class PerformancePage : public juce::Component
{
public:
    PerformancePage(Forge64Processor& processor, ModRingKnob::Services& svcs);
    ~PerformancePage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    Forge64Processor& proc;
    ModRingKnob::Services& services;

    std::unique_ptr<juce::Label> pageTitle;
    std::unique_ptr<juce::TextButton> resetMacrosBtn;
    std::unique_ptr<juce::TextButton> randomMacrosBtn;

    struct MacroSlot
    {
        std::unique_ptr<juce::Label> nameEdit;
        std::unique_ptr<ModRingKnob> knob;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    std::array<MacroSlot, kNumMacros> macroSlots;

    std::unique_ptr<XYPadComponent> xyPadA;
    std::unique_ptr<XYPadComponent> xyPadB;

    // Quick Live Audition Trigger Strip
    std::array<std::unique_ptr<juce::TextButton>, 16> liveTrigBtns;
};

} // namespace f64
