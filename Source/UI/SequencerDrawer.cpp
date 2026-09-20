#include "SequencerDrawer.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

SequencerDrawer::SequencerDrawer(Forge64Processor& processor)
    : proc(processor), seq(processor.getSequencer())
{
    // Toggle fold/unfold button
    foldToggleBtn = std::make_unique<juce::TextButton>("[^] SEQUENCER DRAWER");
    ui::styleButton(*foldToggleBtn);
    foldToggleBtn->setColour(juce::TextButton::buttonColourId, ui::panelHi());
    foldToggleBtn->onClick = [this] { setUnfolded(! unfolded); };
    addAndMakeVisible(foldToggleBtn.get());

    playBtn = std::make_unique<juce::TextButton>("PLAY");
    ui::styleButton(*playBtn);
    playBtn->onClick = [this]
    {
        seq.setPlaying(! seq.isPlaying());
        playBtn->setToggleState(seq.isPlaying(), juce::dontSendNotification);
    };
    addAndMakeVisible(playBtn.get());

    stopBtn = std::make_unique<juce::TextButton>("STOP");
    ui::styleButton(*stopBtn);
    stopBtn->onClick = [this]
    {
        seq.setPlaying(false);
        playBtn->setToggleState(false, juce::dontSendNotification);
    };
    addAndMakeVisible(stopBtn.get());

    tempoLabel = ui::makeLabel("120 BPM", 10.f, ui::dim());
    addAndMakeVisible(tempoLabel.get());

    patternCombo = std::make_unique<juce::ComboBox>();
    ui::styleCombo(*patternCombo);
    for (int i = 1; i <= 16; ++i)
        patternCombo->addItem("Pattern " + juce::String(i), i);
    patternCombo->setSelectedId(1, juce::dontSendNotification);
    patternCombo->onChange = [this]
    {
        seq.setSelectedPattern(patternCombo->getSelectedId() - 1);
        updateTrackInfo();
        repaint();
    };
    addAndMakeVisible(patternCombo.get());

    clearPatternBtn = std::make_unique<juce::TextButton>("[CLEAR]");
    ui::styleButton(*clearPatternBtn);
    clearPatternBtn->setTooltip("Clear all steps across all 8 tracks in current pattern");
    clearPatternBtn->onClick = [this]
    {
        seq.clearCurrentPattern();
        updateTrackInfo();
        repaint();
    };
    addAndMakeVisible(clearPatternBtn.get());

    for (int t = 0; t < 8; ++t)
    {
        trackBtns[(size_t) t] = std::make_unique<TrackButton>(*this, t);
        addAndMakeVisible(trackBtns[(size_t) t].get());
    }

class ClickableStepsLabel : public juce::Label
{
public:
    ClickableStepsLabel(SequencerDrawer& d) : drawer(d) {}
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        auto& track = drawer.seq.currentPattern().tracks[(size_t) drawer.currentTrack];
        const int delta = (wheel.deltaY > 0.f ? 1 : -1);
        track.stepCount = juce::jlimit(1, 64, track.stepCount + delta);
        drawer.updateTrackInfo();
        drawer.repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
        {
            juce::PopupMenu m;
            static const int lengths[] = { 8, 12, 16, 24, 32, 48, 64 };
            for (int l : lengths)
                m.addItem(l, juce::String(l) + " Steps");
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                            [this](int res)
                            {
                                if (res > 0)
                                {
                                    auto& track = drawer.seq.currentPattern().tracks[(size_t) drawer.currentTrack];
                                    track.stepCount = res;
                                    drawer.updateTrackInfo();
                                    drawer.repaint();
                                }
                            });
        }
        else
        {
            auto& track = drawer.seq.currentPattern().tracks[(size_t) drawer.currentTrack];
            int next = 16;
            if (track.stepCount < 16) next = 16;
            else if (track.stepCount < 32) next = 32;
            else if (track.stepCount < 48) next = 48;
            else if (track.stepCount < 64) next = 64;
            else next = 16;
            track.stepCount = next;
            drawer.updateTrackInfo();
            drawer.repaint();
        }
    }
private:
    SequencerDrawer& drawer;
};

    stepsDecBtn = std::make_unique<juce::TextButton>("-");
    ui::styleButton(*stepsDecBtn);
    stepsDecBtn->setTooltip("Decrease track length by 1 step");
    stepsDecBtn->onClick = [this]
    {
        auto& track = seq.currentPattern().tracks[(size_t) currentTrack];
        track.stepCount = juce::jlimit(1, 64, track.stepCount - 1);
        updateTrackInfo();
        repaint();
    };
    addAndMakeVisible(stepsDecBtn.get());

    stepsCountLabel = std::make_unique<ClickableStepsLabel>(*this);
    stepsCountLabel->setText("16 STEPS", juce::dontSendNotification);
    stepsCountLabel->setFont(uiFont(10.5f, true));
    stepsCountLabel->setColour(juce::Label::textColourId, ui::accentHot());
    stepsCountLabel->setJustificationType(juce::Justification::centred);
    stepsCountLabel->setTooltip("Track length (1-64 steps). Click or scroll to change");
    addAndMakeVisible(stepsCountLabel.get());

    stepsIncBtn = std::make_unique<juce::TextButton>("+");
    ui::styleButton(*stepsIncBtn);
    stepsIncBtn->setTooltip("Increase track length by 1 step");
    stepsIncBtn->onClick = [this]
    {
        auto& track = seq.currentPattern().tracks[(size_t) currentTrack];
        track.stepCount = juce::jlimit(1, 64, track.stepCount + 1);
        updateTrackInfo();
        repaint();
    };
    addAndMakeVisible(stepsIncBtn.get());

    for (int p = 0; p < 4; ++p)
    {
        pageBtns[(size_t) p] = std::make_unique<juce::TextButton>(juce::String(p * 16 + 1) + "-" + juce::String((p + 1) * 16));
        ui::styleButton(*pageBtns[(size_t) p]);
        pageBtns[(size_t) p]->onClick = [this, p]
        {
            currentPage = p;
            for (int i = 0; i < 4; ++i)
                pageBtns[(size_t) i]->setToggleState(i == p, juce::dontSendNotification);
            for (int s = 0; s < 16; ++s)
                stepBtns[(size_t) s]->stepIndex = currentPage * 16 + s;
            repaint();
        };
        addAndMakeVisible(pageBtns[(size_t) p].get());
    }
    pageBtns[0]->setToggleState(true, juce::dontSendNotification);

    for (int s = 0; s < 16; ++s)
    {
        stepBtns[(size_t) s] = std::make_unique<StepButton>(*this, s);
        addAndMakeVisible(stepBtns[(size_t) s].get());
    }

    trackInfoLabel = ui::makeLabel("Track 1: Kick", 11.f, ui::accentHot());
    trackInfoLabel->setFont(uiFont(11.f, true));
    addAndMakeVisible(trackInfoLabel.get());

    hintLabel = ui::makeLabel("Drag step to copy | Shift+drag or scroll for velocity | Click empty step to insert sound | Click to edit P-Locks", 10.f, ui::dim());
    addAndMakeVisible(hintLabel.get());

    updateTrackInfo();
    startTimerHz(30);
}

SequencerDrawer::~SequencerDrawer()
{
    stopTimer();
}

void SequencerDrawer::setTrack(int t)
{
    currentTrack = juce::jlimit(0, 7, t);
    updateTrackInfo();
    for (auto& tb : trackBtns)
        tb->repaint();
    repaint();
}

void SequencerDrawer::updateTrackInfo()
{
    const auto& track = seq.currentPattern().tracks[(size_t) currentTrack];
    const int defPad = track.defaultPad;
    const char bc = (char) ('A' + (defPad / 16));
    const juce::String coord = juce::String::charToString(bc) + juce::String::formatted("%02d", (defPad % 16) + 1);
    if (trackInfoLabel != nullptr)
        trackInfoLabel->setText("TRACK " + juce::String(currentTrack + 1) + " (DEFAULT: PAD " + coord + ") // "
                                + juce::String(track.stepCount) + " STEPS", juce::dontSendNotification);

    if (stepsCountLabel != nullptr)
    {
        stepsCountLabel->setText(juce::String(track.stepCount) + " STEPS", juce::dontSendNotification);
    }
}

void SequencerDrawer::setUnfolded(bool shouldUnfold)
{
    unfolded = shouldUnfold;
    foldToggleBtn->setButtonText(unfolded ? "[v] COLLAPSE SEQUENCER" : "[^] SEQUENCER DRAWER");
    if (onFoldStateChanged)
        onFoldStateChanged(unfolded);
}

void SequencerDrawer::timerCallback()
{
    if (unfolded)
    {
        const int playhead = seq.getTrackPlayhead(currentTrack);
        for (int s = 0; s < 16; ++s)
        {
            const int globalStep = currentPage * 16 + s;
            const bool isPlayhead = (seq.isPlaying() && playhead == globalStep);
            // Quick repaint if playhead moved
            stepBtns[(size_t) s]->repaint();
        }
    }
}

void SequencerDrawer::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // Dark forged chassis background
    g.fillAll(juce::Colour(0xFF130E0D));

    // Top molten border line
    g.setColour(ui::accent().withAlpha(0.7f));
    g.drawHorizontalLine(0, 0.f, (float) getWidth());
    g.setColour(ui::line());
    g.drawHorizontalLine(1, 0.f, (float) getWidth());

    if (unfolded)
    {
        ui::drawForgedPlate(g, r.reduced(3.f, 3.f), 6.f, true);
        // Subtle divider separating top header controls & track info from the step buttons
        g.setColour(ui::line().withAlpha(0.35f));
        g.drawHorizontalLine(55, 8.f, (float) getWidth() - 8.f);
    }
}

void SequencerDrawer::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    const int topY = unfolded ? 8 : 3;
    const int btnH = 24;

    // Top control bar
    foldToggleBtn->setBounds(8, topY, 136, btnH);
    playBtn->setBounds(148, topY, 40, btnH);
    stopBtn->setBounds(192, topY, 40, btnH);
    tempoLabel->setBounds(236, topY + 1, 50, btnH - 2);
    patternCombo->setBounds(290, topY, 84, btnH);
    if (clearPatternBtn)
        clearPatternBtn->setBounds(378, topY, 52, btnH);

    int tx = 436;
    for (int t = 0; t < 8; ++t)
    {
        trackBtns[(size_t) t]->setBounds(tx, topY, 27, btnH);
        tx += 29;
    }
    tx += 4;
    stepsDecBtn->setBounds(tx, topY, 18, btnH);
    tx += 20;
    stepsCountLabel->setBounds(tx, topY, 62, btnH);
    tx += 64;
    stepsIncBtn->setBounds(tx, topY, 18, btnH);

    int px = w - 8 - 4 * 38;
    for (int p = 0; p < 4; ++p)
    {
        pageBtns[(size_t) p]->setBounds(px, topY, 36, btnH);
        px += 38;
    }

    if (! unfolded)
    {
        for (auto& sb : stepBtns) sb->setVisible(false);
        trackInfoLabel->setVisible(false);
        hintLabel->setVisible(false);
        return;
    }

    trackInfoLabel->setVisible(true);
    hintLabel->setVisible(true);
    updateTrackInfo();

    trackInfoLabel->setBounds(12, 36, 260, 16);
    hintLabel->setBounds(276, 36, w - 286, 16);

    // 16 Step Buttons
    const int stepAreaY = 60;
    const int stepAreaH = h - stepAreaY - 8;
    const int stepMargin = 10;
    const int stepW = (w - stepMargin * 2) / 16;
    for (int s = 0; s < 16; ++s)
    {
        stepBtns[(size_t) s]->setVisible(true);
        stepBtns[(size_t) s]->setBounds(stepMargin + s * stepW, stepAreaY, stepW - 3, stepAreaH);
    }
}

// ---------------------------------------------------------------------------
// TrackButton Implementation
// ---------------------------------------------------------------------------
SequencerDrawer::TrackButton::TrackButton(SequencerDrawer& d, int track)
    : trackIndex(track), owner(d)
{
}

void SequencerDrawer::TrackButton::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.f);
    const bool isSelected = (owner.currentTrack == trackIndex);

    if (isSelected)
    {
        g.setColour(ui::accentHot().withAlpha(0.35f));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(ui::accentHot());
        g.drawRoundedRectangle(r, 4.f, 1.6f);
    }
    else
    {
        g.setColour(isHovered ? ui::panelHi() : ui::panel());
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(ui::line().withAlpha(0.6f));
        g.drawRoundedRectangle(r, 4.f, 1.0f);
    }

    g.setFont(uiFont(11.f, true));
    g.setColour(isSelected ? ui::accentHot() : (isHovered ? ui::txt() : ui::dim()));
    g.drawText("T" + juce::String(trackIndex + 1), r, juce::Justification::centred);
}

void SequencerDrawer::TrackButton::mouseEnter(const juce::MouseEvent&) { isHovered = true; repaint(); }
void SequencerDrawer::TrackButton::mouseExit(const juce::MouseEvent&)  { isHovered = false; repaint(); }
void SequencerDrawer::TrackButton::mouseDown(const juce::MouseEvent&)  { owner.setTrack(trackIndex); }

// ---------------------------------------------------------------------------
// StepButton Implementation
// ---------------------------------------------------------------------------
SequencerDrawer::StepButton::StepButton(SequencerDrawer& d, int s)
    : stepIndex(s), owner(d)
{
}

void SequencerDrawer::StepButton::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.f);
    const auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
    const auto& step = track.steps[(size_t) stepIndex];

    const int playhead = owner.seq.getTrackPlayhead(owner.currentTrack);
    const bool isCurrentPlayhead = (owner.seq.isPlaying() && playhead == stepIndex);

    const int padNum = (step.padOverride >= 0) ? step.padOverride : track.defaultPad;
    const char bankChar = (char) ('A' + (padNum / 16));
    const juce::String padCoord = juce::String::charToString(bankChar) + juce::String::formatted("%02d", (padNum % 16) + 1);

    // Background
    if (isHoveredTarget)
    {
        g.setColour(ui::accentHot().withAlpha(0.4f));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(ui::accentHot());
        g.drawRoundedRectangle(r, 4.f, 2.0f);
    }
    else if (step.active)
    {
        // Active molten step
        const juce::Colour activeCol = (stepIndex % 4 == 0) ? ui::accent() : ui::accent().darker(0.2f);
        g.setColour(activeCol.withAlpha(0.25f));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(activeCol);
        g.drawRoundedRectangle(r, 4.f, 1.2f);
    }
    else
    {
        // Inactive step
        const juce::Colour bgCol = (stepIndex % 4 == 0) ? juce::Colour(0xFF1E1614) : juce::Colour(0xFF171110);
        g.setColour(bgCol);
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(ui::line().withAlpha(0.4f));
        g.drawRoundedRectangle(r, 4.f, 0.8f);
    }

    // Playhead strike
    if (isCurrentPlayhead)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillRoundedRectangle(r.reduced(2.f), 3.f);
    }

    // Text: Step number and Pad Coordinate
    g.setFont(uiFont(9.f, true));
    g.setColour(step.active ? ui::accentHot() : ui::dim());
    g.drawText(juce::String(stepIndex + 1), r.removeFromTop(16.f), juce::Justification::centred);

    if (step.active)
    {
        g.setFont(uiFont(11.f, true));
        g.setColour(ui::txt());
        g.drawText(padCoord, r.removeFromTop(20.f), juce::Justification::centred);

        // P-Lock badge
        if (step.hasLocks)
        {
            auto lockRc = r.removeFromTop(14.f).reduced(4.f, 1.f);
            g.setColour(ui::accentHot().withAlpha(0.25f));
            g.fillRoundedRectangle(lockRc, 2.f);
            g.setColour(ui::accentHot());
            g.drawRoundedRectangle(lockRc, 2.f, 0.8f);
            g.setFont(uiFont(8.f, true));
            g.drawText("LOCK", lockRc, juce::Justification::centred);
        }

        // Velocity bar at bottom
        auto velRc = r.removeFromBottom(12.f).reduced(2.f, 1.f);
        g.setColour(ui::line().withAlpha(0.3f));
        g.fillRoundedRectangle(velRc, 2.f);
        auto fillRc = velRc.withWidth(velRc.getWidth() * juce::jlimit(0.05f, 1.0f, step.velocity));
        juce::ColourGradient vGrad(ui::accentHot(), fillRc.getX(), fillRc.getY(),
                                  ui::accent(), fillRc.getRight(), fillRc.getY(), false);
        g.setGradientFill(vGrad);
        g.fillRoundedRectangle(fillRc, 2.f);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine((int) fillRc.getRight(), fillRc.getY(), fillRc.getBottom());
    }
}

void SequencerDrawer::StepButton::mouseDown(const juce::MouseEvent& e)
{
    auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
    auto& step = track.steps[(size_t) stepIndex];

    if (e.mods.isPopupMenu())
    {
        step.active = ! step.active;
        repaint();
        return;
    }

    if (! step.active)
    {
        step.active = true;
        step.velocity = 0.85f;
        const int curPad = owner.getActivePad ? owner.getActivePad() : track.defaultPad;
        if (curPad >= 0 && curPad < kNumPads)
            step.padOverride = curPad;
        repaint();
    }

    // Left click always opens Pad panel in P-Lock edit mode and auditions the step!
    const int targetPad = (step.padOverride >= 0) ? step.padOverride : track.defaultPad;
    if (owner.onStepClicked)
        owner.onStepClicked(owner.currentTrack, stepIndex, targetPad);
}

void SequencerDrawer::StepButton::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isShiftDown())
    {
        auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
        auto& step = track.steps[(size_t) stepIndex];
        if (step.active)
        {
            const float delta = -(float) e.getDistanceFromDragStartY() / 100.0f;
            step.velocity = juce::jlimit(0.05f, 1.0f, step.velocity + delta * 0.05f);
            repaint();
        }
        return;
    }

    if (e.getDistanceFromDragStart() > 6)
    {
        if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            if (! dragContainer->isDragAndDropActive())
            {
                auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
                const auto& step = track.steps[(size_t) stepIndex];
                if (step.active)
                {
                    const juce::String desc = "f64step:" + juce::String(owner.currentTrack) + ":" + juce::String(stepIndex);
                    auto snap = createComponentSnapshot(getLocalBounds(), false, 1.0f);
                    dragContainer->startDragging(desc, this, snap, true);
                }
            }
        }
    }
}

void SequencerDrawer::StepButton::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
    auto& step = track.steps[(size_t) stepIndex];
    if (step.active)
    {
        const float delta = (wheel.deltaY > 0.f ? 0.05f : -0.05f);
        step.velocity = juce::jlimit(0.05f, 1.0f, step.velocity + delta);
        repaint();
    }
}

bool SequencerDrawer::StepButton::isInterestedInDragSource(const SourceDetails& details)
{
    const auto d = details.description.toString();
    return d.startsWith("f64pad:") || d.startsWith("f64step:");
}

void SequencerDrawer::StepButton::itemDragEnter(const SourceDetails&)
{
    isHoveredTarget = true;
    repaint();
}

void SequencerDrawer::StepButton::itemDragExit(const SourceDetails&)
{
    isHoveredTarget = false;
    repaint();
}

void SequencerDrawer::StepButton::itemDropped(const SourceDetails& details)
{
    isHoveredTarget = false;
    const auto d = details.description.toString();
    if (d.startsWith("f64pad:"))
    {
        const int padNumber = d.fromFirstOccurrenceOf("f64pad:", false, false).getIntValue();
        auto& track = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
        auto& step = track.steps[(size_t) stepIndex];
        step.padOverride = padNumber;
        step.active = true;
        owner.repaint();

        // Audition the assigned pad without switching page away from current mode
        const int targetPad = (step.padOverride >= 0) ? step.padOverride : track.defaultPad;
        owner.proc.triggerAudition(targetPad, step.velocity > 0.05f ? step.velocity : 0.85f);
    }
    else if (d.startsWith("f64step:"))
    {
        auto rest = d.fromFirstOccurrenceOf("f64step:", false, false);
        auto parts = juce::StringArray::fromTokens(rest, ":", "");
        if (parts.size() >= 2)
        {
            const int srcTrack = parts[0].getIntValue();
            const int srcStep  = parts[1].getIntValue();
            if (srcTrack >= 0 && srcTrack < 8 && srcStep >= 0 && srcStep < 64)
            {
                const auto srcStepData = owner.seq.currentPattern().tracks[(size_t) srcTrack].steps[(size_t) srcStep];
                auto& dstTrack = owner.seq.currentPattern().tracks[(size_t) owner.currentTrack];
                dstTrack.steps[(size_t) stepIndex] = srcStepData;
                owner.repaint();

                // Audition the copied step without forcing page switch
                const int targetPad = (srcStepData.padOverride >= 0) ? srcStepData.padOverride : dstTrack.defaultPad;
                if (srcStepData.hasLocks)
                    owner.proc.triggerStepAudition(targetPad, srcStepData.velocity > 0.05f ? srcStepData.velocity : 0.85f, srcStepData);
                else
                    owner.proc.triggerAudition(targetPad, srcStepData.velocity > 0.05f ? srcStepData.velocity : 0.85f);
            }
        }
    }
}

} // namespace f64
