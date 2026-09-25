#include "SequencerPage.h"
#include "../PluginProcessor.h"

namespace f64 {

// ---------------------------------------------------------------------------
// StepButton
// ---------------------------------------------------------------------------
class SequencerPage::StepButton : public juce::Component
{
public:
    StepButton(SequencerPage& owner_, int trackIdx_, int stepIdx_)
        : owner(owner_), trackIdx(trackIdx_), stepIdx(stepIdx_) {}

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
        if (s.hasLocks)
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
                s.hasLocks = false;
                s.padOverride = -1;
                s.ratchet = 1;
                s.microtiming = 0.f;
                s.probability = 1.f;
                s.pLockPitch = 0.0f;
                s.pLockDecay = 1.0f;
                s.pLockDrive = 0.0f;
                s.pLockLevel = 1.0f;
                s.pLockPan   = 0.0f;
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
        s.hasLocks = (std::abs(s.pLockPitch) > 0.01f || std::abs(s.pLockDecay - 1.0f) > 0.01f
                      || std::abs(s.pLockTone - 0.5f) > 0.01f || s.pLockDrive > 0.01f || s.pLockSendA > 0.01f
                      || std::abs(s.pLockLevel - 1.0f) > 0.01f || std::abs(s.pLockPan) > 0.01f
                      || s.padOverride >= 0 || s.ratchet > 1);
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
                const float newPan = juce::jlimit(-1.0f, 1.0f, dragStartVal + deltaY / 60.0f);
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
        nameLabel->setText(owner.seq.currentPattern().tracks[(size_t) trackIdx].name, juce::dontSendNotification);
        nameLabel->setFont(uiFont(11.f, true));
        nameLabel->setColour(juce::Label::textColourId, ui::accentHot());
        addAndMakeVisible(nameLabel.get());

        muteBtn = std::make_unique<juce::TextButton>("M");
        ui::styleButton(*muteBtn);
        muteBtn->onClick = [this]
        {
            const bool m = ! owner.seq.currentPattern().tracks[(size_t) trackIdx].mute;
            owner.seq.setTrackMute(trackIdx, m);
            muteBtn->setColour(juce::TextButton::buttonColourId, m ? ui::ember() : ui::panelHi());
        };
        addAndMakeVisible(muteBtn.get());

        soloBtn = std::make_unique<juce::TextButton>("S");
        ui::styleButton(*soloBtn);
        soloBtn->onClick = [this]
        {
            const bool s = ! owner.seq.currentPattern().tracks[(size_t) trackIdx].solo;
            owner.seq.setTrackSolo(trackIdx, s);
            soloBtn->setColour(juce::TextButton::buttonColourId, s ? juce::Colour(0xFFFFB703) : ui::panelHi());
        };
        addAndMakeVisible(soloBtn.get());

        padCombo = std::make_unique<juce::ComboBox>();
        ui::styleCombo(*padCombo);
        for (int p = 0; p < kNumPads; ++p)
        {
            const int b = p / 16;
            const char bc = (char) ('A' + b);
            padCombo->addItem(juce::String::charToString(bc) + juce::String::formatted("%02d", (p % 16) + 1), p + 1);
        }
        padCombo->setSelectedId(owner.seq.currentPattern().tracks[(size_t) trackIdx].defaultPad + 1, juce::dontSendNotification);
        padCombo->onChange = [this]
        {
            owner.seq.setSelectedTrack(trackIdx);
            owner.seq.setTrackPad(trackIdx, padCombo->getSelectedId() - 1);
            owner.repaint();
        };
        addAndMakeVisible(padCombo.get());

        lenSlider = std::make_unique<juce::Slider>(juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
        lenSlider->setRange(1, 64, 1);
        lenSlider->setValue(owner.seq.currentPattern().tracks[(size_t) trackIdx].stepCount, juce::dontSendNotification);
        lenSlider->setColour(juce::Slider::textBoxTextColourId, ui::accentHot());
        lenSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        lenSlider->onValueChange = [this]
        {
            owner.seq.setSelectedTrack(trackIdx);
            owner.seq.setTrackLength(trackIdx, (int) lenSlider->getValue());
            owner.repaint();
        };
        addAndMakeVisible(lenSlider.get());

        for (int s = 0; s < 16; ++s)
        {
            stepBtns[(size_t) s] = std::make_unique<StepButton>(owner, trackIdx, s);
            addAndMakeVisible(stepBtns[(size_t) s].get());
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        owner.seq.setSelectedTrack(trackIdx);
        owner.repaint();
    }

    void updateTrackData()
    {
        const auto& trk = owner.seq.currentPattern().tracks[(size_t) trackIdx];
        if (nameLabel != nullptr)
            nameLabel->setText(trk.name, juce::dontSendNotification);
        if (muteBtn != nullptr)
            muteBtn->setColour(juce::TextButton::buttonColourId, trk.mute ? ui::ember() : ui::panelHi());
        if (soloBtn != nullptr)
            soloBtn->setColour(juce::TextButton::buttonColourId, trk.solo ? juce::Colour(0xFFFFB703) : ui::panelHi());
        if (padCombo != nullptr)
            padCombo->setSelectedId(trk.defaultPad + 1, juce::dontSendNotification);
        if (lenSlider != nullptr)
            lenSlider->setValue(trk.stepCount, juce::dontSendNotification);
    }

    void updatePage(int pageIdx)
    {
        updateTrackData();
        const int startStep = pageIdx * 16;
        for (int s = 0; s < 16; ++s)
        {
            stepBtns[(size_t) s] = std::make_unique<StepButton>(owner, trackIdx, startStep + s);
            addAndMakeVisible(stepBtns[(size_t) s].get());
        }
        resized();
    }

    void resized() override
    {
        const int h = getHeight();
        nameLabel->setBounds(4, 4, 75, 20);
        muteBtn->setBounds(82, 4, 20, 20);
        soloBtn->setBounds(104, 4, 20, 20);
        padCombo->setBounds(126, 4, 60, 20);
        lenSlider->setBounds(190, 4, 52, 20);

        const int stepAreaX = 250;
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
    }

    SequencerPage& owner;
    int trackIdx;
    std::unique_ptr<juce::Label> nameLabel;
    std::unique_ptr<juce::TextButton> muteBtn, soloBtn;
    std::unique_ptr<juce::ComboBox> padCombo;
    std::unique_ptr<juce::Slider> lenSlider;
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
    presetCombo->addItem("4-on-the-Floor Techno", 1);
    presetCombo->addItem("Trap 808 & Rolls", 2);
    presetCombo->addItem("Polymetric 5/7/16", 3);
    presetCombo->addItem("Breakbeat Funk", 4);
    presetCombo->addItem("Afro Clave", 5);
    presetCombo->onChange = [this]
    {
        const int id = presetCombo->getSelectedId();
        if (id > 0)
        {
            seq.loadFactoryPreset(id - 1);
            refreshFromSequencer();
        }
    };
    addAndMakeVisible(presetCombo.get());

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
    pasteBtn->onClick = [this] { seq.pastePattern(); refreshFromSequencer(); };
    addAndMakeVisible(pasteBtn.get());

    clearBtn = std::make_unique<juce::TextButton>("CLEAR");
    ui::styleButton(*clearBtn);
    clearBtn->setLookAndFeel(&compactBtnLnF);
    clearBtn->onClick = [this] { seq.clearCurrentPattern(); refreshFromSequencer(); };
    addAndMakeVisible(clearBtn.get());

    randBtn = std::make_unique<juce::TextButton>("RAND");
    ui::styleButton(*randBtn);
    randBtn->setLookAndFeel(&compactBtnLnF);
    randBtn->setTooltip("Randomize selected track (Shift+click to randomize all 8 tracks)");
    randBtn->onClick = [this]
    {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown() ||
            juce::ModifierKeys::getCurrentModifiers().isAltDown())
        {
            seq.randomizeAllTracks();
        }
        else
        {
            seq.randomizeCurrentTrack();
        }
        refreshFromSequencer();
    };
    addAndMakeVisible(randBtn.get());

    // Page Buttons: 1-16, 17-32, 33-48, 49-64
    static const char* pNames[4] = { "1-16", "17-32", "33-48", "49-64" };
    for (int p = 0; p < 4; ++p)
    {
        pageBtns[(size_t) p] = std::make_unique<juce::TextButton>(pNames[p]);
        ui::styleButton(*pageBtns[(size_t) p]);
        pageBtns[(size_t) p]->setLookAndFeel(&compactBtnLnF);
        pageBtns[(size_t) p]->onClick = [this, p]
        {
            seq.setPage(p);
            for (int i = 0; i < 4; ++i)
                pageBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId,
                    (i == p) ? ui::accent() : ui::panelHi());
            for (auto& tl : trackLanes)
                if (tl != nullptr) tl->updatePage(p);
        };
        addAndMakeVisible(pageBtns[(size_t) p].get());
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
    if (copyBtn != nullptr)      copyBtn->setLookAndFeel(nullptr);
    if (pasteBtn != nullptr)     pasteBtn->setLookAndFeel(nullptr);
    if (clearBtn != nullptr)     clearBtn->setLookAndFeel(nullptr);
    if (randBtn != nullptr)      randBtn->setLookAndFeel(nullptr);
    if (playBtn != nullptr)      playBtn->setLookAndFeel(nullptr);
    if (modeBtn != nullptr)      modeBtn->setLookAndFeel(nullptr);
}

void SequencerPage::timerCallback()
{
    repaint();
}

void SequencerPage::refreshFromSequencer()
{
    if (swingSlider != nullptr)
        swingSlider->setValue(seq.currentPattern().tracks[0].swing, juce::dontSendNotification);

    for (auto& tl : trackLanes)
        if (tl != nullptr)
        {
            tl->updateTrackData();
            tl->updatePage(seq.getPage());
        }

    // Refresh Song Blocks
    songBlockViews.clear();
    auto blocks = seq.getSongSequence();
    for (size_t b = 0; b < blocks.size(); ++b)
    {
        auto view = std::make_unique<SongBlockView>(*this, (int) b);
        songContainer->addAndMakeVisible(view.get());
        songBlockViews.push_back(std::move(view));
    }
    resized();
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
    presetCombo->setBounds(r1x, 5, 108, 24); r1x += 114;

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

} // namespace f64
