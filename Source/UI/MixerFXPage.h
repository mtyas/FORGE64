#pragma once
#include <JuceHeader.h>
#include "../Engine/AuxBusManager.h"
#include "ModRingKnob.h"
#include "UICommon.h"
#include <memory>
#include <vector>
#include <array>
#include <functional>

namespace f64 {

class Forge64Processor;

// ---------------------------------------------------------------------------
// Real-time 4-Band Master EQ Graphical Frequency Visualizer
// ---------------------------------------------------------------------------
class MasterEQGraphView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MasterEQGraphView(Forge64Processor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    std::function<void(int band, float newGain)> onBandGainChanged;

private:
    Forge64Processor& proc;
    int activeBand = -1;
};

// ---------------------------------------------------------------------------
// Hardware-Style VCA Gain Reduction (GR) Meter
// ---------------------------------------------------------------------------
class CompGRMeter : public juce::Component
{
public:
    explicit CompGRMeter(Forge64Processor& processor);
    void paint(juce::Graphics& g) override;

private:
    Forge64Processor& proc;
};

// ---------------------------------------------------------------------------
// Stereo L/R Output Peak Meter with Clip Detection
// ---------------------------------------------------------------------------
class MasterPeakMeter : public juce::Component
{
public:
    explicit MasterPeakMeter(Forge64Processor& processor);
    void paint(juce::Graphics& g) override;

private:
    Forge64Processor& proc;
    int clipHoldL = 0;
    int clipHoldR = 0;
};

// ---------------------------------------------------------------------------
// Main Mixer & FX Page
// ---------------------------------------------------------------------------
class MixerFXPage : public juce::Component, private juce::Timer
{
public:
    MixerFXPage(Forge64Processor& processor, ModRingKnob::Services& services);
    ~MixerFXPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class AuxStrip : public juce::Component
    {
    public:
        AuxStrip(MixerFXPage& owner, int auxIndex);
        void resized() override;
        void paint(juce::Graphics& g) override;
        void updateLabels(int fxType);

        MixerFXPage& page;
        const int idx;

        std::unique_ptr<juce::Label> titleLabel;
        std::unique_ptr<juce::ToggleButton> enableBtn;
        std::unique_ptr<juce::ComboBox> fxCombo;

        std::unique_ptr<ModRingKnob> p1Knob, p2Knob, p3Knob, p4Knob;
        std::unique_ptr<ModRingKnob> returnKnob, panKnob;
    };

    void timerCallback() override;

    Forge64Processor& proc;
    ModRingKnob::Services& svcs;

    std::unique_ptr<juce::Viewport> viewport;
    class Content;
    std::unique_ptr<Content> content;

    std::array<std::unique_ptr<AuxStrip>, 4> auxStrips;

    // Master bus section labels & toggle buttons
    std::unique_ptr<juce::Label> masterTitle;
    std::unique_ptr<juce::Label> compSectionTitle;
    std::unique_ptr<juce::Label> eqSectionTitle;
    std::unique_ptr<juce::Label> outSectionTitle;

    // VCA Compressor
    std::unique_ptr<juce::ToggleButton> masterCompEnable;
    std::unique_ptr<CompGRMeter> compGRMeter;
    std::unique_ptr<ModRingKnob> compThreshKnob, compRatioKnob, compAtkKnob, compRelKnob, compGainKnob;

    // 4-Band Master EQ
    std::unique_ptr<juce::ToggleButton> masterEqEnable;
    std::unique_ptr<MasterEQGraphView> eqGraph;
    std::unique_ptr<ModRingKnob> eqLowGainKnob, eqLowMidGainKnob, eqHiMidGainKnob, eqHighGainKnob;

    // Output & Saturation
    std::unique_ptr<ModRingKnob> masterDriveKnob, masterVolKnob;
    std::unique_ptr<MasterPeakMeter> masterPeakMeter;
};

} // namespace f64
