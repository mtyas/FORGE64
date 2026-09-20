#include "VCFGraphView.h"
#include "UICommon.h"
#include <cmath>

namespace f64 {

VCFGraphView::VCFGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex)
    : state(apvts), pad(padIndex)
{
    startTimerHz(30);
}

VCFGraphView::~VCFGraphView()
{
    stopTimer();
}

void VCFGraphView::resized()
{
    plotArea = getLocalBounds().reduced(4).toFloat();
}

float VCFGraphView::freqToX(float freq) const
{
    const float minF = 20.f;
    const float maxF = 20000.f;
    const float norm = (std::log10(juce::jlimit(minF, maxF, freq)) - std::log10(minF))
                     / (std::log10(maxF) - std::log10(minF));
    return plotArea.getX() + norm * plotArea.getWidth();
}

float VCFGraphView::xToFreq(float x) const
{
    if (plotArea.getWidth() <= 0.f) return 1000.f;
    const float minF = 20.f;
    const float maxF = 20000.f;
    const float norm = juce::jlimit(0.f, 1.f, (x - plotArea.getX()) / plotArea.getWidth());
    return std::pow(10.f, std::log10(minF) + norm * (std::log10(maxF) - std::log10(minF)));
}

float VCFGraphView::dbToY(float db) const
{
    const float minDb = -30.f;
    const float maxDb = 18.f;
    const float norm = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return plotArea.getBottom() - norm * plotArea.getHeight();
}

float VCFGraphView::yToDb(float y) const
{
    if (plotArea.getHeight() <= 0.f) return 0.f;
    const float minDb = -30.f;
    const float maxDb = 18.f;
    const float norm = juce::jlimit(0.f, 1.f, (plotArea.getBottom() - y) / plotArea.getHeight());
    return minDb + norm * (maxDb - minDb);
}

void VCFGraphView::timerCallback()
{
    auto getP = [&](const char* base, float def) -> float
    {
        if (auto* p = state.getRawParameterValue(padParamId(pad, base)))
            return p->load();
        return def;
    };

    const int curType = (int) getP("vcft", 0.f);
    const float curCut = getP("vcfc", 20000.f);
    const float curRes = getP("vcfr", 0.707f);
    const float curEnv = getP("vcfe", 0.f);

    if (curType != filterType || std::abs(curCut - cutoff) > 0.5f ||
        std::abs(curRes - resonance) > 0.01f || std::abs(curEnv - envAmt) > 0.01f)
    {
        filterType = curType;
        cutoff = curCut;
        resonance = curRes;
        envAmt = curEnv;
        repaint();
    }
}

void VCFGraphView::paint(juce::Graphics& g)
{
    ui::drawForgedPlate(g, getLocalBounds().toFloat(), 4.f, false);

    if (plotArea.isEmpty()) return;

    // Dark well interior
    g.setColour(juce::Colour(0xFF0C0806));
    g.fillRoundedRectangle(plotArea, 3.f);
    g.setColour(ui::line().withAlpha(0.6f));
    g.drawRoundedRectangle(plotArea, 3.f, 1.f);

    // Grid: Frequencies (100 Hz, 1 kHz, 10 kHz)
    g.setColour(juce::Colour(0xFFFF6600).withAlpha(0.08f));
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        float gx = freqToX(f);
        g.drawVerticalLine((int) gx, plotArea.getY(), plotArea.getBottom());
    }

    // Grid: 0 dB line
    float y0 = dbToY(0.f);
    g.setColour(ui::accent().withAlpha(0.2f));
    g.drawHorizontalLine((int) y0, plotArea.getX(), plotArea.getRight());

    // Compute curve path
    juce::Path curve;
    const int numPoints = (int) plotArea.getWidth();
    if (numPoints <= 0) return;

    const float Q = juce::jmax(0.1f, resonance);
    const float invQ = 1.0f / Q;

    for (int i = 0; i < numPoints; ++i)
    {
        float x = plotArea.getX() + (float) i;
        float f = xToFreq(x);
        float r = f / juce::jmax(10.f, cutoff);
        float r2 = r * r;

        // 2nd-order state-variable filter magnitude
        float denom = std::sqrt((1.f - r2) * (1.f - r2) + r2 * (invQ * invQ)) + 1e-6f;
        float mag = 1.0f;

        switch (filterType)
        {
            case 1: // Lowpass
                mag = 1.0f / denom;
                break;
            case 2: // Highpass
                mag = r2 / denom;
                break;
            case 3: // Bandpass
                mag = (r * invQ) / denom;
                break;
            case 4: // Notch
                mag = std::abs(1.f - r2) / denom;
                break;
            default: // Bypass (0)
                mag = 1.0f;
                break;
        }

        float db = 20.f * std::log10(juce::jmax(0.001f, mag));
        float y = dbToY(db);

        if (i == 0) curve.startNewSubPath(x, y);
        else        curve.lineTo(x, y);
    }

    // Fill under curve
    if (filterType != 0)
    {
        juce::Path fillPath = curve;
        fillPath.lineTo(plotArea.getRight(), plotArea.getBottom());
        fillPath.lineTo(plotArea.getX(), plotArea.getBottom());
        fillPath.closeSubPath();

        juce::ColourGradient grad(ui::accentHot().withAlpha(0.24f), 0.f, plotArea.getY(),
                                  juce::Colour(0x00000000), 0.f, plotArea.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(fillPath);
    }

    // Curve line
    g.setColour(filterType == 0 ? ui::dim() : ui::accentHot());
    g.strokePath(curve, juce::PathStrokeType(filterType == 0 ? 1.0f : 1.8f));

    // Handle puck at cutoff point
    if (filterType != 0)
    {
        float hx = freqToX(cutoff);
        float hy = dbToY(filterType == 3 ? 0.f : (filterType == 4 ? -24.f : 20.f * std::log10(juce::jmax(0.01f, Q))));

        // Glow
        juce::ColourGradient hg(ui::accentHot().withAlpha(isDragging ? 0.6f : 0.3f), hx, hy,
                                ui::accentHot().withAlpha(0.f), hx, hy, true);
        hg.addColour(1.0, juce::Colour(0x00000000));
        g.setGradientFill(hg);
        g.fillEllipse(hx - 10.f, hy - 10.f, 20.f, 20.f);

        // Puck
        g.setColour(ui::panelHi());
        g.fillEllipse(hx - 5.f, hy - 5.f, 10.f, 10.f);
        g.setColour(ui::accentHot());
        g.drawEllipse(hx - 5.f, hy - 5.f, 10.f, 10.f, 1.5f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(hx - 1.5f, hy - 1.5f, 3.f, 3.f);
    }

    // Status / Mode readout in corner
    g.setFont(uiFont(9.f, true));
    g.setColour(ui::accent());
    juce::String modeStr;
    switch (filterType)
    {
        case 1: modeStr = "LP 12dB"; break;
        case 2: modeStr = "HP 12dB"; break;
        case 3: modeStr = "BP 12dB"; break;
        case 4: modeStr = "NOTCH"; break;
        default: modeStr = "BYPASS"; break;
    }

    juce::String cutStr = cutoff >= 1000.f ? juce::String(cutoff / 1000.f, 1) + " kHz"
                                          : juce::String((int) cutoff) + " Hz";
    juce::String info = modeStr + "  " + cutStr + "  Q:" + juce::String(resonance, 2);
    g.drawText(info, plotArea.reduced(6, 4), juce::Justification::topRight, false);
}

void VCFGraphView::mouseDown(const juce::MouseEvent& e)
{
    if (filterType == 0) return;
    isDragging = true;
    updateFromMouse(e);
}

void VCFGraphView::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging && filterType != 0)
        updateFromMouse(e);
}

void VCFGraphView::mouseUp(const juce::MouseEvent& /*e*/)
{
    isDragging = false;
}

void VCFGraphView::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    if (auto* cp = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(padParamId(pad, "vcfc"))))
        cp->setValueNotifyingHost(cp->convertTo0to1(20000.f));
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(padParamId(pad, "vcfr"))))
        rp->setValueNotifyingHost(rp->convertTo0to1(0.707f));
    if (onParamsChanged) onParamsChanged();
    repaint();
}

void VCFGraphView::updateFromMouse(const juce::MouseEvent& e)
{
    float newF = xToFreq((float) e.x);
    // Y maps to resonance (0.1 .. 10.0)
    float normY = juce::jlimit(0.f, 1.f, (plotArea.getBottom() - (float) e.y) / plotArea.getHeight());
    float newQ = 0.1f + std::pow(normY, 1.8f) * 9.9f;

    if (auto* cp = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(padParamId(pad, "vcfc"))))
        cp->setValueNotifyingHost(cp->convertTo0to1(newF));
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(padParamId(pad, "vcfr"))))
        rp->setValueNotifyingHost(rp->convertTo0to1(newQ));

    cutoff = newF;
    resonance = newQ;

    if (onParamsChanged)
        onParamsChanged();

    repaint();
}

} // namespace f64
