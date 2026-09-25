#pragma once
#include <JuceHeader.h>
#include "../Engine/StepSequencer.h"
#include "UICommon.h"
#include <memory>
#include <vector>

namespace f64 {

class Forge64Processor;

class SequencerPage : public juce::Component, private juce::Timer
{
public:
    SequencerPage(Forge64Processor& processor);
    ~SequencerPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    enum StepLockViewMode
    {
        LOCK_VIEW_VELOCITY = 0,
        LOCK_VIEW_PROBABILITY,
        LOCK_VIEW_MICROTIMING,
        LOCK_VIEW_PITCH,
        LOCK_VIEW_DECAY,
        LOCK_VIEW_DRIVE,
        LOCK_VIEW_LEVEL,
        LOCK_VIEW_PAN
    };

    void setLockViewMode(StepLockViewMode mode);
    StepLockViewMode getLockViewMode() const { return lockViewMode; }

private:
    class StepButton;
    class TrackLane;
    class PLockPopover;
    class SongBlockView;

    void timerCallback() override;
    void refreshFromSequencer();

    Forge64Processor& proc;
    StepSequencer& seq;

    // Header Controls
    std::unique_ptr<juce::TextButton> modeBtn;
    std::unique_ptr<juce::TextButton> playBtn;
    std::unique_ptr<juce::Label> tempoLabel;
    std::unique_ptr<juce::Slider> swingSlider;
    std::unique_ptr<juce::Label> swingLabel;

    std::unique_ptr<juce::Label> patternBarLabel;
    std::array<std::unique_ptr<juce::TextButton>, 16> patternBtns;
    std::unique_ptr<juce::ComboBox> presetCombo;
    std::unique_ptr<juce::TextButton> copyBtn, pasteBtn, clearBtn, randBtn;
    std::array<std::unique_ptr<juce::TextButton>, 4> pageBtns;

    // Lock View & Quick Edit mode controls
    StepLockViewMode lockViewMode = LOCK_VIEW_VELOCITY;
    std::unique_ptr<juce::Label> lockModeLabel;
    std::unique_ptr<juce::TextButton> velLockBtn;
    std::unique_ptr<juce::TextButton> probLockBtn;
    std::unique_ptr<juce::TextButton> microLockBtn;
    std::unique_ptr<juce::TextButton> pitchLockBtn;
    std::unique_ptr<juce::TextButton> decayLockBtn;
    std::unique_ptr<juce::TextButton> driveLockBtn;
    std::unique_ptr<juce::TextButton> levelLockBtn;
    std::unique_ptr<juce::TextButton> panLockBtn;

    // Pattern Mode components
    std::unique_ptr<juce::Component> patternContainer;
    std::array<std::unique_ptr<TrackLane>, 8> trackLanes;

    // Song Mode components
    std::unique_ptr<juce::Component> songContainer;
    std::unique_ptr<juce::TextButton> addSongBlockBtn, clearSongBtn;
    std::vector<std::unique_ptr<SongBlockView>> songBlockViews;
    std::unique_ptr<juce::Viewport> songViewport;

    // P-Lock Popover
    std::unique_ptr<PLockPopover> pLockPopover;
    int popoverTrack = -1, popoverStep = -1;

    class CompactButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
        {
            return uiFont(juce::jlimit(8.0f, 10.5f, (float) buttonHeight * 0.44f), true);
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
        {
            auto font = getTextButtonFont(button, button.getHeight());
            g.setFont(font);
            g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
            g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        }
    };

    CompactButtonLookAndFeel compactBtnLnF;
};

} // namespace f64
