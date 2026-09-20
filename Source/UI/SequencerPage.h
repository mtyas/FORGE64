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
};

} // namespace f64
