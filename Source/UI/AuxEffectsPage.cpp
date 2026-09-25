#include "AuxEffectsPage.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

AuxEffectsPage::AuxStrip::AuxStrip(AuxEffectsPage& owner, int auxIndex)
    : page(owner), idx(auxIndex)
{
    titleLabel = ui::makeLabel("AUX " + juce::String(idx + 1), 13.f, ui::accent());
    titleLabel->setFont(uiFont(13.f, true));
    addAndMakeVisible(titleLabel.get());

    enableBtn = std::make_unique<juce::ToggleButton>("ON");
    ui::styleToggle(*enableBtn);
    enableBtn->setToggleState(page.proc.getAuxManager().auxParams[idx].enabled, juce::dontSendNotification);
    enableBtn->onClick = [this]
    {
        page.proc.getAuxManager().auxParams[idx].enabled = enableBtn->getToggleState();
    };
    addAndMakeVisible(enableBtn.get());

    fxCombo = std::make_unique<juce::ComboBox>();
    ui::styleCombo(*fxCombo);
    for (int i = 0; i < AUX_FX_COUNT; ++i)
        fxCombo->addItem(auxFxTypeName(i), i + 1);

    fxCombo->setSelectedId(page.proc.getAuxManager().auxParams[idx].fxType + 1, juce::dontSendNotification);
    fxCombo->onChange = [this]
    {
        const int type = fxCombo->getSelectedId() - 1;
        page.proc.getAuxManager().auxParams[idx].fxType = type;
        updateControlLabels(type);
    };
    addAndMakeVisible(fxCombo.get());

    auto makeAuxSlider = [&](std::unique_ptr<juce::Slider>& sl, std::unique_ptr<juce::Label>& lb,
                             float def, float minV, float maxV, const char* name,
                             std::function<void(float)> onChange)
    {
        sl = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        sl->setRange(minV, maxV, 0.01);
        sl->setValue(def, juce::dontSendNotification);
        sl->setDoubleClickReturnValue(true, def);
        sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        sl->setColour(juce::Slider::thumbColourId, ui::accent());
        sl->setColour(juce::Slider::rotarySliderFillColourId, ui::accent());
        sl->setColour(juce::Slider::rotarySliderOutlineColourId, ui::panelHi().darker(0.3f));
        sl->setColour(juce::Slider::textBoxTextColourId, ui::txt());
        sl->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sl->onValueChange = [slPtr = sl.get(), onChange] { onChange((float) slPtr->getValue()); };
        addAndMakeVisible(sl.get());

        lb = ui::makeLabel(name, 9.5f, ui::dim());
        lb->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lb.get());
    };

    auto& p = page.proc.getAuxManager().auxParams[idx];
    makeAuxSlider(p1Slider, p1Label, p.p1, 0.f, 1.f, "PARAM 1", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p1 = v;
        if (p1Slider) p1Slider->updateText();
    });
    makeAuxSlider(p2Slider, p2Label, p.p2, 0.f, 1.f, "PARAM 2", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p2 = v;
        if (p2Slider) p2Slider->updateText();
    });
    makeAuxSlider(p3Slider, p3Label, p.p3, 0.f, 1.f, "PARAM 3", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p3 = v;
        if (p3Slider) p3Slider->updateText();
    });
    makeAuxSlider(p4Slider, p4Label, p.p4, 0.f, 1.f, "PARAM 4", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p4 = v;
        const int type = page.proc.getAuxManager().auxParams[idx].fxType;
        if (type == AUX_FX_DELAY || type == AUX_FX_PINGPONG)
        {
            const bool sync = (v >= 0.5f);
            if (p1Label) p1Label->setText(sync ? "DIVISION" : "TIME", juce::dontSendNotification);
            if (p1Slider) p1Slider->updateText();
        }
        if (p4Slider) p4Slider->updateText();
    });

    makeAuxSlider(returnSlider, returnLabel, p.returnLevel, 0.f, 1.5f, "RETURN", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].returnLevel = v;
        if (returnSlider) returnSlider->updateText();
    });
    makeAuxSlider(panSlider, panLabel, p.returnPan, -1.f, 1.f, "PAN", [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].returnPan = v;
        if (panSlider) panSlider->updateText();
    });

    p1Slider->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 0, (float) v, ap.p4);
    };
    p2Slider->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 1, (float) v, ap.p4);
    };
    p3Slider->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 2, (float) v, ap.p4);
    };
    p4Slider->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 3, (float) v, ap.p4);
    };

    p1Slider->valueFromTextFunction = [this](const juce::String& s)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return parseAuxParamText(ap.fxType, 0, s, ap.p4);
    };
    p2Slider->valueFromTextFunction = [this](const juce::String& s)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return parseAuxParamText(ap.fxType, 1, s, ap.p4);
    };
    p3Slider->valueFromTextFunction = [this](const juce::String& s)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return parseAuxParamText(ap.fxType, 2, s, ap.p4);
    };
    p4Slider->valueFromTextFunction = [this](const juce::String& s)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return parseAuxParamText(ap.fxType, 3, s, ap.p4);
    };

    returnSlider->textFromValueFunction = [](double v) { return formatAuxReturnLevel((float) v); };
    panSlider->textFromValueFunction    = [](double v) { return formatAuxReturnPan((float) v); };

    updateControlLabels(p.fxType);
}

void AuxEffectsPage::AuxStrip::updateControlLabels(int fxType)
{
    auto& p = page.proc.getAuxManager().auxParams[idx];
    if (fxType == AUX_FX_DELAY || fxType == AUX_FX_PINGPONG)
        p4Slider->setRange(0.0, 1.0, 1.0);
    else
        p4Slider->setRange(0.0, 1.0, 0.01);

    switch (fxType)
    {
        case AUX_FX_REVERB:
            p1Label->setText("SIZE", juce::dontSendNotification);
            p2Label->setText("DAMP", juce::dontSendNotification);
            p3Label->setText("PRE-DLY", juce::dontSendNotification);
            p4Label->setText("WIDTH", juce::dontSendNotification);
            break;
        case AUX_FX_DELAY:
            p1Label->setText(p.p4 >= 0.5f ? "DIVISION" : "TIME", juce::dontSendNotification);
            p2Label->setText("FEEDBACK", juce::dontSendNotification);
            p3Label->setText("HI-DAMP", juce::dontSendNotification);
            p4Label->setText("SYNC", juce::dontSendNotification);
            break;
        case AUX_FX_DRIVE:
            p1Label->setText("DRIVE", juce::dontSendNotification);
            p2Label->setText("TONE", juce::dontSendNotification);
            p3Label->setText("BIAS", juce::dontSendNotification);
            p4Label->setText("MIX", juce::dontSendNotification);
            break;
        case AUX_FX_CHORUS:
        case AUX_FX_FLANGER:
        case AUX_FX_PHASER:
            p1Label->setText("RATE", juce::dontSendNotification);
            p2Label->setText("DEPTH", juce::dontSendNotification);
            p3Label->setText("FEEDBACK", juce::dontSendNotification);
            p4Label->setText("MIX", juce::dontSendNotification);
            break;
        case AUX_FX_COMP:
            p1Label->setText("THRESH", juce::dontSendNotification);
            p2Label->setText("RATIO", juce::dontSendNotification);
            p3Label->setText("ATTACK", juce::dontSendNotification);
            p4Label->setText("RELEASE", juce::dontSendNotification);
            break;
        case AUX_FX_FILTER:
            p1Label->setText("CUTOFF", juce::dontSendNotification);
            p2Label->setText("RESO", juce::dontSendNotification);
            p3Label->setText("MODE", juce::dontSendNotification);
            p4Label->setText("DRIVE", juce::dontSendNotification);
            break;
        case AUX_FX_SHIMMER:
            p1Label->setText("DECAY", juce::dontSendNotification);
            p2Label->setText("SHIMMER", juce::dontSendNotification);
            p3Label->setText("TONE", juce::dontSendNotification);
            p4Label->setText("WIDTH", juce::dontSendNotification);
            break;
        case AUX_FX_PINGPONG:
            p1Label->setText(p.p4 >= 0.5f ? "DIVISION" : "TIME", juce::dontSendNotification);
            p2Label->setText("FEEDBACK", juce::dontSendNotification);
            p3Label->setText("HI-DAMP", juce::dontSendNotification);
            p4Label->setText("SYNC", juce::dontSendNotification);
            break;
        case AUX_FX_GATED_VERB:
            p1Label->setText("GATE TIME", juce::dontSendNotification);
            p2Label->setText("DENSITY", juce::dontSendNotification);
            p3Label->setText("TONE", juce::dontSendNotification);
            p4Label->setText("WIDTH", juce::dontSendNotification);
            break;
        case AUX_FX_TUBE:
            p1Label->setText("DRIVE", juce::dontSendNotification);
            p2Label->setText("BIAS", juce::dontSendNotification);
            p3Label->setText("WARMTH", juce::dontSendNotification);
            p4Label->setText("MIX", juce::dontSendNotification);
            break;
        case AUX_FX_PITCH:
            p1Label->setText("PITCH", juce::dontSendNotification);
            p2Label->setText("FINE", juce::dontSendNotification);
            p3Label->setText("FEEDBACK", juce::dontSendNotification);
            p4Label->setText("MIX", juce::dontSendNotification);
            break;
        case AUX_FX_SPRING:
            p1Label->setText("TENSION", juce::dontSendNotification);
            p2Label->setText("BOING", juce::dontSendNotification);
            p3Label->setText("TONE", juce::dontSendNotification);
            p4Label->setText("MIX", juce::dontSendNotification);
            break;
        default:
            p1Label->setText("PARAM 1", juce::dontSendNotification);
            p2Label->setText("PARAM 2", juce::dontSendNotification);
            p3Label->setText("PARAM 3", juce::dontSendNotification);
            p4Label->setText("PARAM 4", juce::dontSendNotification);
            break;
    }

    if (p1Slider) p1Slider->updateText();
    if (p2Slider) p2Slider->updateText();
    if (p3Slider) p3Slider->updateText();
    if (p4Slider) p4Slider->updateText();
    if (returnSlider) returnSlider->updateText();
    if (panSlider) panSlider->updateText();
}

void AuxEffectsPage::AuxStrip::paint(juce::Graphics& g)
{
    auto rc = getLocalBounds().toFloat();

    // Chassis frame: Forged console strip
    ui::drawForgedPlate(g, rc, 8.f, false);

    // Live Activity LED: glowing molten spark with radiant heat halo
    const float meterVal = page.proc.getAuxManager().getAuxMeter(idx);
    const bool active = meterVal > 0.01f;
    if (active)
    {
        g.setColour(ui::accentGlow().withAlpha(0.45f));
        g.fillEllipse(rc.getX() + 8.f, rc.getY() + 10.f, 11.f, 11.f);
        g.setColour(ui::accentHot());
        g.fillEllipse(rc.getX() + 10.f, rc.getY() + 12.f, 7.f, 7.f);
    }
    else
    {
        g.setColour(ui::line().darker(0.3f));
        g.fillEllipse(rc.getX() + 10.f, rc.getY() + 12.f, 7.f, 7.f);
    }
}

void AuxEffectsPage::AuxStrip::resized()
{
    const int w = getWidth();
    int y = 8;

    titleLabel->setBounds(24, y, 64, 22);
    enableBtn->setBounds(w - 56, y, 48, 22);
    y += 30;

    fxCombo->setBounds(10, y, w - 20, 26);
    y += 38;

    auto layoutKnob = [&](std::unique_ptr<juce::Slider>& sl, std::unique_ptr<juce::Label>& lb, int kx, int ky)
    {
        sl->setBounds(kx, ky, 68, 70);
        lb->setBounds(kx - 4, ky + 70, 76, 14);
    };

    const int col1 = (w / 2) - 72;
    const int col2 = (w / 2) + 4;

    layoutKnob(p1Slider, p1Label, col1, y);
    layoutKnob(p2Slider, p2Label, col2, y);
    y += 92;

    layoutKnob(p3Slider, p3Label, col1, y);
    layoutKnob(p4Slider, p4Label, col2, y);
    y += 96;

    // Master Return & Pan
    layoutKnob(returnSlider, returnLabel, col1, y);
    layoutKnob(panSlider, panLabel, col2, y);
}

// ---------------------------------------------------------------------------
AuxEffectsPage::AuxEffectsPage(Forge64Processor& processor, ModRingKnob::Services& svcs)
    : proc(processor), services(svcs)
{
    for (int i = 0; i < 4; ++i)
    {
        strips[(size_t) i] = std::make_unique<AuxStrip>(*this, i);
        addAndMakeVisible(strips[(size_t) i].get());
    }
    startTimerHz(25);
}

AuxEffectsPage::~AuxEffectsPage()
{
    stopTimer();
}

void AuxEffectsPage::timerCallback()
{
    repaint();
}

void AuxEffectsPage::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());

    g.setFont(uiFont(14.f, true));
    g.setColour(ui::accentHot());
    g.drawText("STUDIO AUX SEND PROCESSORS // FORGE FX RACKS", 14, 8, 420, 20, juce::Justification::left);
}

void AuxEffectsPage::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int topY = 32;
    const int stripW = (w - 24) / 4;
    const int stripH = h - topY - 12;

    for (int i = 0; i < 4; ++i)
    {
        strips[(size_t) i]->setBounds(10 + i * (stripW + 4), topY, stripW, stripH);
    }
}

} // namespace f64
