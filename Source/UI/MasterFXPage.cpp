#include "MasterFXPage.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

MasterFXPage::MasterFXPage(Forge64Processor& processor, ModRingKnob::Services& svcs)
    : proc(processor), services(svcs)
{
    auto makeKnob = [&](std::unique_ptr<juce::Slider>& sl, std::unique_ptr<juce::Label>& lb,
                        float def, float mn, float mx, const char* name,
                        std::function<void(float)> onChange)
    {
        sl = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        sl->setRange(mn, mx, 0.1);
        sl->setValue(def, juce::dontSendNotification);
        sl->setDoubleClickReturnValue(true, def);
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

    auto& mp = proc.getAuxManager().masterParams;

    // 1. Master Bus Compressor
    compTitle = ui::makeLabel("MASTER BUS COMPRESSOR (VCA GLUE)", 12.f, ui::accent());
    compTitle->setFont(uiFont(12.f, true));
    addAndMakeVisible(compTitle.get());

    compEnableBtn = std::make_unique<juce::ToggleButton>("ON");
    ui::styleToggle(*compEnableBtn);
    compEnableBtn->setToggleState(mp.compOn, juce::dontSendNotification);
    compEnableBtn->onClick = [this] { proc.getAuxManager().masterParams.compOn = compEnableBtn->getToggleState(); };
    addAndMakeVisible(compEnableBtn.get());

    makeKnob(compThreshSlider, compThreshLabel, mp.compThresh, -40.f, 0.f, "THRESHOLD",
             [this](float v) { proc.getAuxManager().masterParams.compThresh = v; });
    makeKnob(compRatioSlider, compRatioLabel, mp.compRatio, 1.5f, 10.f, "RATIO",
             [this](float v) { proc.getAuxManager().masterParams.compRatio = v; });
    makeKnob(compAtkSlider, compAtkLabel, mp.compAtk, 0.1f, 30.f, "ATTACK ms",
             [this](float v) { proc.getAuxManager().masterParams.compAtk = v; });
    makeKnob(compRelSlider, compRelLabel, mp.compRel, 50.f, 1200.f, "RELEASE ms",
             [this](float v) { proc.getAuxManager().masterParams.compRel = v; });
    makeKnob(compMakeupSlider, compMakeupLabel, mp.compMakeup, 0.f, 18.f, "MAKEUP dB",
             [this](float v) { proc.getAuxManager().masterParams.compMakeup = v; });

    // 2. Master EQ
    eqTitle = ui::makeLabel("MASTERING 4-BAND EQUALIZER", 12.f, ui::accent());
    eqTitle->setFont(uiFont(12.f, true));
    addAndMakeVisible(eqTitle.get());

    eqEnableBtn = std::make_unique<juce::ToggleButton>("ON");
    ui::styleToggle(*eqEnableBtn);
    eqEnableBtn->setToggleState(mp.eqOn, juce::dontSendNotification);
    eqEnableBtn->onClick = [this] { proc.getAuxManager().masterParams.eqOn = eqEnableBtn->getToggleState(); };
    addAndMakeVisible(eqEnableBtn.get());

    makeKnob(eqLowSlider, eqLowLabel, mp.eqLowGain, -12.f, 12.f, "LOW 80Hz",
             [this](float v) { proc.getAuxManager().masterParams.eqLowGain = v; });
    makeKnob(eqLowMidSlider, eqLowMidLabel, mp.eqLowMidGain, -12.f, 12.f, "L-MID 450Hz",
             [this](float v) { proc.getAuxManager().masterParams.eqLowMidGain = v; });
    makeKnob(eqHiMidSlider, eqHiMidLabel, mp.eqHiMidGain, -12.f, 12.f, "H-MID 2.5kHz",
             [this](float v) { proc.getAuxManager().masterParams.eqHiMidGain = v; });
    makeKnob(eqHighSlider, eqHighLabel, mp.eqHighGain, -12.f, 12.f, "HIGH 10kHz",
             [this](float v) { proc.getAuxManager().masterParams.eqHighGain = v; });

    // 3. Master Tape Drive & Limiter
    masterTitle = ui::makeLabel("MASTER TAPE DRIVE & CEILING LIMITER", 12.f, ui::accent());
    masterTitle->setFont(uiFont(12.f, true));
    addAndMakeVisible(masterTitle.get());

    driveEnableBtn = std::make_unique<juce::ToggleButton>("TAPE ON");
    ui::styleToggle(*driveEnableBtn);
    driveEnableBtn->setToggleState(mp.driveOn, juce::dontSendNotification);
    driveEnableBtn->onClick = [this] { proc.getAuxManager().masterParams.driveOn = driveEnableBtn->getToggleState(); };
    addAndMakeVisible(driveEnableBtn.get());

    limiterEnableBtn = std::make_unique<juce::ToggleButton>("LIMIT ON");
    ui::styleToggle(*limiterEnableBtn);
    limiterEnableBtn->setToggleState(mp.limiterOn, juce::dontSendNotification);
    limiterEnableBtn->onClick = [this] { proc.getAuxManager().masterParams.limiterOn = limiterEnableBtn->getToggleState(); };
    addAndMakeVisible(limiterEnableBtn.get());

    makeKnob(driveSlider, driveLabel, mp.drive, 0.f, 1.f, "TAPE WARMTH",
             [this](float v) { proc.getAuxManager().masterParams.drive = v; });
    makeKnob(ceilingSlider, ceilingLabel, mp.ceiling, -3.f, 0.f, "CEILING dB",
             [this](float v) { proc.getAuxManager().masterParams.ceiling = v; });

    startTimerHz(25);
}

MasterFXPage::~MasterFXPage()
{
    stopTimer();
}

void MasterFXPage::timerCallback()
{
    repaint();
}

void MasterFXPage::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());

    auto drawCard = [&](int y, int h)
    {
        auto box = juce::Rectangle<float>(10.f, (float) y, (float) getWidth() - 20.f, (float) h);
        ui::drawForgedPlate(g, box, 8.f, false);
    };

    drawCard(36, 136);  // Master Bus Compressor
    drawCard(184, 136); // Master EQ
    drawCard(332, 136); // Tape Drive & Limiter

    // Gain reduction meter on Compressor card (molten heat VU)
    const float gr = proc.getAuxManager().getMasterCompGR();
    const float meterX = (float) getWidth() - 230.f;
    auto meterRc = juce::Rectangle<float>(meterX, 58.f, 140.f, 12.f);
    g.setColour(juce::Colour(0xFF0C0807));
    g.fillRoundedRectangle(meterRc, 3.f);
    g.setColour(ui::line().withAlpha(0.45f));
    g.drawRoundedRectangle(meterRc, 3.f, 1.f);

    const float grNorm = juce::jlimit(0.f, 1.f, gr / 12.f);
    if (grNorm > 0.005f)
    {
        auto fillRc = meterRc.withWidth(meterRc.getWidth() * grNorm);
        g.setGradientFill(juce::ColourGradient(ui::accent(), fillRc.getX(), fillRc.getY(),
                                               ui::accentHot(), fillRc.getRight(), fillRc.getY(), false));
        g.fillRoundedRectangle(fillRc, 3.f);
    }
    g.setFont(uiFont(9.f, true));
    g.setColour(grNorm > 0.05f ? ui::accentHot() : ui::dim());
    g.drawText("GR: " + juce::String(gr, 1) + " dB", meterX + 150.f, 56.f, 65.f, 16.f, juce::Justification::left);
}

void MasterFXPage::resized()
{
    const int w = getWidth();

    // Card 1: Master Bus Compressor
    int y = 42;
    compTitle->setBounds(20, y, 320, 22);
    compEnableBtn->setBounds(w - 74, y, 54, 22);

    int kx = 24;
    y += 32;
    for (auto* k : { compThreshSlider.get(), compRatioSlider.get(), compAtkSlider.get(),
                     compRelSlider.get(), compMakeupSlider.get() })
    {
        k->setBounds(kx, y, 68, 68);
        kx += 76;
    }
    kx = 24;
    for (auto* l : { compThreshLabel.get(), compRatioLabel.get(), compAtkLabel.get(),
                     compRelLabel.get(), compMakeupLabel.get() })
    {
        l->setBounds(kx - 4, y + 68, 76, 14);
        kx += 76;
    }

    // Card 2: Master EQ
    y = 190;
    eqTitle->setBounds(20, y, 300, 22);
    eqEnableBtn->setBounds(w - 74, y, 54, 22);

    kx = 24;
    y += 32;
    for (auto* k : { eqLowSlider.get(), eqLowMidSlider.get(), eqHiMidSlider.get(), eqHighSlider.get() })
    {
        k->setBounds(kx, y, 68, 68);
        kx += 76;
    }
    kx = 24;
    for (auto* l : { eqLowLabel.get(), eqLowMidLabel.get(), eqHiMidLabel.get(), eqHighLabel.get() })
    {
        l->setBounds(kx - 4, y + 68, 76, 14);
        kx += 76;
    }

    // Card 3: Tape Drive & Limiter
    y = 338;
    masterTitle->setBounds(20, y, 340, 22);
    driveEnableBtn->setBounds(w - 170, y, 76, 22);
    limiterEnableBtn->setBounds(w - 86, y, 76, 22);

    kx = 24;
    y += 32;
    for (auto* k : { driveSlider.get(), ceilingSlider.get() })
    {
        k->setBounds(kx, y, 68, 68);
        kx += 76;
    }
    kx = 24;
    for (auto* l : { driveLabel.get(), ceilingLabel.get() })
    {
        l->setBounds(kx - 4, y + 68, 76, 14);
        kx += 76;
    }
}

} // namespace f64
