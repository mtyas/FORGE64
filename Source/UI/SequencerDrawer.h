#pragma once
#include <JuceHeader.h>
#include "../Engine/StepSequencer.h"
#include "../Engine/PadDefs.h"
#include <functional>

namespace f64 {

class Forge64Processor;

class SequencerDrawer : public juce::Component,
                        private juce::Timer
{
public:
    SequencerDrawer(Forge64Processor& processor);
    ~SequencerDrawer() override;

    bool isUnfolded() const { return unfolded; }
    void setUnfolded(bool shouldUnfold);
    int  getDesiredHeight() const { return unfolded ? 196 : 30; }

    void setTrack(int trackIdx);
    void updateTrackInfo();

    std::function<void(bool unfolded)> onFoldStateChanged;
    std::function<void(int trackIdx, int stepIdx, int padIdx)> onStepClicked;
    std::function<int()> getActivePad;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    class TrackButton : public juce::Component
    {
    public:
        TrackButton(SequencerDrawer& drawer, int track);
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;

        int trackIndex;

    private:
        SequencerDrawer& owner;
        bool isHovered = false;
    };

    class StepButton : public juce::Component,
                       public juce::DragAndDropTarget
    {
    public:
        StepButton(SequencerDrawer& drawer, int step);
        ~StepButton() override = default;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragEnter(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;

        int stepIndex;

    private:
        SequencerDrawer& owner;
        bool isHoveredTarget = false;
    };

    Forge64Processor& proc;
    StepSequencer& seq;
    bool unfolded = false;

    // Header bar controls
    std::unique_ptr<juce::TextButton> foldToggleBtn;
    std::unique_ptr<juce::TextButton> playBtn;
    std::unique_ptr<juce::TextButton> stopBtn;
    std::unique_ptr<juce::Label> tempoLabel;
    std::unique_ptr<juce::ComboBox> patternCombo;
    std::unique_ptr<juce::TextButton> clearPatternBtn;
    std::array<std::unique_ptr<TrackButton>, 8> trackBtns;
    std::unique_ptr<juce::TextButton> stepsDecBtn;
    std::unique_ptr<juce::Label> stepsCountLabel;
    std::unique_ptr<juce::TextButton> stepsIncBtn;
    std::array<std::unique_ptr<juce::TextButton>, 4> pageBtns;

    // Step buttons (16 visible for current page)
    std::array<std::unique_ptr<StepButton>, 16> stepBtns;
    std::unique_ptr<juce::Label> trackInfoLabel;
    std::unique_ptr<juce::Label> hintLabel;

    int currentTrack = 0;
    int currentPage = 0;
};

} // namespace f64
