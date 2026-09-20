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
    };
    addAndMakeVisible(springToggle.get());

    coordsLabel = ui::makeLabel("X: 0.50 | Y: 0.50", 10.5f, ui::accent());
    coordsLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(coordsLabel.get());

    startTimerHz(30);
}

XYPadComponent::~XYPadComponent()
{
    stopTimer();
}

void XYPadComponent::resized()
{
    auto r = getLocalBounds().reduced(6);
    auto topRow = r.removeFromTop(24);
    titleLabel->setBounds(topRow.removeFromLeft(140));
    coordsLabel->setBounds(topRow.removeFromRight(120));

    auto cfgRow = r.removeFromTop(24);
    xDestLabel->setBounds(cfgRow.removeFromLeft(46));
    xDestCombo->setBounds(cfgRow.removeFromLeft(90));
    cfgRow.removeFromLeft(8);
    yDestLabel->setBounds(cfgRow.removeFromLeft(46));
    yDestCombo->setBounds(cfgRow.removeFromLeft(90));
    cfgRow.removeFromLeft(8);
    springToggle->setBounds(cfgRow.removeFromLeft(110));

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
    if (padArea.contains(e.position))
    {
        isDragging = true;
        updateFromMouse(e);
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
        isDragging = false;
        if (springToCenter)
        {
            puckX = 0.5f;
            puckY = 0.5f;
            applyPuckToParams();
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

    // Live Audition Trigger Strip
    for (int i = 0; i < 16; ++i)
    {
        liveTrigBtns[(size_t) i] = std::make_unique<juce::TextButton>(juce::String(i + 1).paddedLeft('0', 2));
        ui::styleButton(*liveTrigBtns[(size_t) i]);
        liveTrigBtns[(size_t) i]->setColour(juce::TextButton::buttonColourId, ui::panelHi());
        liveTrigBtns[(size_t) i]->onClick = [this, i]
        {
            proc.triggerAudition(i, 0.9f);
        };
        addAndMakeVisible(liveTrigBtns[(size_t) i].get());
    }
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
    drawCard(162, 340); // Dual XY Expression Pads plate
    drawCard(508, 64);  // Quick Pad Audition Strip plate
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
    xyPadA->setBounds(12, 168, padW, 328);
    xyPadB->setBounds(16 + padW, 168, padW, 328);

    // Live Audition Trigger Strip
    const int btnW = (w - 24) / 16;
    for (int i = 0; i < 16; ++i)
    {
        liveTrigBtns[(size_t) i]->setBounds(12 + i * btnW, 520, btnW - 2, 38);
    }
}

} // namespace f64
