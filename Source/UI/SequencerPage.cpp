#include "SequencerPage.h"
#include "../PluginProcessor.h"
#include "../Presets/SequencePresetManager.h"

namespace f64 {

// ---------------------------------------------------------------------------
// StepButton
// ---------------------------------------------------------------------------
class SequencerPage::StepButton : public juce::Component
{
public:
    StepButton(SequencerPage& owner_, int trackIdx_, int stepIdx_)
        : owner(owner_), trackIdx(trackIdx_), stepIdx(stepIdx_) {}

    void setStepIndex(int newStepIdx)
    {
        if (stepIdx != newStepIdx)
        {
            stepIdx = newStepIdx;
            repaint();
        }
    }

    int getStepIndex() const { return stepIdx; }

    void paint(juce::Graphics& g) override
    {
        const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
        const bool isInRange = (stepIdx < trk.stepCount);
        const auto& s = trk.steps[(size_t) stepIdx];

        auto r = getLocalBounds().toFloat().reduced(1.5f);

        if (! isInRange)
        {
            // Dimmed out of track polymetric length
            g.setColour(ui::bg().darker(0.3f));
            g.fillRoundedRectangle(r, 3.f);
            g.setColour(ui::line().withAlpha(0.2f));
            g.drawRoundedRectangle(r, 3.f, 1.f);
            return;
        }

        const bool isCurStep = (owner.seq.isPlaying() && owner.seq.getTrackPlayhead(trackIdx) == stepIdx);

        if (s.active)
        {
            g.setColour(ui::panel());
            g.fillRoundedRectangle(r, 3.f);

            if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_VELOCITY)
            {
                // Active Step (Velocity): Molten fire orange fill
                const float vH = r.getHeight() * clampRange(s.velocity, 0.08f, 1.0f);
                auto barR = r.withTrimmedTop(r.getHeight() - vH);

                juce::ColourGradient grad(ui::accentHot(), barR.getX(), barR.getY(),
                                          ui::accent(), barR.getX(), barR.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(barR, 3.f);

                g.setColour(juce::Colours::white.withAlpha(0.6f));
                g.drawHorizontalLine((int) barR.getY(), barR.getX() + 1.f, barR.getRight() - 1.f);

                g.setColour(ui::accentHot().withAlpha(0.8f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_PROBABILITY)
            {
                // Active Step (Probability): Bright Neon Mint / Emerald Teal fill
                const float pH = r.getHeight() * clampRange(s.probability, 0.08f, 1.0f);
                auto barR = r.withTrimmedTop(r.getHeight() - pH);

                const juce::Colour probHi(0xFF00F5D4);
                const juce::Colour probLo(0xFF00897B);
                juce::ColourGradient grad(probHi, barR.getX(), barR.getY(),
                                          probLo, barR.getX(), barR.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(barR, 3.f);

                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawHorizontalLine((int) barR.getY(), barR.getX() + 1.f, barR.getRight() - 1.f);

                g.setColour(probHi.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);

                if (s.probability < 0.99f)
                {
                    g.setFont(uiFont(7.5f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText(juce::String((int) std::round(s.probability * 100.f)) + "%",
                               r.reduced(1.f), juce::Justification::centred);
                }
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_MICROTIMING)
            {
                // Active Step (Microtiming): Vivid Electric Magenta / Purple offset
                const juce::Colour microCol(0xFFCC55FF);
                const float centerY = r.getCentreY();

                // Center reference gridline (0.0 offset)
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawHorizontalLine((int) centerY, r.getX() + 2.f, r.getRight() - 2.f);

                const float maxHalfH = r.getHeight() * 0.44f;
                const float mOffset = juce::jlimit(-0.5f, 0.5f, s.microtiming);

                if (std::abs(mOffset) < 0.015f)
                {
                    // Perfectly on grid
                    g.setColour(microCol);
                    g.fillRoundedRectangle(r.getX() + 2.f, centerY - 2.f, r.getWidth() - 4.f, 4.f, 1.5f);
                }
                else if (mOffset > 0.f)
                {
                    // Late (delayed): bar upwards from center
                    const float bH = (mOffset / 0.5f) * maxHalfH;
                    auto barR = juce::Rectangle<float>(r.getX() + 2.f, centerY - bH, r.getWidth() - 4.f, bH);
                    juce::ColourGradient grad(microCol.brighter(0.2f), barR.getX(), barR.getY(),
                                              microCol, barR.getX(), barR.getBottom(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText("+" + juce::String((int) std::round(mOffset * 100.f)) + "%",
                               r.reduced(1.f), juce::Justification::centredTop);
                }
                else
                {
                    // Early (rushed): bar downwards from center
                    const float bH = (-mOffset / 0.5f) * maxHalfH;
                    auto barR = juce::Rectangle<float>(r.getX() + 2.f, centerY, r.getWidth() - 4.f, bH);
                    juce::ColourGradient grad(microCol, barR.getX(), barR.getY(),
                                              microCol.darker(0.3f), barR.getX(), barR.getBottom(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText(juce::String((int) std::round(mOffset * 100.f)) + "%",
                               r.reduced(1.f), juce::Justification::centredBottom);
                }

                g.setColour(microCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_PITCH)
            {
                // Active Step (Pitch): Electric Neon Blue
                const juce::Colour pitchCol(0xFF2979FF);
                const float centerY = r.getCentreY();
                const float maxHalfH = r.getHeight() * 0.44f;
                const float pOffset = juce::jlimit(-24.0f, 24.0f, s.pLockPitch);

                // Center reference line (0st)
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawHorizontalLine((int) centerY, r.getX() + 2.f, r.getRight() - 2.f);

                if (std::abs(pOffset) < 0.2f)
                {
                    g.setColour(pitchCol);
                    g.fillRoundedRectangle(r.getX() + 2.f, centerY - 2.f, r.getWidth() - 4.f, 4.f, 1.5f);
                }
                else if (pOffset > 0.f)
                {
                    const float bH = (pOffset / 24.0f) * maxHalfH;
                    auto barR = juce::Rectangle<float>(r.getX() + 2.f, centerY - bH, r.getWidth() - 4.f, bH);
                    juce::ColourGradient grad(pitchCol.brighter(0.25f), barR.getX(), barR.getY(),
                                              pitchCol, barR.getX(), barR.getBottom(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText("+" + juce::String((int) std::round(pOffset)),
                               r.reduced(1.f), juce::Justification::centredTop);
                }
                else
                {
                    const float bH = (-pOffset / 24.0f) * maxHalfH;
                    auto barR = juce::Rectangle<float>(r.getX() + 2.f, centerY, r.getWidth() - 4.f, bH);
                    juce::ColourGradient grad(pitchCol, barR.getX(), barR.getY(),
                                              pitchCol.darker(0.35f), barR.getX(), barR.getBottom(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText(juce::String((int) std::round(pOffset)),
                               r.reduced(1.f), juce::Justification::centredBottom);
                }

                g.setColour(pitchCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_DECAY)
            {
                // Active Step (Decay): Neon Emerald Green
                const juce::Colour decayCol(0xFF00E676);
                const float dNorm = clampRange((s.pLockDecay - 0.05f) / 4.95f, 0.08f, 1.0f);
                const float dH = r.getHeight() * dNorm;
                auto barR = r.withTrimmedTop(r.getHeight() - dH);

                juce::ColourGradient grad(decayCol.brighter(0.2f), barR.getX(), barR.getY(),
                                          decayCol.darker(0.3f), barR.getX(), barR.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(barR, 3.f);

                g.setColour(juce::Colours::white.withAlpha(0.6f));
                g.drawHorizontalLine((int) barR.getY(), barR.getX() + 1.f, barR.getRight() - 1.f);

                g.setColour(decayCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);

                g.setFont(uiFont(7.5f, true));
                g.setColour(juce::Colours::white);
                g.drawText(juce::String(s.pLockDecay, 1) + "x", r.reduced(1.f), juce::Justification::centred);
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_DRIVE)
            {
                // Active Step (Drive): Crimson Flame
                const juce::Colour driveCol(0xFFFF3D00);
                const float drH = r.getHeight() * clampRange(s.pLockDrive, 0.08f, 1.0f);
                auto barR = r.withTrimmedTop(r.getHeight() - drH);

                juce::ColourGradient grad(driveCol.brighter(0.2f), barR.getX(), barR.getY(),
                                          driveCol.darker(0.4f), barR.getX(), barR.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(barR, 3.f);

                g.setColour(juce::Colours::white.withAlpha(0.6f));
                g.drawHorizontalLine((int) barR.getY(), barR.getX() + 1.f, barR.getRight() - 1.f);

                g.setColour(driveCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);

                if (s.pLockDrive > 0.01f)
                {
                    g.setFont(uiFont(7.5f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText(juce::String((int) std::round(s.pLockDrive * 100.f)) + "%",
                               r.reduced(1.f), juce::Justification::centred);
                }
            }
            else if (owner.getLockViewMode() == SequencerPage::LOCK_VIEW_LEVEL)
            {
                // Active Step (Level): Radiant Gold / Cyber Amber
                const juce::Colour levCol(0xFFFFD600);
                const float levNorm = clampRange(s.pLockLevel / 1.5f, 0.08f, 1.0f);
                const float lH = r.getHeight() * levNorm;
                auto barR = r.withTrimmedTop(r.getHeight() - lH);

                juce::ColourGradient grad(levCol.brighter(0.2f), barR.getX(), barR.getY(),
                                          levCol.darker(0.35f), barR.getX(), barR.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(barR, 3.f);

                g.setColour(juce::Colours::white.withAlpha(0.6f));
                g.drawHorizontalLine((int) barR.getY(), barR.getX() + 1.f, barR.getRight() - 1.f);

                g.setColour(levCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);

                g.setFont(uiFont(7.5f, true));
                g.setColour(juce::Colours::white);
                g.drawText(juce::String((int) std::round(s.pLockLevel * 100.f)) + "%",
                           r.reduced(1.f), juce::Justification::centred);
            }
            else // LOCK_VIEW_PAN
            {
                // Active Step (Pan): Violet Orchid / Electric Fuchsia
                const juce::Colour panCol(0xFFE040FB);
                const float centerX = r.getCentreX();

                // Vertical center reference line
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawVerticalLine((int) centerX, r.getY() + 2.f, r.getBottom() - 2.f);

                const float maxHalfW = r.getWidth() * 0.44f;
                const float panVal = juce::jlimit(-1.0f, 1.0f, s.pLockPan);

                if (std::abs(panVal) < 0.05f)
                {
                    g.setColour(panCol);
                    g.fillRoundedRectangle(centerX - 2.f, r.getY() + 2.f, 4.f, r.getHeight() - 4.f, 1.5f);

                    g.setFont(uiFont(7.5f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText("C", r.reduced(1.f), juce::Justification::centred);
                }
                else if (panVal < 0.f)
                {
                    // Left
                    const float bW = (-panVal) * maxHalfW;
                    auto barR = juce::Rectangle<float>(centerX - bW, r.getY() + 2.f, bW, r.getHeight() - 4.f);
                    juce::ColourGradient grad(panCol.brighter(0.2f), barR.getX(), barR.getY(),
                                              panCol, barR.getRight(), barR.getY(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText("L" + juce::String((int) std::round(-panVal * 100.f)),
                               r.reduced(1.f), juce::Justification::centred);
                }
                else
                {
                    // Right
                    const float bW = panVal * maxHalfW;
                    auto barR = juce::Rectangle<float>(centerX, r.getY() + 2.f, bW, r.getHeight() - 4.f);
                    juce::ColourGradient grad(panCol, barR.getX(), barR.getY(),
                                              panCol.brighter(0.2f), barR.getRight(), barR.getY(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(barR, 2.f);

                    g.setFont(uiFont(7.f, true));
                    g.setColour(juce::Colours::white);
                    g.drawText("R" + juce::String((int) std::round(panVal * 100.f)),
                               r.reduced(1.f), juce::Justification::centred);
                }

                g.setColour(panCol.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 3.f, 1.2f);
            }
        }
        else
        {
            // Inactive step
            const bool isBeatMarker = (stepIdx % 4 == 0);
            g.setColour(isBeatMarker ? ui::panelHi() : ui::panel());
            g.fillRoundedRectangle(r, 3.f);
            g.setColour(ui::line());
            g.drawRoundedRectangle(r, 3.f, 1.f);
        }

        // P-Lock hot ember indicator dot
        if (s.hasLocks && ((s.lockMask & ~StepLockFlags::LOCK_FLAG_PAD_OVERRIDE) != 0 || s.ratchet > 1))
        {
            g.setColour(juce::Colour(0xFFFFD166));
            g.fillEllipse(r.getRight() - 6.f, r.getY() + 3.f, 4.f, 4.f);
        }

        // Ratchet roll indicator
        if (s.ratchet > 1)
        {
            g.setFont(uiFont(7.5f, true));
            g.setColour(ui::txt());
            g.drawText(juce::String(s.ratchet) + "x", r.reduced(2.f), juce::Justification::bottomLeft);
        }

        // Playhead cursor
        if (isCurStep)
        {
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(r.expanded(0.5f), 3.f, 2.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    SequencerPage& owner;
    int trackIdx, stepIdx;
    float dragStartVal = 0.85f;
    float dragStartY = 0.f;
    float dragStartX = 0.f;
    bool wasActiveOnDown = false;
};

// ---------------------------------------------------------------------------
// P-Lock Popover
// ---------------------------------------------------------------------------
class SequencerPage::PLockPopover : public juce::Component
{
public:
    explicit PLockPopover(SequencerPage& owner_) : owner(owner_)
    {
        title = ui::makeLabel("STEP PARAMETER LOCKS", 12.f, ui::accentHot());
        title->setFont(uiFont(12.f, true));
        addAndMakeVisible(title.get());

        padCombo = std::make_unique<juce::ComboBox>();
        ui::styleCombo(*padCombo);
        padCombo->addItem("Default Track Pad", 1);
        for (int p = 0; p < kNumPads; ++p)
        {
            const int b = p / 16;
            const char bc = (char) ('A' + b);
            padCombo->addItem("Pad " + juce::String::charToString(bc) + juce::String::formatted("%02d", (p % 16) + 1), p + 2);
        }
        padCombo->onChange = [this] { applyChanges(); };
        addAndMakeVisible(padCombo.get());

        auto makeSlider = [&](std::unique_ptr<juce::Slider>& sl, std::unique_ptr<juce::Label>& lb,
                              const char* name, float minV, float maxV, float def)
        {
            sl = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            sl->setRange(minV, maxV, 0.01);
            sl->setValue(def, juce::dontSendNotification);
            sl->setColour(juce::Slider::thumbColourId, ui::accent());
            sl->setColour(juce::Slider::trackColourId, ui::accent());
            sl->setColour(juce::Slider::backgroundColourId, ui::panelHi());
            sl->setColour(juce::Slider::textBoxTextColourId, ui::txt());
            sl->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            sl->onValueChange = [this] { applyChanges(); };
            addAndMakeVisible(sl.get());

            lb = ui::makeLabel(name, 9.5f, ui::dim());
            addAndMakeVisible(lb.get());
        };

        makeSlider(velSlider, velLabel, "Velocity", 0.05f, 1.0f, 0.85f);
        makeSlider(probSlider, probLabel, "Probability", 0.0f, 1.0f, 1.0f);
        makeSlider(mtimeSlider, mtimeLabel, "Microtiming", -0.5f, 0.5f, 0.0f);
        makeSlider(pitchSlider, pitchLabel, "Pitch Lock", -24.f, 24.f, 0.0f);
        makeSlider(decaySlider, decayLabel, "Decay Lock", 0.1f, 4.0f, 1.0f);
        makeSlider(toneSlider, toneLabel, "Tone / Filter", 0.0f, 1.0f, 0.5f);
        makeSlider(driveSlider, driveLabel, "Drive Lock", 0.0f, 1.0f, 0.0f);
        makeSlider(levelSlider, levelLabel, "Level Lock", 0.0f, 1.5f, 1.0f);
        makeSlider(panSlider, panLabel, "Pan Lock", -1.0f, 1.0f, 0.0f);
        makeSlider(sendASlider, sendALabel, "Send A Lock", 0.0f, 1.0f, 0.0f);

        ratchetCombo = std::make_unique<juce::ComboBox>();
        ui::styleCombo(*ratchetCombo);
        ratchetCombo->addItem("1x (Normal)", 1);
        ratchetCombo->addItem("2x (Roll)", 2);
        ratchetCombo->addItem("3x (Triplet)", 3);
        ratchetCombo->addItem("4x (Rapid)", 4);
        ratchetCombo->onChange = [this] { applyChanges(); };
        addAndMakeVisible(ratchetCombo.get());

        clearBtn = std::make_unique<juce::TextButton>("Clear Locks");
        ui::styleButton(*clearBtn);
        clearBtn->onClick = [this]
        {
            if (curTrack >= 0 && curStep >= 0)
            {
                auto& s = owner.seq.currentPattern().tracks[(size_t) curTrack].steps[(size_t) curStep];
                s.clearLocks();
                s.ratchet = 1;
                s.microtiming = 0.f;
                s.probability = 1.f;
                openForStep(curTrack, curStep);
                owner.repaint();
            }
        };
        addAndMakeVisible(clearBtn.get());

        closeBtn = std::make_unique<juce::TextButton>("Done");
        ui::styleButton(*closeBtn);
        closeBtn->onClick = [this] { setVisible(false); };
        addAndMakeVisible(closeBtn.get());
    }

    void openForStep(int t, int s)
    {
        curTrack = t; curStep = s;
        const auto& st = owner.seq.currentPattern().tracks[(size_t) t].steps[(size_t) s];

        title->setText("TRACK " + juce::String(t + 1) + " // STEP " + juce::String(s + 1) + " LOCKS", juce::dontSendNotification);
        padCombo->setSelectedId(st.padOverride < 0 ? 1 : st.padOverride + 2, juce::dontSendNotification);
        velSlider->setValue(st.velocity, juce::dontSendNotification);
        probSlider->setValue(st.probability, juce::dontSendNotification);
        mtimeSlider->setValue(st.microtiming, juce::dontSendNotification);
        ratchetCombo->setSelectedId(st.ratchet, juce::dontSendNotification);

        pitchSlider->setValue(st.pLockPitch, juce::dontSendNotification);
        decaySlider->setValue(st.pLockDecay, juce::dontSendNotification);
        toneSlider->setValue(st.pLockTone, juce::dontSendNotification);
        driveSlider->setValue(st.pLockDrive, juce::dontSendNotification);
        levelSlider->setValue(st.pLockLevel, juce::dontSendNotification);
        panSlider->setValue(st.pLockPan, juce::dontSendNotification);
        sendASlider->setValue(st.pLockSendA, juce::dontSendNotification);

        setVisible(true);
        toFront(true);
    }

    void applyChanges()
    {
        if (curTrack < 0 || curStep < 0) return;
        auto& s = owner.seq.currentPattern().tracks[(size_t) curTrack].steps[(size_t) curStep];
        s.active = true;
        const int pId = padCombo->getSelectedId();
        s.padOverride = (pId <= 1) ? -1 : (pId - 2);
        s.velocity = (float) velSlider->getValue();
        s.probability = (float) probSlider->getValue();
        s.microtiming = (float) mtimeSlider->getValue();
        s.ratchet = ratchetCombo->getSelectedId();

        s.pLockPitch = (float) pitchSlider->getValue();
        s.pLockDecay = (float) decaySlider->getValue();
        s.pLockTone  = (float) toneSlider->getValue();
        s.pLockDrive = (float) driveSlider->getValue();
        s.pLockLevel = (float) levelSlider->getValue();
        s.pLockPan   = (float) panSlider->getValue();
        s.pLockSendA = (float) sendASlider->getValue();

        uint64_t mask = 0;
        if (std::abs(s.pLockPitch) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_PITCH;
        if (std::abs(s.pLockDecay - 1.0f) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_DECAY;
        if (s.pLockDrive > 0.01f) mask |= StepLockFlags::LOCK_FLAG_DRIVE;
        if (std::abs(s.pLockTone - 0.5f) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_TONE;
        if (std::abs(s.pLockLevel - 1.0f) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_LEVEL;
        if (std::abs(s.pLockPan) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_PAN;
        if (s.pLockSendA > 0.01f) mask |= StepLockFlags::LOCK_FLAG_SEND_A;
        const uint64_t paramMask = mask;
        if (s.padOverride >= 0 && s.padOverride != owner.seq.currentPattern().tracks[(size_t) curTrack].defaultPad)
            mask |= StepLockFlags::LOCK_FLAG_PAD_OVERRIDE;
        s.lockMask = mask;
        s.hasLocks = (paramMask != 0 || s.ratchet > 1);
        owner.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xEB110D0C));
        auto r = getLocalBounds().toFloat().reduced(2.f);
        ui::drawForgedPlate(g, r, 6.f, true);
    }

    void resized() override
    {
        title->setBounds(14, 10, 320, 20);
        closeBtn->setBounds(getWidth() - 70, 10, 56, 22);

        int y = 36;
        auto row = [&](std::unique_ptr<juce::Label>& lb, std::unique_ptr<juce::Slider>& sl)
        {
            lb->setBounds(14, y, 90, 20);
            sl->setBounds(108, y, getWidth() - 124, 20);
            y += 24;
        };

        padCombo->setBounds(14, y, getWidth() - 28, 22); y += 28;
        row(velLabel, velSlider);
        row(probLabel, probSlider);
        row(mtimeLabel, mtimeSlider);

        ratchetCombo->setBounds(14, y, getWidth() - 28, 22); y += 28;
        row(pitchLabel, pitchSlider);
        row(decayLabel, decaySlider);
        row(toneLabel, toneSlider);
        row(driveLabel, driveSlider);
        row(levelLabel, levelSlider);
        row(panLabel, panSlider);
        row(sendALabel, sendASlider);

        clearBtn->setBounds(14, y + 6, 120, 24);
    }

    SequencerPage& owner;
    int curTrack = -1, curStep = -1;
    std::unique_ptr<juce::Label> title, velLabel, probLabel, mtimeLabel, pitchLabel, decayLabel, toneLabel, driveLabel, levelLabel, panLabel, sendALabel;
    std::unique_ptr<juce::ComboBox> padCombo, ratchetCombo;
    std::unique_ptr<juce::Slider> velSlider, probSlider, mtimeSlider, pitchSlider, decaySlider, toneSlider, driveSlider, levelSlider, panSlider, sendASlider;
    std::unique_ptr<juce::TextButton> clearBtn, closeBtn;
};

void SequencerPage::StepButton::mouseDown(const juce::MouseEvent& e)
{
    owner.seq.setSelectedTrack(trackIdx);
    owner.repaint();

    if (e.mods.isPopupMenu() || e.mods.isAltDown())
    {
        owner.popoverTrack = trackIdx;
        owner.popoverStep = stepIdx;
        if (owner.pLockPopover)
            owner.pLockPopover->openForStep(trackIdx, stepIdx);
        return;
    }

    const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
    const auto& s = trk.steps[(size_t) stepIdx];
    wasActiveOnDown = s.active;
    dragStartY = (float) e.position.y;
    dragStartX = (float) e.position.x;

    if (! wasActiveOnDown)
    {
        owner.seq.setStepActive(trackIdx, stepIdx, true);
        switch (owner.getLockViewMode())
        {
            case SequencerPage::LOCK_VIEW_PROBABILITY: owner.seq.setStepProbability(trackIdx, stepIdx, 1.0f); break;
            case SequencerPage::LOCK_VIEW_MICROTIMING: owner.seq.setStepMicrotiming(trackIdx, stepIdx, 0.0f); break;
            case SequencerPage::LOCK_VIEW_PITCH:       owner.seq.setStepPitch(trackIdx, stepIdx, 0.0f); break;
            case SequencerPage::LOCK_VIEW_DECAY:       owner.seq.setStepDecay(trackIdx, stepIdx, 1.0f); break;
            case SequencerPage::LOCK_VIEW_DRIVE:       owner.seq.setStepDrive(trackIdx, stepIdx, 0.0f); break;
            case SequencerPage::LOCK_VIEW_LEVEL:       owner.seq.setStepLevel(trackIdx, stepIdx, 1.0f); break;
            case SequencerPage::LOCK_VIEW_PAN:         owner.seq.setStepPan(trackIdx, stepIdx, 0.0f); break;
            case SequencerPage::LOCK_VIEW_VELOCITY:
            default:                                   owner.seq.setStepVelocity(trackIdx, stepIdx, 0.85f); break;
        }
    }

    switch (owner.getLockViewMode())
    {
        case SequencerPage::LOCK_VIEW_PROBABILITY: dragStartVal = s.probability; break;
        case SequencerPage::LOCK_VIEW_MICROTIMING: dragStartVal = s.microtiming; break;
        case SequencerPage::LOCK_VIEW_PITCH:       dragStartVal = s.pLockPitch; break;
        case SequencerPage::LOCK_VIEW_DECAY:       dragStartVal = s.pLockDecay; break;
        case SequencerPage::LOCK_VIEW_DRIVE:       dragStartVal = s.pLockDrive; break;
        case SequencerPage::LOCK_VIEW_LEVEL:       dragStartVal = s.pLockLevel; break;
        case SequencerPage::LOCK_VIEW_PAN:         dragStartVal = s.pLockPan; break;
        case SequencerPage::LOCK_VIEW_VELOCITY:
        default:                                   dragStartVal = s.velocity; break;
    }

    repaint();
}

void SequencerPage::StepButton::mouseDrag(const juce::MouseEvent& e)
{
    const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
    if (trk.steps[(size_t) stepIdx].active)
    {
        const float deltaY = dragStartY - (float) e.position.y;
        const float deltaX = (float) e.position.x - dragStartX;
        switch (owner.getLockViewMode())
        {
            case SequencerPage::LOCK_VIEW_PROBABILITY:
            {
                const float newProb = juce::jlimit(0.0f, 1.0f, dragStartVal + deltaY / 80.0f);
                owner.seq.setStepProbability(trackIdx, stepIdx, newProb);
                break;
            }
            case SequencerPage::LOCK_VIEW_MICROTIMING:
            {
                const float newMicro = juce::jlimit(-0.5f, 0.5f, dragStartVal + deltaY / 120.0f);
                owner.seq.setStepMicrotiming(trackIdx, stepIdx, newMicro);
                break;
            }
            case SequencerPage::LOCK_VIEW_PITCH:
            {
                const float newPitch = std::round(juce::jlimit(-24.0f, 24.0f, dragStartVal + deltaY * (24.0f / 80.0f)));
                owner.seq.setStepPitch(trackIdx, stepIdx, newPitch);
                break;
            }
            case SequencerPage::LOCK_VIEW_DECAY:
            {
                const float newDecay = juce::jlimit(0.05f, 5.0f, dragStartVal + deltaY / 40.0f);
                owner.seq.setStepDecay(trackIdx, stepIdx, newDecay);
                break;
            }
            case SequencerPage::LOCK_VIEW_DRIVE:
            {
                const float newDrive = juce::jlimit(0.0f, 1.0f, dragStartVal + deltaY / 80.0f);
                owner.seq.setStepDrive(trackIdx, stepIdx, newDrive);
                break;
            }
            case SequencerPage::LOCK_VIEW_LEVEL:
            {
                const float newLevel = juce::jlimit(0.0f, 1.5f, dragStartVal + deltaY / 60.0f);
                owner.seq.setStepLevel(trackIdx, stepIdx, newLevel);
                break;
            }
            case SequencerPage::LOCK_VIEW_PAN:
            {
                const float newPan = juce::jlimit(-1.0f, 1.0f, dragStartVal + deltaX / 60.0f);
                owner.seq.setStepPan(trackIdx, stepIdx, newPan);
                break;
            }
            case SequencerPage::LOCK_VIEW_VELOCITY:
            default:
            {
                const float newVel = juce::jlimit(0.05f, 1.0f, dragStartVal + deltaY / 80.0f);
                owner.seq.setStepVelocity(trackIdx, stepIdx, newVel);
                break;
            }
        }
        repaint();
    }
}

void SequencerPage::StepButton::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isAltDown())
        return;

    if (wasActiveOnDown && e.getDistanceFromDragStart() < 4)
    {
        owner.seq.setStepActive(trackIdx, stepIdx, false);
        repaint();
    }
}

void SequencerPage::StepButton::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
    const auto& s = trk.steps[(size_t) stepIdx];
    if (s.active)
    {
        const bool up = (wheel.deltaY > 0.f);
        switch (owner.getLockViewMode())
        {
            case SequencerPage::LOCK_VIEW_PROBABILITY:
                owner.seq.setStepProbability(trackIdx, stepIdx, juce::jlimit(0.0f, 1.0f, s.probability + (up ? 0.05f : -0.05f)));
                break;
            case SequencerPage::LOCK_VIEW_MICROTIMING:
                owner.seq.setStepMicrotiming(trackIdx, stepIdx, juce::jlimit(-0.5f, 0.5f, s.microtiming + (up ? 0.02f : -0.02f)));
                break;
            case SequencerPage::LOCK_VIEW_PITCH:
                owner.seq.setStepPitch(trackIdx, stepIdx, juce::jlimit(-24.0f, 24.0f, s.pLockPitch + (up ? 1.0f : -1.0f)));
                break;
            case SequencerPage::LOCK_VIEW_DECAY:
                owner.seq.setStepDecay(trackIdx, stepIdx, juce::jlimit(0.05f, 5.0f, s.pLockDecay + (up ? 0.1f : -0.1f)));
                break;
            case SequencerPage::LOCK_VIEW_DRIVE:
                owner.seq.setStepDrive(trackIdx, stepIdx, juce::jlimit(0.0f, 1.0f, s.pLockDrive + (up ? 0.05f : -0.05f)));
                break;
            case SequencerPage::LOCK_VIEW_LEVEL:
                owner.seq.setStepLevel(trackIdx, stepIdx, juce::jlimit(0.0f, 1.5f, s.pLockLevel + (up ? 0.05f : -0.05f)));
                break;
            case SequencerPage::LOCK_VIEW_PAN:
                owner.seq.setStepPan(trackIdx, stepIdx, juce::jlimit(-1.0f, 1.0f, s.pLockPan + (up ? 0.05f : -0.05f)));
                break;
            case SequencerPage::LOCK_VIEW_VELOCITY:
            default:
                owner.seq.setStepVelocity(trackIdx, stepIdx, juce::jlimit(0.05f, 1.0f, s.velocity + (up ? 0.05f : -0.05f)));
                break;
        }
        repaint();
    }
}

// ---------------------------------------------------------------------------
// TrackLane
// ---------------------------------------------------------------------------
class SequencerPage::TrackLane : public juce::Component
{
public:
    TrackLane(SequencerPage& owner_, int trackIdx_)
        : owner(owner_), trackIdx(trackIdx_)
    {
        nameLabel = std::make_unique<juce::Label>();
        nameLabel->setText("TRK " + juce::String(trackIdx + 1), juce::dontSendNotification);
        nameLabel->setFont(uiFont(11.5f, true));
        nameLabel->setColour(juce::Label::textColourId, ui::accentHot());
        nameLabel->setJustificationType(juce::Justification::centredLeft);
        nameLabel->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(nameLabel.get());

        muteBtn = std::make_unique<juce::TextButton>("M");
        ui::styleButton(*muteBtn);
        muteBtn->setTooltip("Mute Track");
        muteBtn->onClick = [this]
        {
            const bool m = ! owner.seq.currentPattern().tracks[(size_t) trackIdx].mute;
            owner.seq.setTrackMute(trackIdx, m);
            muteBtn->setColour(juce::TextButton::buttonColourId, m ? ui::ember() : ui::panelHi());
        };
        addAndMakeVisible(muteBtn.get());

        soloBtn = std::make_unique<juce::TextButton>("S");
        ui::styleButton(*soloBtn);
        soloBtn->setTooltip("Solo Track");
        soloBtn->onClick = [this]
        {
            const bool s = ! owner.seq.currentPattern().tracks[(size_t) trackIdx].solo;
            owner.seq.setTrackSolo(trackIdx, s);
            soloBtn->setColour(juce::TextButton::buttonColourId, s ? juce::Colour(0xFFFFB703) : ui::panelHi());
        };
        addAndMakeVisible(soloBtn.get());

        speedBtn = std::make_unique<juce::TextButton>("1x");
        ui::styleButton(*speedBtn);
        speedBtn->setTooltip("Track clock speed multiplier/divider (click to choose 1/8x to 8x)");
        speedBtn->onClick = [this] { showSpeedMenu(); };
        addAndMakeVisible(speedBtn.get());

        lenBtn = std::make_unique<juce::TextButton>("16 STEPS");
        ui::styleButton(*lenBtn);
        lenBtn->setTooltip("Track polymetric length (click for options, scroll wheel to adjust 1..64)");
        lenBtn->onClick = [this] { showLengthMenu(); };
        addAndMakeVisible(lenBtn.get());

        padCombo = std::make_unique<juce::ComboBox>();
        ui::styleCombo(*padCombo);
        padCombo->setTooltip("Assigned Drum Pad (Bank & Sound)");
        populatePadCombo();
        padCombo->setSelectedId(owner.seq.currentPattern().tracks[(size_t) trackIdx].defaultPad + 1, juce::dontSendNotification);
        padCombo->onChange = [this]
        {
            owner.seq.setSelectedTrack(trackIdx);
            owner.seq.setTrackPad(trackIdx, padCombo->getSelectedId() - 1);
            owner.repaint();
        };
        addAndMakeVisible(padCombo.get());

        for (int s = 0; s < 16; ++s)
        {
            stepBtns[(size_t) s] = std::make_unique<StepButton>(owner, trackIdx, s);
            addAndMakeVisible(stepBtns[(size_t) s].get());
        }
    }

    void populatePadCombo(bool force = false)
    {
        if (! force && padCombo->getNumItems() > 0)
            return;

        padCombo->clear(juce::dontSendNotification);
        for (int p = 0; p < kNumPads; ++p)
        {
            const int b = p / 16;
            const char bc = (char) ('A' + b);
            const juce::String padName = owner.proc.grid().padState(p).getProperty("name", "Pad " + juce::String(p + 1)).toString();
            if (p % 16 == 0)
                padCombo->getRootMenu()->addSectionHeader("--- BANK " + juce::String::charToString(bc) + " ---");
            padCombo->addItem(juce::String::charToString(bc) + juce::String::formatted("%02d: ", (p % 16) + 1) + padName, p + 1);
        }
    }

    void showLengthMenu()
    {
        juce::PopupMenu m;
        const int curLen = owner.seq.currentPattern().tracks[(size_t) trackIdx].stepCount;

        m.addSectionHeader("COMMON LENGTHS");
        m.addItem(16, "16 Steps (1 Bar)", true, curLen == 16);
        m.addItem(32, "32 Steps (2 Bars)", true, curLen == 32);
        m.addItem(48, "48 Steps (3 Bars)", true, curLen == 48);
        m.addItem(64, "64 Steps (4 Bars / Full)", true, curLen == 64);

        m.addSeparator();
        m.addSectionHeader("POLYMETER / ODD");
        static const int oddLens[] = { 3, 5, 7, 9, 10, 11, 12, 13, 14, 15, 24 };
        for (int ol : oddLens)
            m.addItem(ol, juce::String(ol) + " Steps", true, curLen == ol);

        juce::PopupMenu allMenu;
        for (int i = 1; i <= 64; ++i)
            allMenu.addItem(100 + i, juce::String(i) + " Steps", true, curLen == i);
        m.addSubMenu("All Lengths (1 to 64)...", allMenu);

        m.addSeparator();
        m.addItem(201, "Duplicate Loop x2 (Double Length)");
        m.addItem(202, "Duplicate Loop x4 (Quadruple Length, e.g. 15 -> 60)");

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(lenBtn.get()),
            [this](int res)
            {
                if (res >= 1 && res <= 64)
                {
                    owner.seq.setTrackLength(trackIdx, res);
                    updateTrackData();
                    owner.repaint();
                }
                else if (res > 100 && res <= 164)
                {
                    owner.seq.setTrackLength(trackIdx, res - 100);
                    updateTrackData();
                    owner.repaint();
                }
                else if (res == 201)
                {
                    owner.seq.duplicateTrackLoop(trackIdx, 2);
                    updateTrackData();
                    owner.repaint();
                }
                else if (res == 202)
                {
                    owner.seq.duplicateTrackLoop(trackIdx, 4);
                    updateTrackData();
                    owner.repaint();
                }
            });
    }

    void showSpeedMenu()
    {
        juce::PopupMenu m;
        static const struct { float val; const char* label; } speeds[] = {
            { 0.125f, "1/8x (Slowest)" },
            { 0.25f,  "1/4x" },
            { 0.333f, "1/3x (Triplet Div)" },
            { 0.5f,   "1/2x (Half speed)" },
            { 0.667f, "2/3x" },
            { 0.75f,  "3/4x" },
            { 1.0f,   "1x (Normal)" },
            { 1.25f,  "1.25x" },
            { 1.5f,   "1.5x" },
            { 2.0f,   "2x (Double)" },
            { 3.0f,   "3x (Triple)" },
            { 4.0f,   "4x" },
            { 5.0f,   "5x" },
            { 6.0f,   "6x" },
            { 8.0f,   "8x (Fastest)" }
        };
        const float current = owner.seq.getTrackSpeed(trackIdx);
        for (int i = 0; i < 15; ++i)
        {
            const bool isCur = std::abs(current - speeds[i].val) < 0.02f;
            m.addItem(i + 1, speeds[i].label, true, isCur);
        }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(speedBtn.get()),
            [this](int res)
            {
                if (res > 0 && res <= 15)
                {
                    owner.seq.setTrackSpeed(trackIdx, speeds[res - 1].val);
                    updateTrackData();
                    owner.repaint();
                }
            });
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        owner.seq.setSelectedTrack(trackIdx);
        owner.repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.position.x < 276)
        {
            const int curLen = owner.seq.currentPattern().tracks[(size_t) trackIdx].stepCount;
            const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
            const int newLen = juce::jlimit(1, 64, curLen + delta);
            if (newLen != curLen)
            {
                owner.seq.setTrackLength(trackIdx, newLen);
                updateTrackData();
                owner.repaint();
            }
        }
    }

    void updateTrackData()
    {
        const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
        if (nameLabel != nullptr)
            nameLabel->setText("TRK " + juce::String(trackIdx + 1), juce::dontSendNotification);
        if (muteBtn != nullptr)
            muteBtn->setColour(juce::TextButton::buttonColourId, trk.mute ? ui::ember() : ui::panelHi());
        if (soloBtn != nullptr)
            soloBtn->setColour(juce::TextButton::buttonColourId, trk.solo ? juce::Colour(0xFFFFB703) : ui::panelHi());
        if (padCombo != nullptr)
        {
            populatePadCombo();
            padCombo->setSelectedId(trk.defaultPad + 1, juce::dontSendNotification);
        }
        if (lenBtn != nullptr)
            lenBtn->setButtonText(juce::String(trk.stepCount) + (trk.stepCount == 1 ? " STEP" : " STEPS"));

        if (speedBtn != nullptr)
        {
            const float s = owner.seq.getTrackSpeed(trackIdx);
            auto formatSpd = [](float spd) -> juce::String
            {
                if (std::abs(spd - 0.125f) < 0.01f) return "1/8x";
                if (std::abs(spd - 0.25f)  < 0.01f) return "1/4x";
                if (std::abs(spd - 0.333f) < 0.02f) return "1/3x";
                if (std::abs(spd - 0.5f)   < 0.01f) return "1/2x";
                if (std::abs(spd - 0.667f) < 0.02f) return "2/3x";
                if (std::abs(spd - 0.75f)  < 0.01f) return "3/4x";
                if (std::abs(spd - 1.0f)   < 0.01f) return "1x";
                if (std::abs(spd - 1.25f)  < 0.01f) return "1.25x";
                if (std::abs(spd - 1.5f)   < 0.01f) return "1.5x";
                if (std::abs(spd - 2.0f)   < 0.01f) return "2x";
                if (std::abs(spd - 3.0f)   < 0.01f) return "3x";
                if (std::abs(spd - 4.0f)   < 0.01f) return "4x";
                if (std::abs(spd - 5.0f)   < 0.01f) return "5x";
                if (std::abs(spd - 6.0f)   < 0.01f) return "6x";
                if (std::abs(spd - 8.0f)   < 0.01f) return "8x";
                return juce::String(spd, 1) + "x";
            };
            speedBtn->setButtonText(formatSpd(s));
        }
    }

    void updatePage(int pageIdx)
    {
        updateTrackData();
        const int startStep = pageIdx * 16;
        for (int s = 0; s < 16; ++s)
        {
            if (stepBtns[(size_t) s] != nullptr)
                stepBtns[(size_t) s]->setStepIndex(startStep + s);
        }
        repaint();
    }

    void resized() override
    {
        const int h = getHeight();
        const int stepAreaX = 280;

        // Row 1 (y = 5, height = 22): TRK name, Mute, Solo, Speed, Steps
        nameLabel->setBounds(4, 5, 52, 22);
        muteBtn->setBounds(58, 5, 24, 22);
        soloBtn->setBounds(84, 5, 24, 22);
        speedBtn->setBounds(111, 5, 46, 22);
        lenBtn->setBounds(160, 5, 112, 22);

        // Row 2 (y = 31, height = 24): Full width Pad Selector with Bank & Name
        padCombo->setBounds(4, 31, 268, 24);

        const int stepW = (getWidth() - stepAreaX - 8) / 16;
        for (int s = 0; s < 16; ++s)
            if (stepBtns[(size_t) s] != nullptr)
                stepBtns[(size_t) s]->setBounds(stepAreaX + s * stepW, 2, stepW - 2, h - 4);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const bool isSelected = (owner.seq.selectedTrackIndex() == trackIdx);
        ui::drawForgedPlate(g, r, 3.f, isSelected);

        if (isSelected)
        {
            g.setColour(ui::accentHot().withAlpha(0.75f));
            g.drawRoundedRectangle(r, 3.f, 1.5f);
        }

        // Draw vertical channel strip separator between header and steps
        g.setColour(ui::panelHi().withAlpha(0.6f));
        g.drawVerticalLine(276, 4.f, (float) getHeight() - 4.f);
    }

    SequencerPage& owner;
    int trackIdx;
    std::unique_ptr<juce::Label> nameLabel;
    std::unique_ptr<juce::TextButton> muteBtn, soloBtn;
    std::unique_ptr<juce::ComboBox> padCombo;
    std::unique_ptr<juce::TextButton> lenBtn;
    std::unique_ptr<juce::TextButton> speedBtn;
    std::array<std::unique_ptr<StepButton>, 16> stepBtns;
};

// ---------------------------------------------------------------------------
// SongBlockView
// ---------------------------------------------------------------------------
class SequencerPage::SongBlockView : public juce::Component
{
public:
    SongBlockView(SequencerPage& owner_, int blockIdx_)
        : owner(owner_), blockIdx(blockIdx_)
    {
        const auto blocks = owner.seq.getSongSequence();
        const int patIdx = (blockIdx < (int) blocks.size()) ? blocks[(size_t) blockIdx].patternIndex : 0;
        const int rep    = (blockIdx < (int) blocks.size()) ? blocks[(size_t) blockIdx].repeats : 1;

        patCombo = std::make_unique<juce::ComboBox>();
        ui::styleCombo(*patCombo);
        for (int p = 0; p < 16; ++p)
            patCombo->addItem("Pat " + juce::String(p + 1), p + 1);
        patCombo->setSelectedId(patIdx + 1, juce::dontSendNotification);
        patCombo->setTooltip("Pattern played in this song block");
        patCombo->onChange = [this]
        {
            auto seq = owner.seq.getSongSequence();
            if (blockIdx < (int) seq.size())
            {
                seq[(size_t) blockIdx].patternIndex = patCombo->getSelectedId() - 1;
                owner.seq.setSongSequence(seq);
            }
        };
        addAndMakeVisible(patCombo.get());

        repSlider = std::make_unique<juce::Slider>(juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
        repSlider->setRange(1, 16, 1);
        repSlider->setValue(rep, juce::dontSendNotification);
        repSlider->setColour(juce::Slider::textBoxTextColourId, ui::txt());
        repSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        repSlider->setTooltip("Repeats for this block");
        repSlider->onValueChange = [this]
        {
            auto seq = owner.seq.getSongSequence();
            if (blockIdx < (int) seq.size())
            {
                seq[(size_t) blockIdx].repeats = (int) repSlider->getValue();
                owner.seq.setSongSequence(seq);
            }
        };
        addAndMakeVisible(repSlider.get());

        delBtn = std::make_unique<juce::TextButton>("X");
        ui::styleButton(*delBtn);
        delBtn->onClick = [this]
        {
            owner.seq.removeSongBlock(blockIdx);
            owner.refreshFromSequencer();
        };
        addAndMakeVisible(delBtn.get());
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.f);
        const bool isPlayingThis = (owner.seq.getPlayMode() == StepSequencer::MODE_SONG
                                    && owner.seq.getSongBlockIndex() == blockIdx);
        ui::drawForgedPlate(g, r, 4.f, isPlayingThis);
    }

    void resized() override
    {
        patCombo->setBounds(6, 6, getWidth() - 32, 20);
        delBtn->setBounds(getWidth() - 24, 6, 18, 18);
        repSlider->setBounds(6, 30, getWidth() - 12, 22);
    }

    SequencerPage& owner;
    int blockIdx;
    std::unique_ptr<juce::ComboBox> patCombo;
    std::unique_ptr<juce::Slider> repSlider;
    std::unique_ptr<juce::TextButton> delBtn;
};

// ---------------------------------------------------------------------------
// SequencerPage
// ---------------------------------------------------------------------------
SequencerPage::SequencerPage(Forge64Processor& processor)
    : proc(processor), seq(processor.getSequencer())
{
    // Mode toggle button
    modeBtn = std::make_unique<juce::TextButton>("PATTERN MODE");
    ui::styleButton(*modeBtn);
    modeBtn->setLookAndFeel(&compactBtnLnF);
    modeBtn->onClick = [this]
    {
        const bool song = (seq.getPlayMode() == StepSequencer::MODE_PATTERN);
        seq.setPlayMode(song ? StepSequencer::MODE_SONG : StepSequencer::MODE_PATTERN);
        modeBtn->setButtonText(song ? "SONG ARRANGER" : "PATTERN MODE");
        patternContainer->setVisible(! song);
        songContainer->setVisible(song);
        resized();
    };
    addAndMakeVisible(modeBtn.get());

    // Play / Stop
    playBtn = std::make_unique<juce::TextButton>("PLAY");
    ui::styleButton(*playBtn);
    playBtn->setLookAndFeel(&compactBtnLnF);
    playBtn->onClick = [this]
    {
        const bool pl = ! seq.isPlaying();
        seq.setPlaying(pl);
        playBtn->setButtonText(pl ? "STOP" : "PLAY");
        playBtn->setColour(juce::TextButton::buttonColourId, pl ? ui::accent() : ui::panelHi());
    };
    addAndMakeVisible(playBtn.get());

    tempoLabel = ui::makeLabel("SYNC: DAW", 11.f, ui::dim());
    addAndMakeVisible(tempoLabel.get());

    swingLabel = ui::makeLabel("SWING", 9.f, ui::dim());
    addAndMakeVisible(swingLabel.get());
    swingSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    swingSlider->setRange(0.0, 0.75, 0.01);
    swingSlider->setValue(0.0, juce::dontSendNotification);
    swingSlider->setColour(juce::Slider::thumbColourId, ui::accent());
    swingSlider->setColour(juce::Slider::trackColourId, ui::accent());
    swingSlider->onValueChange = [this]
    {
        seq.setPatternSwing((float) swingSlider->getValue());
    };
    addAndMakeVisible(swingSlider.get());

    // 16 Pattern Buttons
    patternBarLabel = ui::makeLabel("PAT:", 10.f, ui::accentHot());
    patternBarLabel->setFont(uiFont(10.f, true));
    addAndMakeVisible(patternBarLabel.get());

    for (int p = 0; p < 16; ++p)
    {
        patternBtns[(size_t) p] = std::make_unique<juce::TextButton>(juce::String(p + 1));
        ui::styleButton(*patternBtns[(size_t) p]);
        patternBtns[(size_t) p]->setLookAndFeel(&compactBtnLnF);
        patternBtns[(size_t) p]->onClick = [this, p]
        {
            seq.setSelectedPattern(p);
            for (int i = 0; i < 16; ++i)
                patternBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId,
                    (i == p) ? ui::accent() : ui::panelHi());
            refreshFromSequencer();
        };
        addAndMakeVisible(patternBtns[(size_t) p].get());
    }
    patternBtns[0]->setColour(juce::TextButton::buttonColourId, ui::accent());

    // Pattern Presets
    presetCombo = std::make_unique<juce::ComboBox>();
    ui::styleCombo(*presetCombo);
    presetCombo->setTooltip("Sequence presets (Factory & User presets)");
    refreshPresetCombo();
    presetCombo->onChange = [this]
    {
        const int id = presetCombo->getSelectedId();
        if (id >= 1 && id <= 5)
        {
            confirmLoadPreset(id, presetCombo->getText(), [this, id] { seq.loadFactoryPreset(id - 1); });
        }
        else if (id >= 100 && id < 900)
        {
            const auto name = presetCombo->getText();
            confirmLoadPreset(id, name, [this, name]
            {
                SequencePresetManager::loadPreset(name, seq, seq.selectedPatternIndex());
            });
        }
        else if (id == 990)
        {
            presetCombo->setSelectedId(lastLoadedPresetId, juce::dontSendNotification);
            promptSaveSequence();
        }
        else if (id == 991)
        {
            presetCombo->setSelectedId(lastLoadedPresetId, juce::dontSendNotification);
            importSequenceFile();
        }
        else if (id == 992)
        {
            presetCombo->setSelectedId(lastLoadedPresetId, juce::dontSendNotification);
            exportSequenceFile();
        }
    };
    addAndMakeVisible(presetCombo.get());

    saveSeqBtn = std::make_unique<juce::TextButton>("SAVE");
    ui::styleButton(*saveSeqBtn);
    saveSeqBtn->setTooltip("Save current pattern as sequence preset");
    saveSeqBtn->onClick = [this] { promptSaveSequence(); };
    addAndMakeVisible(saveSeqBtn.get());

    // Lock View & Quick Edit mode controls
    lockModeLabel = ui::makeLabel("LOCK:", 10.f, ui::dim());
    addAndMakeVisible(lockModeLabel.get());

    auto setupLockBtn = [this](std::unique_ptr<juce::TextButton>& btn, const char* name, const char* tip, StepLockViewMode mode)
    {
        btn = std::make_unique<juce::TextButton>(name);
        ui::styleButton(*btn);
        btn->setLookAndFeel(&compactBtnLnF);
        btn->setTooltip(tip);
        btn->onClick = [this, mode] { setLockViewMode(mode); };
        addAndMakeVisible(btn.get());
    };

    setupLockBtn(velLockBtn,   "VEL",   "View and edit step Velocity (Molten Orange)", LOCK_VIEW_VELOCITY);
    setupLockBtn(probLockBtn,  "PROB",  "View and edit step Probability % (Mint Green)", LOCK_VIEW_PROBABILITY);
    setupLockBtn(microLockBtn, "TIME",  "View and edit step Microtiming offset (Electric Purple)", LOCK_VIEW_MICROTIMING);
    setupLockBtn(pitchLockBtn, "PITCH", "View and edit step Pitch lock -24..+24 st (Electric Blue)", LOCK_VIEW_PITCH);
    setupLockBtn(decayLockBtn, "DECAY", "View and edit step Decay lock 0.1x..5.0x (Emerald Green)", LOCK_VIEW_DECAY);
    setupLockBtn(driveLockBtn, "DRIVE", "View and edit step Drive saturation (Crimson Flame)", LOCK_VIEW_DRIVE);
    setupLockBtn(levelLockBtn, "LEVEL", "View and edit step Level lock (Radiant Gold)", LOCK_VIEW_LEVEL);
    setupLockBtn(panLockBtn,   "PAN",   "View and edit step Pan lock (Violet Orchid)", LOCK_VIEW_PAN);

    setLockViewMode(LOCK_VIEW_VELOCITY);

    // Action buttons
    copyBtn = std::make_unique<juce::TextButton>("COPY");
    ui::styleButton(*copyBtn);
    copyBtn->setLookAndFeel(&compactBtnLnF);
    copyBtn->onClick = [this] { seq.copyPattern(); };
    addAndMakeVisible(copyBtn.get());

    pasteBtn = std::make_unique<juce::TextButton>("PASTE");
    ui::styleButton(*pasteBtn);
    pasteBtn->setLookAndFeel(&compactBtnLnF);
    pasteBtn->onClick = [this]
    {
        const int patIdx = seq.selectedPatternIndex();
        auto before = seq.getPatternCopy(patIdx);
        seq.pastePattern();
        auto after = seq.getPatternCopy(patIdx);
        seq.setPattern(patIdx, *before);
        proc.getUndoManager().perform(new SequencerPatternAction(seq, patIdx, std::move(before), std::move(after)));
        refreshFromSequencer();
    };
    addAndMakeVisible(pasteBtn.get());

    clearBtn = std::make_unique<juce::TextButton>("CLEAR");
    ui::styleButton(*clearBtn);
    clearBtn->setLookAndFeel(&compactBtnLnF);
    clearBtn->setTooltip("Clear pattern (click to choose: current pattern, all patterns, or selected track)");
    clearBtn->onClick = [this]
    {
        juce::Component::SafePointer<SequencerPage> safeThis(this);
        juce::PopupMenu menu;
        const int patIdx = seq.selectedPatternIndex();
        menu.addSectionHeader("CLEAR OPTIONS");
        menu.addItem(1, "Clear Pattern " + juce::String(patIdx + 1) + " (Current)");
        menu.addItem(2, "Clear All 16 Patterns");
        menu.addItem(3, "Clear Selected Track Only");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(clearBtn.get()),
            [safeThis, patIdx](int result)
            {
                if (safeThis == nullptr || result == 0) return;
                auto* self = safeThis.getComponent();
                if (result == 1)
                {
                    auto before = self->seq.getPatternCopy(patIdx);
                    auto after = std::make_unique<PatternData>(*before);
                    for (auto& t : after->tracks)
                        for (auto& s : t.steps)
                            s.resetStep();
                    self->proc.getUndoManager().perform(new SequencerPatternAction(self->seq, patIdx, std::move(before), std::move(after)));
                    self->refreshFromSequencer();
                }
                else if (result == 2)
                {
                    auto before = self->seq.getAllPatternsCopy();
                    auto after = std::make_unique<std::array<PatternData, 16>>(*before);
                    for (auto& p : *after)
                        for (auto& t : p.tracks)
                            for (auto& s : t.steps)
                                s.resetStep();
                    self->proc.getUndoManager().perform(new SequencerAllPatternsAction(self->seq, std::move(before), std::move(after)));
                    self->refreshFromSequencer();
                }
                else if (result == 3)
                {
                    const int selTrk = juce::jlimit(0, 7, self->seq.selectedTrackIndex());
                    auto before = self->seq.getPatternCopy(patIdx);
                    auto after = std::make_unique<PatternData>(*before);
                    for (auto& s : after->tracks[(size_t) selTrk].steps)
                        s.resetStep();
                    self->proc.getUndoManager().perform(new SequencerPatternAction(self->seq, patIdx, std::move(before), std::move(after)));
                    self->refreshFromSequencer();
                }
            });
    };
    addAndMakeVisible(clearBtn.get());

    randBtn = std::make_unique<juce::TextButton>("RAND");
    ui::styleButton(*randBtn);
    randBtn->setLookAndFeel(&compactBtnLnF);
    randBtn->setTooltip("Randomize selected track (Shift+click to randomize all 8 tracks)");
    randBtn->onClick = [this]
    {
        const int patIdx = seq.selectedPatternIndex();
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown() ||
            juce::ModifierKeys::getCurrentModifiers().isAltDown())
        {
            auto before = seq.getPatternCopy(patIdx);
            seq.randomizeAllTracks();
            auto after = seq.getPatternCopy(patIdx);
            seq.setPattern(patIdx, *before);
            proc.getUndoManager().perform(new SequencerPatternAction(seq, patIdx, std::move(before), std::move(after)));
            refreshFromSequencer();
        }
        else
        {
            auto before = seq.getPatternCopy(patIdx);
            seq.randomizeCurrentTrack();
            auto after = seq.getPatternCopy(patIdx);
            seq.setPattern(patIdx, *before);
            proc.getUndoManager().perform(new SequencerPatternAction(seq, patIdx, std::move(before), std::move(after)));
            refreshFromSequencer();
        }
    };
    addAndMakeVisible(randBtn.get());

    class PageNavBtn : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;
        std::function<void(const juce::MouseEvent&)> onRightClick;
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
            {
                if (onRightClick)
                    onRightClick(e);
                return;
            }
            juce::TextButton::mouseDown(e);
        }
    };

    // Page Buttons: 1-16, 17-32, 33-48, 49-64
    static const char* pNames[4] = { "1-16", "17-32", "33-48", "49-64" };
    for (int p = 0; p < 4; ++p)
    {
        auto pb = std::make_unique<PageNavBtn>(pNames[p]);
        ui::styleButton(*pb);
        pb->setLookAndFeel(&compactBtnLnF);
        pb->setTooltip("Steps " + juce::String(p * 16 + 1) + "-" + juce::String((p + 1) * 16) + " (Right-click to Copy/Paste/Duplicate)");
        pb->onRightClick = [this, p](const juce::MouseEvent& e) { showPageContextMenu(p, e); };
        pb->onClick = [this, p]
        {
            seq.setPage(p);
            for (int i = 0; i < 4; ++i)
                pageBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId,
                    (i == p) ? ui::accent() : ui::panelHi());
            for (auto& tl : trackLanes)
                if (tl != nullptr) tl->updatePage(p);
        };
        addAndMakeVisible(pb.get());
        pageBtns[(size_t) p] = std::move(pb);
    }
    pageBtns[0]->setColour(juce::TextButton::buttonColourId, ui::accent());

    // Pattern Container
    patternContainer = std::make_unique<juce::Component>();
    addAndMakeVisible(patternContainer.get());

    for (int t = 0; t < 8; ++t)
    {
        trackLanes[(size_t) t] = std::make_unique<TrackLane>(*this, t);
        patternContainer->addAndMakeVisible(trackLanes[(size_t) t].get());
    }

    // Song Container
    songContainer = std::make_unique<juce::Component>();
    addSongBlockBtn = std::make_unique<juce::TextButton>("+ Add Pattern Block");
    ui::styleButton(*addSongBlockBtn);
    addSongBlockBtn->onClick = [this]
    {
        seq.addSongBlock(seq.selectedPatternIndex(), 2);
        refreshFromSequencer();
    };
    songContainer->addAndMakeVisible(addSongBlockBtn.get());

    clearSongBtn = std::make_unique<juce::TextButton>("Clear Arranger");
    ui::styleButton(*clearSongBtn);
    clearSongBtn->onClick = [this]
    {
        seq.clearSongSequence();
        refreshFromSequencer();
    };
    songContainer->addAndMakeVisible(clearSongBtn.get());

    songViewport = std::make_unique<juce::Viewport>();
    songContainer->addAndMakeVisible(songViewport.get());

    songContainer->setVisible(false);
    addChildComponent(songContainer.get());

    // P-Lock Popover
    pLockPopover = std::make_unique<PLockPopover>(*this);
    pLockPopover->setVisible(false);
    addChildComponent(pLockPopover.get());

    refreshFromSequencer();
    startTimerHz(30);
}

SequencerPage::~SequencerPage()
{
    stopTimer();
    for (auto& b : patternBtns) if (b != nullptr) b->setLookAndFeel(nullptr);
    for (auto& b : pageBtns)    if (b != nullptr) b->setLookAndFeel(nullptr);
    if (velLockBtn != nullptr)   velLockBtn->setLookAndFeel(nullptr);
    if (probLockBtn != nullptr)  probLockBtn->setLookAndFeel(nullptr);
    if (microLockBtn != nullptr) microLockBtn->setLookAndFeel(nullptr);
    if (pitchLockBtn != nullptr) pitchLockBtn->setLookAndFeel(nullptr);
    if (decayLockBtn != nullptr) decayLockBtn->setLookAndFeel(nullptr);
    if (driveLockBtn != nullptr) driveLockBtn->setLookAndFeel(nullptr);
    if (levelLockBtn != nullptr) levelLockBtn->setLookAndFeel(nullptr);
    if (panLockBtn != nullptr)   panLockBtn->setLookAndFeel(nullptr);
    if (saveSeqBtn != nullptr)   saveSeqBtn->setLookAndFeel(nullptr);
    if (copyBtn != nullptr)      copyBtn->setLookAndFeel(nullptr);
    if (pasteBtn != nullptr)     pasteBtn->setLookAndFeel(nullptr);
    if (clearBtn != nullptr)     clearBtn->setLookAndFeel(nullptr);
    if (randBtn != nullptr)      randBtn->setLookAndFeel(nullptr);
    if (playBtn != nullptr)      playBtn->setLookAndFeel(nullptr);
    if (modeBtn != nullptr)      modeBtn->setLookAndFeel(nullptr);
}

void SequencerPage::showPageContextMenu(int pageIdx, const juce::MouseEvent& e)
{
    juce::PopupMenu menu;
    const int selTrk = seq.selectedTrackIndex();
    menu.addSectionHeader("PAGE " + juce::String(pageIdx * 16 + 1) + "-" + juce::String((pageIdx + 1) * 16) + " (TRACK " + juce::String(selTrk + 1) + ")");
    menu.addItem(1, "Copy Page (Track " + juce::String(selTrk + 1) + ")");
    menu.addItem(2, "Paste Page (Track " + juce::String(selTrk + 1) + ")");
    menu.addItem(3, "Duplicate to Next Page (Track " + juce::String(selTrk + 1) + ")");
    menu.addItem(4, "Duplicate to All 4 Pages (Track " + juce::String(selTrk + 1) + ")");
    menu.addItem(6, "Duplicate Track Loop x2 (Double Length)");
    menu.addItem(7, "Duplicate Track Loop x4 (Quadruple Length, e.g. 15 -> 60)");
    menu.addItem(5, "Clear Page (Track " + juce::String(selTrk + 1) + ")");

    menu.addSeparator();
    menu.addSectionHeader("PAGE " + juce::String(pageIdx * 16 + 1) + "-" + juce::String((pageIdx + 1) * 16) + " (ALL 8 TRACKS)");
    menu.addItem(11, "Copy Page (All 8 Tracks)");
    menu.addItem(12, "Paste Page (All 8 Tracks)");
    menu.addItem(13, "Duplicate to Next Page (All 8 Tracks)");
    menu.addItem(14, "Duplicate to All 4 Pages (All 8 Tracks)");
    menu.addItem(16, "Duplicate All Tracks Loop x2");
    menu.addItem(17, "Duplicate All Tracks Loop x4");
    menu.addItem(15, "Clear Page (All 8 Tracks)");

    juce::Component::SafePointer<SequencerPage> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
        [safeThis, selTrk, pageIdx](int res)
        {
            if (safeThis == nullptr || res == 0) return;
            auto* self = safeThis.getComponent();
            switch (res)
            {
                case 1:  self->seq.copyPage(selTrk, pageIdx); break;
                case 2:  self->seq.pastePage(selTrk, pageIdx); self->refreshFromSequencer(); break;
                case 3:  self->seq.duplicatePageToNext(selTrk, pageIdx); self->refreshFromSequencer(); break;
                case 4:  self->seq.duplicatePageToAll(selTrk, pageIdx); self->refreshFromSequencer(); break;
                case 5:  self->seq.clearPage(selTrk, pageIdx); self->refreshFromSequencer(); break;
                case 6:  self->seq.duplicateTrackLoop(selTrk, 2); self->refreshFromSequencer(); break;
                case 7:  self->seq.duplicateTrackLoop(selTrk, 4); self->refreshFromSequencer(); break;
                case 11: self->seq.copyAllTracksPage(pageIdx); break;
                case 12: self->seq.pasteAllTracksPage(pageIdx); self->refreshFromSequencer(); break;
                case 13: self->seq.duplicateAllTracksPageToNext(pageIdx); self->refreshFromSequencer(); break;
                case 14: self->seq.duplicateAllTracksPageToAll(pageIdx); self->refreshFromSequencer(); break;
                case 15: self->seq.clearAllTracksPage(pageIdx); self->refreshFromSequencer(); break;
                case 16: self->seq.duplicateAllTracksLoop(2); self->refreshFromSequencer(); break;
                case 17: self->seq.duplicateAllTracksLoop(4); self->refreshFromSequencer(); break;
                default: break;
            }
        });
}

void SequencerPage::timerCallback()
{
    const int curPat = seq.selectedPatternIndex();
    const uint32_t curVer = seq.getPatternVersion();
    if (curPat != lastActivePattern || curVer != lastPatternVersion)
    {
        lastActivePattern = curPat;
        lastPatternVersion = curVer;
        for (int i = 0; i < 16; ++i)
            if (patternBtns[(size_t) i] != nullptr)
                patternBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId,
                    (i == curPat) ? ui::accent() : ui::panelHi());
        refreshFromSequencer();
    }
    const int curPg = seq.getPage();
    if (curPg != lastActivePage)
    {
        lastActivePage = curPg;
        for (int i = 0; i < 4; ++i)
            if (pageBtns[(size_t) i] != nullptr)
                pageBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId,
                    (i == curPg) ? ui::accent() : ui::panelHi());
        for (auto& tl : trackLanes)
            if (tl != nullptr) tl->updatePage(curPg);
    }
    repaint();
}

void SequencerPage::refreshFromSequencer()
{
    if (swingSlider != nullptr)
        swingSlider->setValue(seq.currentPattern().tracks[0].swing, juce::dontSendNotification);

    const int curPg = seq.getPage();
    for (auto& tl : trackLanes)
        if (tl != nullptr)
            tl->updatePage(curPg);

    // Refresh Song Blocks only if block count changed
    auto blocks = seq.getSongSequence();
    if (songBlockViews.size() != blocks.size())
    {
        songBlockViews.clear();
        for (size_t b = 0; b < blocks.size(); ++b)
        {
            auto view = std::make_unique<SongBlockView>(*this, (int) b);
            songContainer->addAndMakeVisible(view.get());
            songBlockViews.push_back(std::move(view));
        }
        resized();
    }
    repaint();
}

void SequencerPage::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
}

void SequencerPage::setLockViewMode(StepLockViewMode mode)
{
    lockViewMode = mode;
    if (velLockBtn != nullptr)
        velLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_VELOCITY) ? ui::accent() : ui::panelHi());

    if (probLockBtn != nullptr)
        probLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_PROBABILITY) ? juce::Colour(0xFF00BFA5) : ui::panelHi());

    if (microLockBtn != nullptr)
        microLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_MICROTIMING) ? juce::Colour(0xFFA826E8) : ui::panelHi());

    if (pitchLockBtn != nullptr)
        pitchLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_PITCH) ? juce::Colour(0xFF2979FF) : ui::panelHi());

    if (decayLockBtn != nullptr)
        decayLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_DECAY) ? juce::Colour(0xFF00E676) : ui::panelHi());

    if (driveLockBtn != nullptr)
        driveLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_DRIVE) ? juce::Colour(0xFFFF3D00) : ui::panelHi());

    if (levelLockBtn != nullptr)
        levelLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_LEVEL) ? juce::Colour(0xFFFFD600) : ui::panelHi());

    if (panLockBtn != nullptr)
        panLockBtn->setColour(juce::TextButton::buttonColourId,
            (mode == LOCK_VIEW_PAN) ? juce::Colour(0xFFE040FB) : ui::panelHi());

    repaint();
}

void SequencerPage::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Row 1 (y = 5, height = 24): Transport, Mode, Groove, Presets, Lock Modes, Pages
    int r1x = 6;
    modeBtn->setBounds(r1x, 5, 84, 24);    r1x += 88;
    playBtn->setBounds(r1x, 5, 44, 24);    r1x += 48;
    swingSlider->setBounds(r1x, 5, 50, 24); r1x += 54;
    presetCombo->setBounds(r1x, 5, 114, 24); r1x += 118;
    if (saveSeqBtn != nullptr) { saveSeqBtn->setBounds(r1x, 5, 42, 24); r1x += 46; }

    if (lockModeLabel != nullptr) { lockModeLabel->setBounds(r1x, 5, 36, 24); r1x += 38; }
    if (velLockBtn != nullptr)    { velLockBtn->setBounds(r1x, 5, 28, 24);    r1x += 30; }
    if (probLockBtn != nullptr)   { probLockBtn->setBounds(r1x, 5, 34, 24);   r1x += 36; }
    if (microLockBtn != nullptr)  { microLockBtn->setBounds(r1x, 5, 34, 24);  r1x += 36; }
    if (pitchLockBtn != nullptr)  { pitchLockBtn->setBounds(r1x, 5, 38, 24);  r1x += 40; }
    if (decayLockBtn != nullptr)  { decayLockBtn->setBounds(r1x, 5, 40, 24);  r1x += 42; }
    if (driveLockBtn != nullptr)  { driveLockBtn->setBounds(r1x, 5, 38, 24);  r1x += 40; }
    if (levelLockBtn != nullptr)  { levelLockBtn->setBounds(r1x, 5, 38, 24);  r1x += 40; }
    if (panLockBtn != nullptr)    { panLockBtn->setBounds(r1x, 5, 30, 24);    r1x += 32; }

    // Page Buttons on Row 1 (Right-aligned)
    const int pageBtnW = 34;
    int px = w - 6 - 4 * (pageBtnW + 3);
    for (int p = 0; p < 4; ++p)
    {
        pageBtns[(size_t) p]->setBounds(px, 5, pageBtnW, 24);
        px += pageBtnW + 3;
    }

    // Row 2 (y = 33, height = 24): Pattern Selection & Action Buttons
    int r2x = 6;
    if (patternBarLabel != nullptr)
    {
        patternBarLabel->setBounds(r2x, 33, 34, 24);
        r2x += 36;
    }

    // 16 hardware-style pattern buttons (each 24px wide)
    const int patBtnW = 24;
    for (int p = 0; p < 16; ++p)
    {
        patternBtns[(size_t) p]->setBounds(r2x, 33, patBtnW, 24);
        r2x += patBtnW + 2;
    }
    r2x += 6;

    copyBtn->setBounds(r2x, 33, 38, 24);  r2x += 41;
    pasteBtn->setBounds(r2x, 33, 40, 24); r2x += 43;
    clearBtn->setBounds(r2x, 33, 42, 24); r2x += 45;
    randBtn->setBounds(r2x, 33, 38, 24);

    // Pattern Mode container (starting below row 2 at y = 62)
    const int topMargin = 62;
    patternContainer->setBounds(6, topMargin, w - 12, h - topMargin - 6);
    const int laneH = (h - topMargin - 10) / 8;
    for (int t = 0; t < 8; ++t)
        if (trackLanes[(size_t) t] != nullptr)
            trackLanes[(size_t) t]->setBounds(0, t * laneH, w - 12, laneH);

    // Song Mode
    songContainer->setBounds(8, topMargin, w - 16, h - topMargin - 8);
    addSongBlockBtn->setBounds(10, 8, 150, 26);
    clearSongBtn->setBounds(170, 8, 110, 26);

    int bx = 10, by = 46;
    for (auto& sb : songBlockViews)
    {
        if (sb != nullptr)
        {
            sb->setBounds(bx, by, 100, 60);
            bx += 106;
            if (bx + 100 > w - 24)
            {
                bx = 10;
                by += 68;
            }
        }
    }

    // Popover overlay center
    if (pLockPopover->isVisible())
        pLockPopover->setBounds((w - 380) / 2, (h - 430) / 2, 380, 430);
}

void SequencerPage::refreshPresetCombo()
{
    if (presetCombo == nullptr) return;
    presetCombo->clear(juce::dontSendNotification);

    presetCombo->addItem("--- FACTORY PRESETS ---", 900);
    presetCombo->setItemEnabled(900, false);
    presetCombo->addItem("4-on-the-Floor Techno", 1);
    presetCombo->addItem("Trap 808 & Rolls", 2);
    presetCombo->addItem("Polymetric 5/7/16", 3);
    presetCombo->addItem("Breakbeat Funk", 4);
    presetCombo->addItem("Afro Clave", 5);

    auto userPresets = SequencePresetManager::getUserPresetNames();
    presetCombo->addSeparator();
    presetCombo->addItem("--- USER PRESETS ---", 901);
    presetCombo->setItemEnabled(901, false);

    if (userPresets.isEmpty())
    {
        presetCombo->addItem("(No Saved Presets)", 902);
        presetCombo->setItemEnabled(902, false);
    }
    else
    {
        for (int i = 0; i < userPresets.size(); ++i)
        {
            presetCombo->addItem(userPresets[i], 100 + i);
        }
    }

    presetCombo->addSeparator();
    presetCombo->addItem("[+] Save Sequence...", 990);
    presetCombo->addItem("Import Sequence File...", 991);
    presetCombo->addItem("Export Sequence File...", 992);
}

void SequencerPage::confirmLoadPreset(int id, const juce::String& name, std::function<void()> loadAction)
{
    auto* w = new juce::AlertWindow("Load Sequence",
                                    "Loading sequence preset '" + name + "' will overwrite your current pattern.\n"
                                    "Any unsaved work will be lost!\n\nDo you want to proceed?",
                                    juce::AlertWindow::WarningIcon);
    w->addButton("Load", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->toFront(true);

    juce::Component::SafePointer<SequencerPage> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, id, loadAction](int result)
    {
        std::unique_ptr<juce::AlertWindow> deleter(w);
        if (safe == nullptr) return;
        if (result == 1)
        {
            safe->lastLoadedPresetId = id;
            loadAction();
            safe->refreshFromSequencer();
            safe->repaint();
        }
        else
        {
            if (safe->presetCombo)
                safe->presetCombo->setSelectedId(safe->lastLoadedPresetId, juce::dontSendNotification);
        }
    }));
}

void SequencerPage::promptSaveSequence()
{
    auto* w = new juce::AlertWindow("Save Sequence Preset",
                                    "Enter a name for this sequence preset:",
                                    juce::AlertWindow::QuestionIcon);
    juce::String defName = seq.currentPattern().name;
    if (defName.isEmpty() || defName.startsWith("Pattern "))
        defName = "My Sequence";
    w->addTextEditor("name", defName, "Preset Name:");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->toFront(true);

    juce::Component::SafePointer<SequencerPage> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int result)
    {
        std::unique_ptr<juce::AlertWindow> deleter(w);
        if (safe == nullptr || result != 1) return;
        auto name = w->getTextEditorContents("name").trim();
        if (name.isEmpty()) return;

        if (SequencePresetManager::savePreset(name, safe->seq, safe->seq.selectedPatternIndex()))
        {
            safe->refreshPresetCombo();
            auto userPresets = SequencePresetManager::getUserPresetNames();
            for (int i = 0; i < userPresets.size(); ++i)
            {
                if (userPresets[i] == name)
                {
                    safe->lastLoadedPresetId = 100 + i;
                    safe->presetCombo->setSelectedId(100 + i, juce::dontSendNotification);
                    break;
                }
            }
        }
    }));
}

void SequencerPage::importSequenceFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Sequence File",
        SequencePresetManager::getPresetsDirectory(),
        "*.f64seq;*.xml");

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<SequencerPage> safe(this);
    chooser->launchAsync(folderFlags, [safe, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile() && safe != nullptr)
        {
            safe->confirmLoadPreset(991, file.getFileNameWithoutExtension(), [safe, file]
            {
                SequencePresetManager::loadPresetFromFile(file, safe->seq, safe->seq.selectedPatternIndex());
            });
        }
    });
}

void SequencerPage::exportSequenceFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Sequence File",
        SequencePresetManager::getPresetsDirectory().getChildFile(
            juce::File::createLegalFileName(seq.currentPattern().name.isNotEmpty() ? seq.currentPattern().name : "Sequence") + ".f64seq"),
        "*.f64seq");

    auto folderFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
    juce::Component::SafePointer<SequencerPage> safe(this);
    chooser->launchAsync(folderFlags, [safe, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File() && safe != nullptr)
        {
            SequencePresetManager::exportPresetToFile(file, safe->seq, safe->seq.selectedPatternIndex());
            safe->refreshPresetCombo();
        }
    });
}

} // namespace f64
