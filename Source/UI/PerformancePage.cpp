#include "PerformancePage.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace f64 {

// ===========================================================================
// XYPadComponent
// ===========================================================================
XYPadComponent::XYPadComponent(Forge64Processor& processor, const juce::String& title,
                               int defaultXMacro, int defaultYMacro)
    : proc(processor), padTitle(title)
{
    titleLabel = ui::makeLabel(padTitle, 12.f, ui::accentHot());
    titleLabel->setFont(uiFont(12.f, true));
    addAndMakeVisible(titleLabel.get());

    xDestLabel = ui::makeLabel("X-AXIS:", 10.f, ui::dim());
    addAndMakeVisible(xDestLabel.get());

    xDestCombo = std::make_unique<juce::ComboBox>();
    ui::styleCombo(*xDestCombo);
    for (int i = 0; i < kNumMacros; ++i)
        xDestCombo->addItem("Macro " + juce::String(i + 1), i + 1);
    xDestCombo->addItem("Master Volume", 101);
    xDestCombo->addItem("Reverb Size", 102);
    xDestCombo->addItem("Delay Time", 103);
    xDestCombo->setSelectedId(defaultXMacro + 1, juce::dontSendNotification);
    addAndMakeVisible(xDestCombo.get());

    yDestLabel = ui::makeLabel("Y-AXIS:", 10.f, ui::dim());
    addAndMakeVisible(yDestLabel.get());

    yDestCombo = std::make_unique<juce::ComboBox>();
    ui::styleCombo(*yDestCombo);
    for (int i = 0; i < kNumMacros; ++i)
        yDestCombo->addItem("Macro " + juce::String(i + 1), i + 1);
    yDestCombo->addItem("Master Volume", 101);
    yDestCombo->addItem("Reverb Size", 102);
    yDestCombo->addItem("Delay Time", 103);
    yDestCombo->setSelectedId(defaultYMacro + 1, juce::dontSendNotification);
    addAndMakeVisible(yDestCombo.get());

    springToggle = std::make_unique<juce::ToggleButton>("SPRING RETURN");
    ui::styleToggle(*springToggle);
    springToggle->setTooltip("When enabled, puck returns to center (0.5, 0.5) on release");
    springToggle->onClick = [this]
    {
        springToCenter = springToggle->getToggleState();
        if (speedSlider)
            speedSlider->setEnabled(springToCenter);
    };
    addAndMakeVisible(springToggle.get());

    speedLabel = ui::makeLabel("SPD:", 9.5f, ui::dim());
    speedLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(speedLabel.get());

    speedSlider = std::make_unique<juce::Slider>();
    speedSlider->setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    speedSlider->setRange(0.02, 1.0, 0.01);
    speedSlider->setValue(0.20, juce::dontSendNotification);
    speedSlider->setColour(juce::Slider::thumbColourId, ui::accent());
    speedSlider->setColour(juce::Slider::trackColourId, ui::accent());
    speedSlider->setColour(juce::Slider::backgroundColourId, ui::panelHi());
    speedSlider->setTooltip("Spring Return Speed (0.02 = Slow glide, 1.0 = Instant snap)");
    speedSlider->setEnabled(springToCenter);
    addAndMakeVisible(speedSlider.get());

    coordsLabel = ui::makeLabel("X: 0.50 | Y: 0.50", 10.5f, ui::accent());
    coordsLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(coordsLabel.get());

    loopIndex = defaultXMacro / 2;
    auto& loop = proc.getXYLooper(loopIndex);
    xDestCombo->setSelectedId(loop.xDestination.load(), juce::dontSendNotification);
    yDestCombo->setSelectedId(loop.yDestination.load(), juce::dontSendNotification);
    xDestCombo->onChange = [this] { proc.getXYLooper(loopIndex).xDestination.store(xDestCombo->getSelectedId()); };
    yDestCombo->onChange = [this] { proc.getXYLooper(loopIndex).yDestination.store(yDestCombo->getSelectedId()); };
    recordButton = std::make_unique<juce::TextButton>("RECORD");
    playButton = std::make_unique<juce::TextButton>("PLAY");
    clearButton = std::make_unique<juce::TextButton>("CLEAR");
    recordButton->setTooltip("Arm gesture recording. Press the puck to start a new loop; release to finish and play it.");
    recordButton->setClickingTogglesState(true);
    clearButton->setTooltip("Erase this XY pad's movement recording");
    for (auto* button : {recordButton.get(), playButton.get(), clearButton.get()}) { ui::styleButton(*button); addAndMakeVisible(button); }
    recordButton->onClick = [this]
    {
        auto& recorder = proc.getXYLooper(loopIndex);
        if (!recordButton->getToggleState() && recorder.recording.load()) recorder.finish();
    };
    playButton->onClick = [this]
    {
        auto& recorder = proc.getXYLooper(loopIndex);
        if (recorder.recording.load()) recorder.finish();
        else recorder.playing.store(!recorder.playing.load() && recorder.size() > 1);
    };
    clearButton->onClick = [this] { proc.getXYLooper(loopIndex).clear(); };
    loopSpeed = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    loopSpeed->setRange(.125, 8., .001);
    loopSpeed->setSkewFactorFromMidPoint(1.);
    loopSpeed->setValue(loop.speed.load(), juce::dontSendNotification);
    loopSpeed->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    loopSpeed->setTextValueSuffix("x");
    loopSpeed->setTooltip("Movement loop playback speed (1x = recorded speed)");
    loopSpeed->onValueChange = [this] { proc.getXYLooper(loopIndex).speed.store((float)loopSpeed->getValue()); };
    addAndMakeVisible(loopSpeed.get());
    startTimerHz(30);
}

XYPadComponent::~XYPadComponent()
{
    stopTimer();
    if (proc.getXYLooper(loopIndex).recording.load()) proc.getXYLooper(loopIndex).finish();
}

void XYPadComponent::resized()
{
    auto r = getLocalBounds().reduced(6);
    auto topRow = r.removeFromTop(24);
    titleLabel->setBounds(topRow.removeFromLeft(140));
    coordsLabel->setBounds(topRow.removeFromRight(120));

    auto cfgRow = r.removeFromTop(24);
    xDestLabel->setBounds(cfgRow.removeFromLeft(40));
    xDestCombo->setBounds(cfgRow.removeFromLeft((getWidth() - 110) / 2));
    cfgRow.removeFromLeft(6);
    yDestLabel->setBounds(cfgRow.removeFromLeft(40));
    yDestCombo->setBounds(cfgRow);
    auto springRow = r.removeFromTop(24);
    springToggle->setBounds(springRow.removeFromLeft(140));
    speedLabel->setBounds(springRow.removeFromLeft(30));
    speedSlider->setBounds(springRow.removeFromLeft(90));
    auto loopRow = r.removeFromTop(26);
    recordButton->setBounds(loopRow.removeFromLeft(68).reduced(1));
    playButton->setBounds(loopRow.removeFromLeft(56).reduced(1));
    clearButton->setBounds(loopRow.removeFromLeft(56).reduced(1));
    loopSpeed->setBounds(loopRow);
    r.removeFromTop(6);
    padArea = r.toFloat();
}

void XYPadComponent::paint(juce::Graphics& g)
{
    ui::drawForgedPlate(g, getLocalBounds().toFloat(), 6.f, false);

    if (padArea.isEmpty()) return;

    // Inner Touchpad Well
    g.setColour(juce::Colour(0xFF0D0907));
    g.fillRoundedRectangle(padArea, 4.f);
    g.setColour(ui::line());
    g.drawRoundedRectangle(padArea, 4.f, 1.f);

    // Subtle Grid lines
    g.setColour(juce::Colour(0xFFFF6600).withAlpha(0.08f));
    for (int i = 1; i < 4; ++i)
    {
        float gx = padArea.getX() + padArea.getWidth() * (i / 4.f);
        g.drawVerticalLine((int) gx, padArea.getY(), padArea.getBottom());
        float gy = padArea.getY() + padArea.getHeight() * (i / 4.f);
        g.drawHorizontalLine((int) gy, padArea.getX(), padArea.getRight());
    }

    // Center Crosshair
    float cx = padArea.getCentreX();
    float cy = padArea.getCentreY();
    g.setColour(ui::accent().withAlpha(0.2f));
    g.drawVerticalLine((int) cx, padArea.getY(), padArea.getBottom());
    g.drawHorizontalLine((int) cy, padArea.getX(), padArea.getRight());

    // Compute puck position
    float px = padArea.getX() + puckX * padArea.getWidth();
    float py = padArea.getBottom() - puckY * padArea.getHeight();

    // Crosshairs leading to puck
    g.setColour(ui::accent().withAlpha(isDragging ? 0.45f : 0.22f));
    g.drawVerticalLine((int) px, padArea.getY(), padArea.getBottom());
    g.drawHorizontalLine((int) py, padArea.getX(), padArea.getRight());

    // Puck Glow
    float glowR = isDragging ? 32.f : 20.f;
    juce::ColourGradient grad(ui::accentHot().withAlpha(isDragging ? 0.5f : 0.25f), px, py,
                             ui::accentHot().withAlpha(0.f), px, py, true);
    grad.addColour(0.4, ui::accent().withAlpha(isDragging ? 0.3f : 0.12f));
    grad.addColour(1.0, juce::Colour(0x00000000));
    g.setGradientFill(grad);
    g.fillEllipse(px - glowR, py - glowR, glowR * 2.f, glowR * 2.f);

    // Puck Body
    float bodyR = isDragging ? 9.f : 7.5f;
    g.setColour(ui::panelHi());
    g.fillEllipse(px - bodyR, py - bodyR, bodyR * 2.f, bodyR * 2.f);
    g.setColour(ui::accentHot());
    g.drawEllipse(px - bodyR, py - bodyR, bodyR * 2.f, bodyR * 2.f, 2.f);

    // Puck center dot
    g.setColour(juce::Colours::white);
    g.fillEllipse(px - 2.5f, py - 2.5f, 5.f, 5.f);
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown() && padArea.contains(e.position))
    {
        proc.getXYLooper(loopIndex).playing.store(false);
        isDragging = true;
        updateFromMouse(e);
        if (recordButton->getToggleState()) proc.getXYLooper(loopIndex).start(puckX, puckY);
        repaint();
    }
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        updateFromMouse(e);
        repaint();
    }
}

void XYPadComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (isDragging)
    {
        auto& recorder = proc.getXYLooper(loopIndex);
        if (recorder.recording.load()) { recorder.append(puckX, puckY); recorder.finish(); }
        isDragging = false;
        if (springToCenter && !recorder.playing.load())
        {
            if (speedSlider != nullptr && speedSlider->getValue() >= 0.98)
            {
                puckX = 0.5f;
                puckY = 0.5f;
                applyPuckToParams();
            }
        }
        repaint();
    }
}

void XYPadComponent::updateFromMouse(const juce::MouseEvent& e)
{
    if (padArea.getWidth() <= 0.f || padArea.getHeight() <= 0.f) return;

    puckX = juce::jlimit(0.0f, 1.0f, (e.position.x - padArea.getX()) / padArea.getWidth());
    puckY = juce::jlimit(0.0f, 1.0f, (padArea.getBottom() - e.position.y) / padArea.getHeight());

    if (coordsLabel)
        coordsLabel->setText("X: " + juce::String(puckX, 2) + " | Y: " + juce::String(puckY, 2),
                             juce::dontSendNotification);

    applyPuckToParams();
}

static juce::String getParamIdFromChoice(int choiceId)
{
    if (choiceId >= 1 && choiceId <= kNumMacros)
        return "m" + juce::String(choiceId - 1);
    if (choiceId == 101) return "master";
    if (choiceId == 102) return "revsize";
    if (choiceId == 103) return "dlytime";
    return {};
}

void XYPadComponent::applyPuckToParams()
{
    auto setParamNormalized = [this](const juce::String& paramId, float normVal)
    {
        if (paramId.isEmpty()) return;
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter(paramId)))
            p->setValueNotifyingHost(normVal);
        for (auto& aux : proc.getAuxManager().auxParams)
        {
            if (paramId == "revsize" && (aux.fxType == AUX_FX_REVERB || aux.fxType == AUX_FX_PLATE || aux.fxType == AUX_FX_SPRING || aux.fxType == AUX_FX_GATED_VERB)) aux.p1 = normVal;
            if (paramId == "dlytime" && (aux.fxType == AUX_FX_DELAY || aux.fxType == AUX_FX_PINGPONG)) aux.p1 = normVal;
        }
    };

    setParamNormalized(getParamIdFromChoice(xDestCombo->getSelectedId()), puckX);
    setParamNormalized(getParamIdFromChoice(yDestCombo->getSelectedId()), puckY);
}

void XYPadComponent::syncPuckFromParams()
{
    if (isDragging) return;

    auto getParamNormalized = [this](const juce::String& paramId) -> float
    {
        if (paramId.isEmpty()) return 0.5f;
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter(paramId)))
            return p->getValue();
        return 0.5f;
    };

    float curX = getParamNormalized(getParamIdFromChoice(xDestCombo->getSelectedId()));
    float curY = getParamNormalized(getParamIdFromChoice(yDestCombo->getSelectedId()));

    if (std::abs(curX - puckX) > 0.002f || std::abs(curY - puckY) > 0.002f)
    {
        puckX = curX;
        puckY = curY;
        if (coordsLabel)
            coordsLabel->setText("X: " + juce::String(puckX, 2) + " | Y: " + juce::String(puckY, 2),
                                 juce::dontSendNotification);
        repaint();
    }
}

void XYPadComponent::timerCallback()
{
    auto& recorder = proc.getXYLooper(loopIndex);
    recordButton->setButtonText(recorder.recording.load() ? "REC..." : "RECORD");
    playButton->setButtonText(recorder.playing.load() ? "STOP" : "PLAY");
    playButton->setEnabled(recorder.size() > 1);
    if (recorder.playing.load())
    {
        puckX = recorder.x.load(); puckY = recorder.y.load();
        coordsLabel->setText("LOOP " + juce::String(recorder.size() / 30., 1) + "s", juce::dontSendNotification);
        repaint();
        return;
    }
    if (recorder.recording.load()) recorder.append(puckX, puckY);
    if (! isDragging && springToCenter)
    {
        const float dx = 0.5f - puckX;
        const float dy = 0.5f - puckY;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.0005f)
        {
            const float factor = speedSlider != nullptr ? (float) speedSlider->getValue() : 0.20f;
            if (factor >= 0.95f || dist < 0.002f)
            {
                puckX = 0.5f;
                puckY = 0.5f;
            }
            else
            {
                puckX += dx * factor;
                puckY += dy * factor;
            }
            applyPuckToParams();
            if (coordsLabel)
                coordsLabel->setText("X: " + juce::String(puckX, 2) + " | Y: " + juce::String(puckY, 2),
                                     juce::dontSendNotification);
            repaint();
            return;
        }
    }

    if (recorder.recording.load())
    {
        coordsLabel->setText("REC " + juce::String(recorder.size() / 30., 1) + "s", juce::dontSendNotification);
        repaint();
        return;
    }
    syncPuckFromParams();
}

// ===========================================================================
// PerformancePage Implementation
// ===========================================================================
PerformancePage::PerformancePage(Forge64Processor& processor, ModRingKnob::Services& svcs)
    : proc(processor), services(svcs)
{
    pageTitle = ui::makeLabel("PERFORMANCE & MACRO CONTROL", 14.f, ui::accentHot());
    pageTitle->setFont(uiFont(14.f, true));
    addAndMakeVisible(pageTitle.get());

    resetMacrosBtn = std::make_unique<juce::TextButton>("RESET MACROS");
    ui::styleButton(*resetMacrosBtn);
    resetMacrosBtn->setTooltip("Reset all 8 macros to default 50%");
    resetMacrosBtn->onClick = [this]
    {
        for (int i = 0; i < kNumMacros; ++i)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter("m" + juce::String(i))))
                p->setValueNotifyingHost(0.5f);
        }
    };
    addAndMakeVisible(resetMacrosBtn.get());

    randomMacrosBtn = std::make_unique<juce::TextButton>("RANDOMIZE");
    ui::styleButton(*randomMacrosBtn);
    randomMacrosBtn->setTooltip("Generate random creative macro settings");
    randomMacrosBtn->onClick = [this]
    {
        juce::Random rnd;
        for (int i = 0; i < kNumMacros; ++i)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter("m" + juce::String(i))))
                p->setValueNotifyingHost(rnd.nextFloat() * 0.8f + 0.1f);
        }
    };
    addAndMakeVisible(randomMacrosBtn.get());

    // 8 Macro Dials
    for (int i = 0; i < kNumMacros; ++i)
    {
        auto& slot = macroSlots[(size_t) i];
        slot.nameEdit = std::make_unique<juce::Label>("", "MACRO " + juce::String(i + 1));
        slot.nameEdit->setFont(uiFont(10.5f, true));
        slot.nameEdit->setJustificationType(juce::Justification::centred);
        slot.nameEdit->setColour(juce::Label::textColourId, ui::accent());
        slot.nameEdit->setEditable(false, true);
        slot.nameEdit->setTooltip("Double-click to rename this macro");
        addAndMakeVisible(slot.nameEdit.get());

        slot.knob = std::make_unique<ModRingKnob>("m" + juce::String(i), "M" + juce::String(i + 1), services);
        if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter("m" + juce::String(i))))
            slot.attachment = std::make_unique<juce::SliderParameterAttachment>(*par, *slot.knob, nullptr);
        addAndMakeVisible(slot.knob.get());
    }

    // Dual XY Expression Pads
    xyPadA = std::make_unique<XYPadComponent>(proc, "EXPRESSION PAD A (M1/M2)", 0, 1);
    addAndMakeVisible(xyPadA.get());

    xyPadB = std::make_unique<XYPadComponent>(proc, "EXPRESSION PAD B (M3/M4)", 2, 3);
    addAndMakeVisible(xyPadB.get());
    startTimerHz(30);

}

void PerformancePage::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());

    auto drawCard = [&](int y, int h)
    {
        auto box = juce::Rectangle<float>(6.f, (float) y, (float) getWidth() - 12.f, (float) h);
        ui::drawForgedPlate(g, box, 6.f, false);
    };

    drawCard(36, 120);  // Macro Dials plate
    drawCard(162, juce::jmax(240, getHeight() - 168)); // Dual XY loop recorders
}

void PerformancePage::timerCallback()
{
    for (int k = 0; k < kNumMacros; ++k)
    {
        const float motion = proc.performanceValue(k);
        auto* parameter = proc.getAPVTS().getParameter("m" + juce::String(k));
        macroSlots[(size_t)k].knob->setValue(motion >= 0.f ? motion : parameter->getValue(), juce::dontSendNotification);
    }
}

void PerformancePage::resized()
{
    const int w = getWidth();
    auto topBar = juce::Rectangle<int>(8, 6, w - 16, 26);
    pageTitle->setBounds(topBar.removeFromLeft(280));
    randomMacrosBtn->setBounds(topBar.removeFromRight(90));
    topBar.removeFromRight(8);
    resetMacrosBtn->setBounds(topBar.removeFromRight(100));

    // Macro dials in a row of 8
    const int macroW = (w - 24) / 8;
    for (int i = 0; i < kNumMacros; ++i)
    {
        int mx = 12 + i * macroW;
        macroSlots[(size_t) i].nameEdit->setBounds(mx, 40, macroW - 4, 18);
        macroSlots[(size_t) i].knob->setBounds(mx + (macroW - 68) / 2, 58, 68, 88);
    }

    // Dual XY Pads side-by-side
    const int padW = (w - 28) / 2;
    xyPadA->setBounds(12, 168, padW, juce::jmax(228, getHeight() - 180));
    xyPadB->setBounds(16 + padW, 168, padW, juce::jmax(228, getHeight() - 180));


}

} // namespace f64
