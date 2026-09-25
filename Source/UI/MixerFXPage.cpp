#include "MixerFXPage.h"
#include "../PluginProcessor.h"

namespace f64 {

static juce::Colour getAuxThemeColour(int idx)
{
    switch (idx)
    {
        case 0: return juce::Colour(0xFF00B4D8); // Cyan - Reverb
        case 1: return juce::Colour(0xFF2EC4B6); // Mint Teal - Delay
        case 2: return juce::Colour(0xFFFF6B00); // Molten Orange - Saturation / Drive
        case 3: return juce::Colour(0xFF9D4EDD); // Electric Violet - Modulation / Filter
        default: return ui::accent();
    }
}

// ---------------------------------------------------------------------------
// Real-time 4-Band Master EQ Graphical Frequency Visualizer
// ---------------------------------------------------------------------------
MasterEQGraphView::MasterEQGraphView(Forge64Processor& processor)
    : proc(processor)
{
    setTooltip("4-Band Master EQ curve. Click and drag band nodes (Low, Lo-Mid, Hi-Mid, High) to adjust gain.");
}

void MasterEQGraphView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.f);

    // Deep obsidian display screen
    g.setColour(juce::Colour(0xFF100C0B));
    g.fillRoundedRectangle(bounds, 4.f);
    g.setColour(ui::line().withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 4.f, 1.0f);

    const float rx = bounds.getX() + 6.f;
    const float ry = bounds.getY() + 6.f;
    const float rw = bounds.getWidth() - 12.f;
    const float rh = bounds.getHeight() - 12.f;
    const float midY = ry + rh * 0.5f;

    auto freqToX = [&](double f) -> float
    {
        return rx + (float) ((std::log10(f / 20.0) / 3.0) * rw);
    };

    auto gainToY = [&](float gDb) -> float
    {
        return midY - (gDb / 15.f) * (rh * 0.44f);
    };

    // Frequency gridlines
    static const double gridFreqs[] = { 100.0, 1000.0, 10000.0 };
    static const char* gridLabels[] = { "100Hz", "1kHz", "10kHz" };
    g.setFont(uiFont(9.f));
    for (int i = 0; i < 3; ++i)
    {
        float gx = freqToX(gridFreqs[i]);
        g.setColour(ui::line().withAlpha(0.45f));
        g.drawVerticalLine((int) gx, ry, ry + rh);
        g.setColour(ui::dim().withAlpha(0.6f));
        g.drawText(gridLabels[i], (int) gx - 20, (int) (ry + rh - 12.f), 40, 12, juce::Justification::centred);
    }

    // Gain gridlines
    g.setColour(ui::line().withAlpha(0.35f));
    g.drawHorizontalLine((int) gainToY(6.f), rx, rx + rw);
    g.drawHorizontalLine((int) gainToY(-6.f), rx, rx + rw);
    g.setColour(ui::dim().withAlpha(0.3f));
    g.drawHorizontalLine((int) midY, rx, rx + rw);

    // Gain labels
    g.setColour(ui::dim().withAlpha(0.55f));
    g.drawText("+12", (int) rx + 2, (int) gainToY(12.f) - 6, 24, 12, juce::Justification::left);
    g.drawText("0", (int) rx + 2, (int) midY - 6, 24, 12, juce::Justification::left);
    g.drawText("-12", (int) rx + 2, (int) gainToY(-12.f) - 6, 24, 12, juce::Justification::left);

    const auto& mp = proc.getAuxManager().masterParams;
    if (! mp.eqOn)
    {
        g.setColour(ui::dim().withAlpha(0.35f));
        g.drawHorizontalLine((int) midY, rx, rx + rw);
        g.setFont(uiFont(12.f, true));
        g.setColour(ui::dim().withAlpha(0.45f));
        g.drawText("MASTER EQ BYPASSED", bounds, juce::Justification::centred);
        return;
    }

    // Compute curve using JUCE biquad response
    const double sr = 48000.0;
    auto c0 = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 80.f, 0.707f, std::pow(10.f, mp.eqLowGain / 20.f));
    auto c1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 450.f, 0.9f, std::pow(10.f, mp.eqLowMidGain / 20.f));
    auto c2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 2500.f, 0.9f, std::pow(10.f, mp.eqHiMidGain / 20.f));
    auto c3 = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 10000.f, 0.707f, std::pow(10.f, mp.eqHighGain / 20.f));

    juce::Path curve, fillPath;
    const int numSteps = 75;
    for (int i = 0; i <= numSteps; ++i)
    {
        const float t = (float) i / (float) numSteps;
        const double freq = 20.0 * std::pow(1000.0, (double) t);
        const double mag = c0->getMagnitudeForFrequency(freq, sr)
                         * c1->getMagnitudeForFrequency(freq, sr)
                         * c2->getMagnitudeForFrequency(freq, sr)
                         * c3->getMagnitudeForFrequency(freq, sr);
        const float gainDb = (float) (20.0 * std::log10(std::max(0.00001, mag)));
        const float px = rx + t * rw;
        const float py = juce::jlimit(ry + 1.f, ry + rh - 1.f, gainToY(gainDb));

        if (i == 0)
        {
            curve.startNewSubPath(px, py);
            fillPath.startNewSubPath(px, midY);
            fillPath.lineTo(px, py);
        }
        else
        {
            curve.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }
    fillPath.lineTo(rx + rw, midY);
    fillPath.closeSubPath();

    // Heat gradient fill
    g.setGradientFill(juce::ColourGradient(ui::accent().withAlpha(0.35f), rx, midY,
                                           ui::accentGlow().withAlpha(0.05f), rx, ry, false));
    g.fillPath(fillPath);

    // Glowing response curve
    g.setColour(ui::accentHot());
    g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

    // Interactive band nodes
    static const double bandFreqs[4] = { 80.0, 450.0, 2500.0, 10000.0 };
    const float bandGains[4] = { mp.eqLowGain, mp.eqLowMidGain, mp.eqHiMidGain, mp.eqHighGain };
    static const juce::Colour bandCols[4] = {
        juce::Colour(0xFF00B4D8), // Low: cyan
        juce::Colour(0xFFFFB703), // Lo-Mid: amber-gold
        juce::Colour(0xFFFF6B00), // Hi-Mid: molten orange
        juce::Colour(0xFFFF3366)  // High: hot pink
    };

    for (int b = 0; b < 4; ++b)
    {
        float nx = freqToX(bandFreqs[b]);
        float ny = gainToY(bandGains[b]);
        auto nodeR = juce::Rectangle<float>(nx - 4.5f, ny - 4.5f, 9.f, 9.f);

        g.setColour(bandCols[b].withAlpha(0.35f));
        g.fillEllipse(nodeR.expanded(3.f));
        g.setColour(bandCols[b]);
        g.fillEllipse(nodeR);
        g.setColour(juce::Colours::white);
        g.fillEllipse(nodeR.reduced(2.f));
    }
}

void MasterEQGraphView::mouseDown(const juce::MouseEvent& e)
{
    const auto& mp = proc.getAuxManager().masterParams;
    if (! mp.eqOn) return;

    auto bounds = getLocalBounds().toFloat().reduced(2.f);
    const float rx = bounds.getX() + 6.f;
    const float rw = bounds.getWidth() - 12.f;
    const float rh = bounds.getHeight() - 12.f;
    const float midY = bounds.getY() + 6.f + rh * 0.5f;

    auto freqToX = [&](double f) { return rx + (float) ((std::log10(f / 20.0) / 3.0) * rw); };
    auto gainToY = [&](float gDb) { return midY - (gDb / 15.f) * (rh * 0.44f); };

    static const double bandFreqs[4] = { 80.0, 450.0, 2500.0, 10000.0 };
    const float bandGains[4] = { mp.eqLowGain, mp.eqLowMidGain, mp.eqHiMidGain, mp.eqHighGain };

    activeBand = -1;
    float bestDist = 20.f;
    for (int b = 0; b < 4; ++b)
    {
        float nx = freqToX(bandFreqs[b]);
        float ny = gainToY(bandGains[b]);
        float d = std::hypot(e.position.x - nx, e.position.y - ny);
        if (d < bestDist)
        {
            bestDist = d;
            activeBand = b;
        }
    }
}

void MasterEQGraphView::mouseDrag(const juce::MouseEvent& e)
{
    if (activeBand < 0) return;
    auto bounds = getLocalBounds().toFloat().reduced(2.f);
    const float rh = bounds.getHeight() - 12.f;
    const float midY = bounds.getY() + 6.f + rh * 0.5f;

    float newGain = - (e.position.y - midY) / (rh * 0.44f) * 15.f;
    newGain = juce::jlimit(-12.f, 12.f, newGain);

    auto& mp = proc.getAuxManager().masterParams;
    if (activeBand == 0) mp.eqLowGain = newGain;
    else if (activeBand == 1) mp.eqLowMidGain = newGain;
    else if (activeBand == 2) mp.eqHiMidGain = newGain;
    else if (activeBand == 3) mp.eqHighGain = newGain;

    if (onBandGainChanged)
        onBandGainChanged(activeBand, newGain);

    repaint();
}

void MasterEQGraphView::mouseUp(const juce::MouseEvent&)
{
    activeBand = -1;
}

// ---------------------------------------------------------------------------
// Hardware-Style VCA Gain Reduction (GR) Meter
// ---------------------------------------------------------------------------
CompGRMeter::CompGRMeter(Forge64Processor& processor)
    : proc(processor)
{
}

void CompGRMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(juce::Colour(0xFF100C0B));
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(ui::line().withAlpha(0.6f));
    g.drawRoundedRectangle(r, 4.f, 1.0f);

    const auto& mp = proc.getAuxManager().masterParams;
    if (! mp.compOn)
    {
        g.setColour(ui::dim().withAlpha(0.45f));
        g.setFont(uiFont(10.f, true));
        g.drawText("VCA COMPRESSOR BYPASSED", r, juce::Justification::centred);
        return;
    }

    const float grDb = juce::jlimit(0.f, 16.f, proc.getAuxManager().getMasterCompGR());
    const float meterLeft = r.getX() + 30.f;
    const float meterRight = r.getRight() - 54.f;
    const float mw = meterRight - meterLeft;
    const float barH = r.getHeight() - 12.f;
    const float barY = r.getY() + 3.f;

    // Scale ticks and labels: -16, -12, -8, -4, -2, 0
    static const float ticksDb[] = { 16.f, 12.f, 8.f, 4.f, 2.f, 0.f };
    g.setFont(uiFont(8.f));
    for (float db : ticksDb)
    {
        float tx = meterRight - (db / 16.f) * mw;
        g.setColour(ui::line());
        g.drawVerticalLine((int) tx, barY + barH - 2.f, barY + barH + 4.f);
        g.setColour(ui::dim().withAlpha(0.7f));
        g.drawText(juce::String((int) db), (int) tx - 10, (int) (r.getBottom() - 10.f), 20, 9, juce::Justification::centred);
    }

    // Meter slot background
    auto slotR = juce::Rectangle<float>(meterLeft, barY, mw, barH - 3.f);
    g.setColour(juce::Colour(0xFF1A1312));
    g.fillRoundedRectangle(slotR, 2.f);

    // Active GR Bar (moves from right 0dB to left)
    if (grDb > 0.05f)
    {
        const float fillW = (grDb / 16.f) * mw;
        auto fillR = juce::Rectangle<float>(meterRight - fillW, barY, fillW, barH - 3.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFF9900), meterRight, barY,
                                               juce::Colour(0xFFFF3300), meterRight - fillW, barY, false));
        g.fillRoundedRectangle(fillR, 2.f);
    }

    // Label on left: GR
    g.setFont(uiFont(10.f, true));
    g.setColour(ui::accent());
    g.drawText("GR", (int) r.getX() + 4, (int) barY, 22, (int) barH - 3, juce::Justification::centred);

    // Numeric readout on right
    auto numR = juce::Rectangle<float>(meterRight + 4.f, barY, 48.f, barH - 3.f);
    g.setFont(uiFont(10.f, true));
    g.setColour(grDb > 0.1f ? ui::accentHot() : ui::dim().withAlpha(0.6f));
    g.drawText(juce::String(-grDb, 1) + " dB", numR, juce::Justification::centredRight);
}

// ---------------------------------------------------------------------------
// Stereo L/R Output Peak Meter with Clip Detection
// ---------------------------------------------------------------------------
MasterPeakMeter::MasterPeakMeter(Forge64Processor& processor)
    : proc(processor)
{
}

void MasterPeakMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(juce::Colour(0xFF100C0B));
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(ui::line().withAlpha(0.6f));
    g.drawRoundedRectangle(r, 4.f, 1.0f);

    const float pkL = proc.getMasterPeakL();
    const float pkR = proc.getMasterPeakR();

    if (pkL >= 1.0f) clipHoldL = 30; else if (clipHoldL > 0) --clipHoldL;
    if (pkR >= 1.0f) clipHoldR = 30; else if (clipHoldR > 0) --clipHoldR;

    // Header Clip LEDs
    const float barW = (r.getWidth() - 14.f) * 0.5f;
    auto clipL = juce::Rectangle<float>(r.getX() + 4.f, r.getY() + 4.f, barW, 6.f);
    auto clipR = juce::Rectangle<float>(r.getX() + 10.f + barW, r.getY() + 4.f, barW, 6.f);

    g.setColour(clipHoldL > 0 ? juce::Colour(0xFFFF2200) : juce::Colour(0xFF3A1212));
    g.fillRoundedRectangle(clipL, 1.5f);
    g.setColour(clipHoldR > 0 ? juce::Colour(0xFFFF2200) : juce::Colour(0xFF3A1212));
    g.fillRoundedRectangle(clipR, 1.5f);

    // Channel meter bars
    const float meterY = r.getY() + 14.f;
    const float meterH = r.getHeight() - 36.f;

    auto slotL = juce::Rectangle<float>(r.getX() + 4.f, meterY, barW, meterH);
    auto slotR = juce::Rectangle<float>(r.getX() + 10.f + barW, meterY, barW, meterH);
    g.setColour(juce::Colour(0xFF1A1312));
    g.fillRoundedRectangle(slotL, 2.f);
    g.fillRoundedRectangle(slotR, 2.f);

    auto dbToFrac = [](float val) -> float
    {
        if (val <= 0.0039f) return 0.f; // below -48 dB
        float db = 20.f * std::log10(val);
        return juce::jlimit(0.f, 1.f, (db + 48.f) / 51.f);
    };

    const float fracL = dbToFrac(pkL);
    const float fracR = dbToFrac(pkR);

    // Segmented LED rendering
    const int numSegments = 24;
    const float segH = (meterH - (numSegments - 1) * 1.5f) / (float) numSegments;

    for (int s = 0; s < numSegments; ++s)
    {
        const float segFrac = (float) (s + 1) / (float) numSegments;
        const float sy = meterY + meterH - (s + 1) * (segH + 1.5f);

        juce::Colour segCol;
        if (segFrac > 0.94f)      segCol = juce::Colour(0xFFFF3300); // Clip red (>0dB)
        else if (segFrac > 0.85f) segCol = juce::Colour(0xFFFF7300); // Hot orange (-2dB..0dB)
        else if (segFrac > 0.65f) segCol = juce::Colour(0xFFFFD166); // Amber (-12dB..-2dB)
        else                      segCol = juce::Colour(0xFF00B4D8); // Cyan/Ice (<-12dB)

        if (fracL >= segFrac)
        {
            g.setColour(segCol);
            g.fillRoundedRectangle(slotL.getX(), sy, barW, segH, 1.f);
        }
        if (fracR >= segFrac)
        {
            g.setColour(segCol);
            g.fillRoundedRectangle(slotR.getX(), sy, barW, segH, 1.f);
        }
    }

    // Numeric readout at bottom
    float maxPk = juce::jmax(pkL, pkR);
    float maxDb = maxPk > 0.0001f ? 20.f * std::log10(maxPk) : -60.f;
    g.setFont(uiFont(9.f, true));
    g.setColour(maxDb > 0.f ? juce::Colour(0xFFFF3300) : (maxDb > -12.f ? ui::accentHot() : ui::dim()));
    g.drawText(maxDb < -48.f ? "-inf" : juce::String(maxDb, 1),
               (int) r.getX(), (int) (r.getBottom() - 18.f), (int) r.getWidth(), 16, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Content Component
// ---------------------------------------------------------------------------
class MixerFXPage::Content : public juce::Component
{
public:
    explicit Content(MixerFXPage& o) : owner(o) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(ui::bg());

        // Aux chassis card
        auto auxArea = juce::Rectangle<float>(8.f, 8.f, (float) getWidth() - 16.f, 310.f);
        ui::drawForgedPlate(g, auxArea, 6.f, false);

        // Master Bus chassis card
        auto masterArea = juce::Rectangle<float>(8.f, 322.f, (float) getWidth() - 16.f, (float) getHeight() - 330.f);
        ui::drawForgedPlate(g, masterArea, 6.f, false);
    }
    MixerFXPage& owner;
};

// ---------------------------------------------------------------------------
// Aux Channel Strip
// ---------------------------------------------------------------------------
MixerFXPage::AuxStrip::AuxStrip(MixerFXPage& owner, int auxIndex)
    : page(owner), idx(auxIndex)
{
    const auto col = getAuxThemeColour(idx);

    titleLabel = ui::makeLabel("AUX " + juce::String(idx + 1), 13.f, col);
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
        updateLabels(type);
    };
    addAndMakeVisible(fxCombo.get());

    auto makeAuxKnob = [&](std::unique_ptr<ModRingKnob>& knob, const char* name, float def,
                           float minV, float maxV, std::function<void(float)> onChange)
    {
        knob = std::make_unique<ModRingKnob>("aux" + juce::String(idx) + "_" + juce::String(name), name, page.svcs);
        knob->setRange(minV, maxV, 0.01);
        knob->setValue(def, juce::dontSendNotification);
        knob->onValueChange = [kPtr = knob.get(), onChange] { onChange((float) kPtr->getValue()); };
        addAndMakeVisible(knob.get());
    };

    auto& p = page.proc.getAuxManager().auxParams[idx];
    makeAuxKnob(p1Knob, "P1", p.p1, 0.f, 1.f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p1 = v;
        if (p1Knob) p1Knob->repaint();
    });
    makeAuxKnob(p2Knob, "P2", p.p2, 0.f, 1.f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p2 = v;
        if (p2Knob) p2Knob->repaint();
    });
    makeAuxKnob(p3Knob, "P3", p.p3, 0.f, 1.f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p3 = v;
        if (p3Knob) p3Knob->repaint();
    });
    makeAuxKnob(p4Knob, "P4", p.p4, 0.f, 1.f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].p4 = v;
        const int type = page.proc.getAuxManager().auxParams[idx].fxType;
        if (type == AUX_FX_DELAY || type == AUX_FX_PINGPONG)
        {
            const bool sync = (v >= 0.5f);
            if (p1Knob)
            {
                p1Knob->setLabel(sync ? "DIVISION" : "TIME");
                p1Knob->repaint();
            }
        }
        if (p4Knob) p4Knob->repaint();
    });

    makeAuxKnob(returnKnob, "RETURN", p.returnLevel, 0.f, 1.5f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].returnLevel = v;
        if (returnKnob) returnKnob->repaint();
    });
    makeAuxKnob(panKnob, "PAN", p.returnPan, -1.f, 1.f, [this](float v)
    {
        page.proc.getAuxManager().auxParams[idx].returnPan = v;
        if (panKnob) panKnob->repaint();
    });

    p1Knob->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 0, (float) v, ap.p4);
    };
    p2Knob->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 1, (float) v, ap.p4);
    };
    p3Knob->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 2, (float) v, ap.p4);
    };
    p4Knob->textFromValueFunction = [this](double v)
    {
        auto& ap = page.proc.getAuxManager().auxParams[idx];
        return formatAuxParam(ap.fxType, 3, (float) v, ap.p4);
    };
    returnKnob->textFromValueFunction = [](double v)
    {
        return formatAuxReturnLevel((float) v);
    };
    panKnob->textFromValueFunction = [](double v)
    {
        return formatAuxReturnPan((float) v);
    };

    updateLabels(p.fxType);
}

void MixerFXPage::AuxStrip::updateLabels(int fxType)
{
    auto& p = page.proc.getAuxManager().auxParams[idx];
    if (fxType == AUX_FX_DELAY || fxType == AUX_FX_PINGPONG)
        p4Knob->setRange(0.0, 1.0, 1.0);
    else
        p4Knob->setRange(0.0, 1.0, 0.01);

    switch (fxType)
    {
        case AUX_FX_REVERB:
            if (p1Knob) p1Knob->setLabel("SIZE");
            if (p2Knob) p2Knob->setLabel("DAMP");
            if (p3Knob) p3Knob->setLabel("PRE-DLY");
            if (p4Knob) p4Knob->setLabel("WIDTH");
            break;
        case AUX_FX_DELAY:
            if (p1Knob) p1Knob->setLabel(p.p4 >= 0.5f ? "DIVISION" : "TIME");
            if (p2Knob) p2Knob->setLabel("FEEDBACK");
            if (p3Knob) p3Knob->setLabel("HI-DAMP");
            if (p4Knob) p4Knob->setLabel("SYNC");
            break;
        case AUX_FX_DRIVE:
            if (p1Knob) p1Knob->setLabel("DRIVE");
            if (p2Knob) p2Knob->setLabel("TONE");
            if (p3Knob) p3Knob->setLabel("BIAS");
            if (p4Knob) p4Knob->setLabel("MIX");
            break;
        case AUX_FX_CHORUS:
        case AUX_FX_FLANGER:
        case AUX_FX_PHASER:
            if (p1Knob) p1Knob->setLabel("RATE");
            if (p2Knob) p2Knob->setLabel("DEPTH");
            if (p3Knob) p3Knob->setLabel("FEEDBACK");
            if (p4Knob) p4Knob->setLabel("MIX");
            break;
        case AUX_FX_COMP:
            if (p1Knob) p1Knob->setLabel("THRESH");
            if (p2Knob) p2Knob->setLabel("RATIO");
            if (p3Knob) p3Knob->setLabel("ATTACK");
            if (p4Knob) p4Knob->setLabel("RELEASE");
            break;
        case AUX_FX_FILTER:
            if (p1Knob) p1Knob->setLabel("CUTOFF");
            if (p2Knob) p2Knob->setLabel("RESO");
            if (p3Knob) p3Knob->setLabel("MODE");
            if (p4Knob) p4Knob->setLabel("DRIVE");
            break;
        case AUX_FX_SHIMMER:
            if (p1Knob) p1Knob->setLabel("DECAY");
            if (p2Knob) p2Knob->setLabel("SHIMMER");
            if (p3Knob) p3Knob->setLabel("TONE");
            if (p4Knob) p4Knob->setLabel("WIDTH");
            break;
        case AUX_FX_PINGPONG:
            if (p1Knob) p1Knob->setLabel(p.p4 >= 0.5f ? "DIVISION" : "TIME");
            if (p2Knob) p2Knob->setLabel("FEEDBACK");
            if (p3Knob) p3Knob->setLabel("HI-DAMP");
            if (p4Knob) p4Knob->setLabel("SYNC");
            break;
        case AUX_FX_GATED_VERB:
            if (p1Knob) p1Knob->setLabel("GATE TIME");
            if (p2Knob) p2Knob->setLabel("DENSITY");
            if (p3Knob) p3Knob->setLabel("TONE");
            if (p4Knob) p4Knob->setLabel("WIDTH");
            break;
        case AUX_FX_TUBE:
            if (p1Knob) p1Knob->setLabel("DRIVE");
            if (p2Knob) p2Knob->setLabel("BIAS");
            if (p3Knob) p3Knob->setLabel("WARMTH");
            if (p4Knob) p4Knob->setLabel("MIX");
            break;
        case AUX_FX_PITCH:
            if (p1Knob) p1Knob->setLabel("PITCH");
            if (p2Knob) p2Knob->setLabel("FINE");
            if (p3Knob) p3Knob->setLabel("FEEDBACK");
            if (p4Knob) p4Knob->setLabel("MIX");
            break;
        case AUX_FX_SPRING:
            if (p1Knob) p1Knob->setLabel("TENSION");
            if (p2Knob) p2Knob->setLabel("BOING");
            if (p3Knob) p3Knob->setLabel("TONE");
            if (p4Knob) p4Knob->setLabel("MIX");
            break;
        default:
            if (p1Knob) p1Knob->setLabel("PARAM 1");
            if (p2Knob) p2Knob->setLabel("PARAM 2");
            if (p3Knob) p3Knob->setLabel("PARAM 3");
            if (p4Knob) p4Knob->setLabel("PARAM 4");
            break;
    }

    if (p1Knob) p1Knob->repaint();
    if (p2Knob) p2Knob->repaint();
    if (p3Knob) p3Knob->repaint();
    if (p4Knob) p4Knob->repaint();
}

void MixerFXPage::AuxStrip::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(2.f);
    ui::drawForgedPlate(g, r, 4.f, false);

    const auto col = getAuxThemeColour(idx);

    // Top color strip
    g.setColour(col);
    g.fillRoundedRectangle(r.getX() + 2.f, r.getY() + 2.f, r.getWidth() - 4.f, 3.f, 1.5f);

    // Active status LED
    const bool on = enableBtn->getToggleState();
    auto ledR = juce::Rectangle<float>(r.getRight() - 20.f, r.getY() + 10.f, 8.f, 8.f);
    if (on)
    {
        g.setColour(col.withAlpha(0.85f));
        g.fillEllipse(ledR.expanded(2.f));
        g.setColour(juce::Colours::white);
        g.fillEllipse(ledR.reduced(1.f));
    }
    else
    {
        g.setColour(ui::panelHi().darker(0.3f));
        g.fillEllipse(ledR);
    }

    // Section line above Return / Pan
    const float divY = 216.f;
    g.setColour(ui::line().withAlpha(0.5f));
    g.drawHorizontalLine((int) divY, r.getX() + 8.f, r.getRight() - 8.f);
    g.setFont(uiFont(9.f));
    g.setColour(ui::dim().withAlpha(0.6f));
    g.drawText("RETURN BUS", (int) r.getX() + 10, (int) divY + 2, 70, 10, juce::Justification::left);

    // Live Return Level Meter Bar (on the right of pan knob)
    const float meterX = r.getRight() - 14.f;
    const float meterY = 228.f;
    const float meterW = 8.f;
    const float meterH = 68.f;

    auto slotR = juce::Rectangle<float>(meterX, meterY, meterW, meterH);
    g.setColour(juce::Colour(0xFF120E0D));
    g.fillRoundedRectangle(slotR, 2.f);
    g.setColour(ui::line().withAlpha(0.4f));
    g.drawRoundedRectangle(slotR, 2.f, 1.f);

    if (on)
    {
        const float rawVal = page.proc.getAuxManager().getAuxMeter(idx);
        const float lvl = std::sqrt(juce::jlimit(0.f, 1.5f, rawVal) / 1.2f);
        const float barH = juce::jlimit(0.f, meterH, lvl * meterH);
        if (barH > 1.f)
        {
            auto fillR = juce::Rectangle<float>(meterX, meterY + meterH - barH, meterW, barH);
            g.setGradientFill(juce::ColourGradient(col, meterX, meterY + meterH,
                                                   ui::accentHot(), meterX, meterY, false));
            g.fillRoundedRectangle(fillR, 1.5f);
        }
    }
}

void MixerFXPage::AuxStrip::resized()
{
    const int w = getWidth();
    titleLabel->setBounds(10, 8, 70, 20);
    enableBtn->setBounds(84, 8, 45, 20);
    fxCombo->setBounds(10, 32, w - 20, 24);

    const int kw = (w - 24) / 2;
    p1Knob->setBounds(8, 62, kw, 74);
    p2Knob->setBounds(12 + kw, 62, kw, 74);
    p3Knob->setBounds(8, 138, kw, 74);
    p4Knob->setBounds(12 + kw, 138, kw, 74);

    const int retW = (w - 38) / 2;
    returnKnob->setBounds(8, 226, retW, 72);
    panKnob->setBounds(12 + retW, 226, retW, 72);
}

// ---------------------------------------------------------------------------
// MixerFXPage Constructor & Methods
// ---------------------------------------------------------------------------
MixerFXPage::MixerFXPage(Forge64Processor& processor, ModRingKnob::Services& services)
    : proc(processor), svcs(services)
{
    content = std::make_unique<Content>(*this);
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(content.get(), false);
    addAndMakeVisible(viewport.get());

    // 4 Aux Channels
    for (int i = 0; i < 4; ++i)
    {
        auxStrips[(size_t) i] = std::make_unique<AuxStrip>(*this, i);
        content->addAndMakeVisible(auxStrips[(size_t) i].get());
    }

    // Master Header
    masterTitle = ui::makeLabel("MASTER BUS CONSOLE // VCA GLUE COMPRESSOR + 4-BAND HARMONIC EQ + TAPE DRIVE", 12.f, ui::accentHot());
    masterTitle->setFont(uiFont(12.f, true));
    content->addAndMakeVisible(masterTitle.get());

    // 1. Compressor Module
    compSectionTitle = ui::makeLabel("VCA BUS COMPRESSOR", 11.f, ui::accent());
    compSectionTitle->setFont(uiFont(11.f, true));
    content->addAndMakeVisible(compSectionTitle.get());

    masterCompEnable = std::make_unique<juce::ToggleButton>("COMP ON");
    ui::styleToggle(*masterCompEnable);
    masterCompEnable->setToggleState(proc.getAuxManager().masterParams.compOn, juce::dontSendNotification);
    masterCompEnable->onClick = [this]
    {
        proc.getAuxManager().masterParams.compOn = masterCompEnable->getToggleState();
        if (compGRMeter) compGRMeter->repaint();
    };
    content->addAndMakeVisible(masterCompEnable.get());

    compGRMeter = std::make_unique<CompGRMeter>(proc);
    content->addAndMakeVisible(compGRMeter.get());

    auto& mp = proc.getAuxManager().masterParams;
    auto makeMasterKnob = [&](std::unique_ptr<ModRingKnob>& knob, const char* id, const char* name,
                              float def, float minV, float maxV, std::function<void(float)> onChange)
    {
        knob = std::make_unique<ModRingKnob>(id, name, svcs);
        knob->setRange(minV, maxV, 0.01);
        knob->setValue(def, juce::dontSendNotification);
        knob->onValueChange = [kPtr = knob.get(), onChange] { onChange((float) kPtr->getValue()); };
        content->addAndMakeVisible(knob.get());
    };

    makeMasterKnob(compThreshKnob, "m_cthr", "THRESH", mp.compThresh, -40.f, 0.f,   [&mp](float v) { mp.compThresh = v; });
    makeMasterKnob(compRatioKnob,  "m_crat", "RATIO",  mp.compRatio,  1.f, 20.f,    [&mp](float v) { mp.compRatio = v; });
    makeMasterKnob(compGainKnob,   "m_cgan", "MAKEUP", mp.compMakeup, 0.f, 18.f,    [&mp](float v) { mp.compMakeup = v; });
    makeMasterKnob(compAtkKnob,    "m_catk", "ATTACK", mp.compAtk,    0.1f, 100.f,  [&mp](float v) { mp.compAtk = v; });
    makeMasterKnob(compRelKnob,    "m_crel", "RELEASE",mp.compRel,    10.f, 1000.f, [&mp](float v) { mp.compRel = v; });

    compThreshKnob->textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    compRatioKnob->textFromValueFunction  = [](double v) { return juce::String(v, 1) + ":1"; };
    compGainKnob->textFromValueFunction   = [](double v) { return (v > 0.05 ? "+" : "") + juce::String(v, 1) + " dB"; };
    compAtkKnob->textFromValueFunction    = [](double v) { return juce::String(v, 1) + " ms"; };
    compRelKnob->textFromValueFunction    = [](double v) { return juce::String((int) std::round(v)) + " ms"; };

    // 2. 4-Band Master EQ Module
    eqSectionTitle = ui::makeLabel("4-BAND MASTER HARMONIC EQ", 11.f, ui::accent());
    eqSectionTitle->setFont(uiFont(11.f, true));
    content->addAndMakeVisible(eqSectionTitle.get());

    masterEqEnable = std::make_unique<juce::ToggleButton>("EQ ON");
    ui::styleToggle(*masterEqEnable);
    masterEqEnable->setToggleState(mp.eqOn, juce::dontSendNotification);
    masterEqEnable->onClick = [this]
    {
        proc.getAuxManager().masterParams.eqOn = masterEqEnable->getToggleState();
        if (eqGraph) eqGraph->repaint();
    };
    content->addAndMakeVisible(masterEqEnable.get());

    eqGraph = std::make_unique<MasterEQGraphView>(proc);
    eqGraph->onBandGainChanged = [this](int band, float newGain)
    {
        if (band == 0 && eqLowGainKnob) eqLowGainKnob->setValue(newGain, juce::dontSendNotification);
        else if (band == 1 && eqLowMidGainKnob) eqLowMidGainKnob->setValue(newGain, juce::dontSendNotification);
        else if (band == 2 && eqHiMidGainKnob) eqHiMidGainKnob->setValue(newGain, juce::dontSendNotification);
        else if (band == 3 && eqHighGainKnob) eqHighGainKnob->setValue(newGain, juce::dontSendNotification);
    };
    content->addAndMakeVisible(eqGraph.get());

    makeMasterKnob(eqLowGainKnob,    "m_eqlg",  "LOW 80Hz",    mp.eqLowGain,    -12.f, 12.f,
                   [&mp, this](float v) { mp.eqLowGain = v; if (eqGraph) eqGraph->repaint(); });
    makeMasterKnob(eqLowMidGainKnob, "m_eqlmg", "LO-MID 450",  mp.eqLowMidGain, -12.f, 12.f,
                   [&mp, this](float v) { mp.eqLowMidGain = v; if (eqGraph) eqGraph->repaint(); });
    makeMasterKnob(eqHiMidGainKnob,  "m_eqhmg", "HI-MID 2.5k", mp.eqHiMidGain,  -12.f, 12.f,
                   [&mp, this](float v) { mp.eqHiMidGain = v; if (eqGraph) eqGraph->repaint(); });
    makeMasterKnob(eqHighGainKnob,   "m_eqhg",  "HIGH 10kHz",  mp.eqHighGain,   -12.f, 12.f,
                   [&mp, this](float v) { mp.eqHighGain = v; if (eqGraph) eqGraph->repaint(); });

    auto formatEqGain = [](double v) { return (v > 0.05 ? "+" : "") + juce::String(v, 1) + " dB"; };
    eqLowGainKnob->textFromValueFunction    = formatEqGain;
    eqLowMidGainKnob->textFromValueFunction = formatEqGain;
    eqHiMidGainKnob->textFromValueFunction  = formatEqGain;
    eqHighGainKnob->textFromValueFunction   = formatEqGain;

    // 3. Output & Saturation Module
    outSectionTitle = ui::makeLabel("OUTPUT & TAPE", 11.f, ui::accentHot());
    outSectionTitle->setFont(uiFont(11.f, true));
    content->addAndMakeVisible(outSectionTitle.get());

    makeMasterKnob(masterDriveKnob, "m_drv",  "TAPE DRIVE", mp.drive, 0.f, 1.f, [&mp](float v) { mp.drive = v; });
    makeMasterKnob(masterVolKnob,   "master", "MASTER OUT", 0.8f,     0.f, 1.25f, [this](float v)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.getAPVTS().getParameter("master")))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    });

    masterDriveKnob->textFromValueFunction = [](double v) { return juce::String((int) std::round(v * 100.0)) + " %"; };
    masterVolKnob->textFromValueFunction   = [](double v) { return juce::String((int) std::round(v * 100.0)) + " %"; };

    masterPeakMeter = std::make_unique<MasterPeakMeter>(proc);
    content->addAndMakeVisible(masterPeakMeter.get());

    startTimerHz(30);
}

MixerFXPage::~MixerFXPage()
{
    stopTimer();
}

void MixerFXPage::timerCallback()
{
    // Fast periodic repaint for dynamic meters and real-time response
    if (compGRMeter)
        compGRMeter->repaint();

    if (masterPeakMeter)
        masterPeakMeter->repaint();

    for (auto& strip : auxStrips)
        if (strip)
            strip->repaint();
}

void MixerFXPage::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
}

void MixerFXPage::resized()
{
    viewport->setBounds(getLocalBounds());
    const int w = juce::jmax(880, viewport->getMaximumVisibleWidth());
    const int h = juce::jmax(640, viewport->getMaximumVisibleHeight());
    content->setSize(w, h);

    // 1. Aux channels plate (Top)
    const int auxPlateW = w - 16;
    const int stripGap = 8;
    const int stripW = (auxPlateW - 16 - (stripGap * 3)) / 4;
    int sx = 16;
    for (int i = 0; i < 4; ++i)
    {
        auxStrips[(size_t) i]->setBounds(sx, 12, stripW, 302);
        sx += stripW + stripGap;
    }

    // 2. Master Console Modules (Bottom)
    masterTitle->setBounds(16, 324, w - 32, 18);

    const int availW = w - 32;
    const int compW = juce::jlimit(270, 310, (int) (availW * 0.33f));
    const int outW  = juce::jlimit(170, 200, (int) (availW * 0.20f));
    const int eqW   = availW - compW - outW - 16;

    int mx = 16;
    const int my = 344;

    // --- Compressor Block ---
    compSectionTitle->setBounds(mx + 8, my + 6, compW - 90, 18);
    masterCompEnable->setBounds(mx + compW - 80, my + 5, 74, 18);
    compGRMeter->setBounds(mx + 8, my + 26, compW - 16, 30);

    const int ckw3 = (compW - 24) / 3;
    compThreshKnob->setBounds(mx + 8, my + 64, ckw3, 76);
    compRatioKnob->setBounds(mx + 12 + ckw3, my + 64, ckw3, 76);
    compGainKnob->setBounds(mx + 16 + ckw3 * 2, my + 64, ckw3, 76);

    const int ckw2 = (compW - 32) / 2;
    compAtkKnob->setBounds(mx + 12, my + 148, ckw2, 76);
    compRelKnob->setBounds(mx + 20 + ckw2, my + 148, ckw2, 76);

    mx += compW + 8;

    // --- 4-Band EQ Block ---
    eqSectionTitle->setBounds(mx + 8, my + 6, eqW - 80, 18);
    masterEqEnable->setBounds(mx + eqW - 74, my + 5, 68, 18);
    eqGraph->setBounds(mx + 8, my + 26, eqW - 16, 114);

    const int eqkw = (eqW - 24) / 4;
    eqLowGainKnob->setBounds(mx + 8, my + 148, eqkw, 76);
    eqLowMidGainKnob->setBounds(mx + 12 + eqkw, my + 148, eqkw, 76);
    eqHiMidGainKnob->setBounds(mx + 16 + eqkw * 2, my + 148, eqkw, 76);
    eqHighGainKnob->setBounds(mx + 20 + eqkw * 3, my + 148, eqkw, 76);

    mx += eqW + 8;

    // --- Output & Saturation Block ---
    outSectionTitle->setBounds(mx + 8, my + 6, outW - 16, 18);
    masterDriveKnob->setBounds(mx + 8, my + 34, 76, 76);
    masterVolKnob->setBounds(mx + 6, my + 124, 80, 96);
    masterPeakMeter->setBounds(mx + outW - 48, my + 24, 40, 200);
}

} // namespace f64
