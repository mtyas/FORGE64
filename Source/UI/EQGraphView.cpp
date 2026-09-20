#include "EQGraphView.h"
#include "UICommon.h"
#include "../Engine/PadDefs.h"
#include <cmath>

namespace f64 {

EQGraphView::EQGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex)
    : vts(apvts), pad(padIndex)
{
    updateFromParams();
    startTimerHz(30);
}

void EQGraphView::timerCallback()
{
    auto getP = [&](const char* base, float def) -> float
    {
        if (auto* p = vts.getRawParameterValue(padParamId(pad, base)))
            return p->load();
        return def;
    };

    const float curLf = getP("eqlf", 200.f);
    const float curLg = getP("eqlg", 0.f);
    const float curMf = getP("eqmf", 1000.f);
    const float curMg = getP("eqmg", 0.f);
    const float curHf = getP("eqhf", 8000.f);
    const float curHg = getP("eqhg", 0.f);

    if (curLf != lf || curLg != lg || curMf != mf || curMg != mg || curHf != hf || curHg != hg)
    {
        lf = curLf; lg = curLg;
        mf = curMf; mg = curMg;
        hf = curHf; hg = curHg;
        repaint();
    }
}

void EQGraphView::updateFromParams()
{
    auto getP = [&](const char* base, float def) -> float
    {
        if (auto* p = vts.getRawParameterValue(padParamId(pad, base)))
            return p->load();
        return def;
    };

    lf = getP("eqlf", 200.f);
    lg = getP("eqlg", 0.f);
    mf = getP("eqmf", 1000.f);
    mg = getP("eqmg", 0.f);
    hf = getP("eqhf", 8000.f);
    hg = getP("eqhg", 0.f);

    repaint();
}

float EQGraphView::freqToX(float f) const
{
    const float w = (float) getWidth() - 8.f;
    const float minLog = std::log10(20.f);
    const float maxLog = std::log10(20000.f);
    const float curLog = std::log10(juce::jlimit(20.f, 20000.f, f));
    return 4.f + ((curLog - minLog) / (maxLog - minLog)) * w;
}

float EQGraphView::xToFreq(float x) const
{
    const float w = (float) getWidth() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, (x - 4.f) / w);
    const float minLog = std::log10(20.f);
    const float maxLog = std::log10(20000.f);
    return std::pow(10.f, minLog + norm * (maxLog - minLog));
}

float EQGraphView::gainToY(float gDb) const
{
    const float h = (float) getHeight() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, (gDb - (-18.f)) / 36.f);
    return 4.f + (1.f - norm) * h;
}

float EQGraphView::yToGain(float y) const
{
    const float h = (float) getHeight() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, 1.f - (y - 4.f) / h);
    return -18.f + norm * 36.f;
}

int EQGraphView::getNodeAt(float x, float y) const
{
    const float r = 12.f;
    auto dist = [&](float nx, float ny) { return std::hypot(x - nx, y - ny); };

    if (dist(freqToX(lf), gainToY(lg)) < r) return 0;
    if (dist(freqToX(mf), gainToY(mg)) < r) return 1;
    if (dist(freqToX(hf), gainToY(hg)) < r) return 2;
    return -1;
}

void EQGraphView::resized()
{
    repaint();
}

void EQGraphView::paint(juce::Graphics& g)
{
    auto rc = getLocalBounds().toFloat();

    // Chassis frame: smelting crucible interior
    g.setColour(juce::Colour(0xFF0C0807));
    g.fillRoundedRectangle(rc, 4.f);
    g.setColour(ui::line().withAlpha(0.55f));
    g.drawRoundedRectangle(rc, 4.f, 1.f);

    const float zeroY = gainToY(0.f);
    const float w = rc.getWidth();

    // Grid lines: 100Hz, 1kHz, 10kHz (smoked slag dividers)
    g.setColour(ui::line().withAlpha(0.3f));
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        const float x = freqToX(f);
        g.drawVerticalLine((int) x, 4.f, rc.getBottom() - 4.f);
    }
    // Zero dB line
    g.setColour(ui::line().withAlpha(0.45f));
    g.drawHorizontalLine((int) zeroY, 4.f, rc.getRight() - 4.f);

    // Compute frequency response curve
    const int numPts = getWidth() - 8;
    if (numPts <= 4)
        return;

    juce::Path curve, fill;
    const float qMid = 1.0f;

    auto calcGainAt = [&](float f) -> float
    {
        // Low shelf approximation
        const float flRatio = f / lf;
        const float gainL = lg / (1.f + flRatio * flRatio);

        // Mid bell approximation
        const float fmRatio = f / mf;
        const float midDiff = fmRatio - 1.f / fmRatio;
        const float gainM = mg / (1.f + qMid * qMid * midDiff * midDiff);

        // High shelf approximation
        const float fhRatio = hf / f;
        const float gainH = hg / (1.f + fhRatio * fhRatio);

        return gainL + gainM + gainH;
    };

    bool first = true;
    for (int x = 4; x < getWidth() - 4; ++x)
    {
        const float freq = xToFreq((float) x);
        const float totalGainDb = calcGainAt(freq);
        const float y = gainToY(totalGainDb);

        if (first)
        {
            curve.startNewSubPath((float) x, y);
            fill.startNewSubPath((float) x, zeroY);
            fill.lineTo((float) x, y);
            first = false;
        }
        else
        {
            curve.lineTo((float) x, y);
            fill.lineTo((float) x, y);
        }
    }
    fill.lineTo((float) (getWidth() - 4), zeroY);
    fill.closeSubPath();

    // Fill furnace heat gradient (molten gold to deep ember red)
    g.setGradientFill(juce::ColourGradient(ui::accent().withAlpha(0.28f), 0.f, 4.f,
                                           ui::ember().withAlpha(0.04f), 0.f, (float) getHeight(), false));
    g.fillPath(fill);

    // Curve line: radiant heat glow + molten wire
    g.setColour(ui::accentGlow().withAlpha(0.35f));
    g.strokePath(curve, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::accent());
    g.strokePath(curve, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Nodes: glowing molten metal nodes with hot aura
    auto drawNode = [&](float f, float gDb, juce::Colour col, const char* label, bool active)
    {
        const float nx = freqToX(f);
        const float ny = gainToY(gDb);
        const float sz = active ? 17.f : 13.f;

        // Radiant heat aura
        g.setColour(col.withAlpha(active ? 0.55f : 0.32f));
        g.fillEllipse(nx - sz * 0.5f - 3.f, ny - sz * 0.5f - 3.f, sz + 6.f, sz + 6.f);

        // Core incandescent node
        g.setColour(col);
        g.fillEllipse(nx - sz * 0.5f, ny - sz * 0.5f, sz, sz);
        g.setColour(ui::accentHot());
        g.drawEllipse(nx - sz * 0.5f, ny - sz * 0.5f, sz, sz, 1.f);

        g.setColour(juce::Colour(0xFF100B09));
        g.setFont(uiFont(8.f, true));
        g.drawText(label, nx - sz * 0.5f, ny - sz * 0.5f, sz, sz, juce::Justification::centred);
    };

    drawNode(lf, lg, juce::Colour(0xFFFF3E14), "1", activeNode == 0); // Low: Red-hot ember
    drawNode(mf, mg, juce::Colour(0xFFFF8800), "2", activeNode == 1); // Mid: Molten flame orange
    drawNode(hf, hg, juce::Colour(0xFFFFD166), "3", activeNode == 2); // Hi: White-hot crucible steel
}

void EQGraphView::mouseMove(const juce::MouseEvent& e)
{
    const int n = getNodeAt((float) e.x, (float) e.y);
    if (n >= 0)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void EQGraphView::mouseDown(const juce::MouseEvent& e)
{
    activeNode = getNodeAt((float) e.x, (float) e.y);
    repaint();
}

void EQGraphView::mouseDrag(const juce::MouseEvent& e)
{
    if (activeNode < 0)
        return;

    const float freq = xToFreq((float) e.x);
    const float gain = juce::jlimit(-18.f, 18.f, yToGain((float) e.y));

    auto setParam = [&](const char* base, float val)
    {
        if (auto* p = vts.getParameter(padParamId(pad, base)))
            p->setValueNotifyingHost(p->convertTo0to1(val));
    };

    if (activeNode == 0)
    {
        lf = juce::jlimit(20.f, 2000.f, freq);
        lg = gain;
        setParam("eqlf", lf);
        setParam("eqlg", lg);
    }
    else if (activeNode == 1)
    {
        mf = juce::jlimit(100.f, 8000.f, freq);
        mg = gain;
        setParam("eqmf", mf);
        setParam("eqmg", mg);
    }
    else if (activeNode == 2)
    {
        hf = juce::jlimit(2000.f, 16000.f, freq);
        hg = gain;
        setParam("eqhf", hf);
        setParam("eqhg", hg);
    }

    if (onParamsChanged)
        onParamsChanged();

    repaint();
}

void EQGraphView::mouseUp(const juce::MouseEvent&)
{
    activeNode = -1;
    repaint();
}

} // namespace f64
