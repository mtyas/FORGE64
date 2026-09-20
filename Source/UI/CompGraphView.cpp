#include "CompGraphView.h"
#include "UICommon.h"
#include "../Engine/PadDefs.h"
#include <cmath>

namespace f64 {

CompGraphView::CompGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex)
    : vts(apvts), pad(padIndex)
{
    updateFromParams();
    startTimerHz(30);
}

void CompGraphView::timerCallback()
{
    float curThr = thresh;
    float curRat = ratio;
    if (auto* p = vts.getRawParameterValue(padParamId(pad, "cthr")))
        curThr = p->load();
    if (auto* p = vts.getRawParameterValue(padParamId(pad, "crat")))
        curRat = p->load();

    if (curThr != thresh || curRat != ratio)
    {
        thresh = curThr;
        ratio = curRat;
        repaint();
    }
}

void CompGraphView::updateFromParams()
{
    if (auto* p = vts.getRawParameterValue(padParamId(pad, "cthr")))
        thresh = p->load();
    if (auto* p = vts.getRawParameterValue(padParamId(pad, "crat")))
        ratio = p->load();
    repaint();
}

float CompGraphView::dbToX(float db) const
{
    const float w = (float) getWidth() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, (db - (-60.f)) / 60.f);
    return 4.f + norm * w;
}

float CompGraphView::xToDb(float x) const
{
    const float w = (float) getWidth() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, (x - 4.f) / w);
    return -60.f + norm * 60.f;
}

float CompGraphView::dbToY(float db) const
{
    const float h = (float) getHeight() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, (db - (-60.f)) / 60.f);
    return 4.f + (1.f - norm) * h;
}

float CompGraphView::yToDb(float y) const
{
    const float h = (float) getHeight() - 8.f;
    const float norm = juce::jlimit(0.f, 1.f, 1.f - (y - 4.f) / h);
    return -60.f + norm * 60.f;
}

void CompGraphView::resized()
{
    repaint();
}

void CompGraphView::paint(juce::Graphics& g)
{
    auto rc = getLocalBounds().toFloat();

    // Chassis frame: smelting crucible interior
    g.setColour(juce::Colour(0xFF0C0807));
    g.fillRoundedRectangle(rc, 4.f);
    g.setColour(ui::line().withAlpha(0.55f));
    g.drawRoundedRectangle(rc, 4.f, 1.f);

    const float x0 = dbToX(-60.f), y0 = dbToY(-60.f);
    const float xMax = dbToX(0.f), yMax = dbToY(0.f);

    // 1:1 Unity gain reference dashed line
    g.setColour(ui::line().withAlpha(0.4f));
    const float dashes[2] = { 4.f, 4.f };
    g.drawDashedLine(juce::Line<float>(x0, y0, xMax, yMax), dashes, 2, 1.f);

    // Threshold vertical line (heat reference line)
    const float tx = dbToX(thresh);
    const float ty = dbToY(thresh);
    g.setColour(ui::accentGlow().withAlpha(0.32f));
    g.drawDashedLine(juce::Line<float>(tx, 4.f, tx, (float) getHeight() - 4.f), dashes, 2, 1.f);

    // Dynamic curve
    juce::Path curve, wedge;
    curve.startNewSubPath(x0, y0);
    curve.lineTo(tx, ty);

    // Above threshold: output = threshold + (input - threshold) / ratio
    const float outAtMax = thresh + (0.f - thresh) / std::max(1.f, ratio);
    const float yCompMax = dbToY(outAtMax);
    curve.lineTo(xMax, yCompMax);

    // Gain reduction wedge between unity line and compressed curve (furnace heat)
    if (ratio > 1.05f && thresh < -1.f)
    {
        wedge.startNewSubPath(tx, ty);
        wedge.lineTo(xMax, yMax);
        wedge.lineTo(xMax, yCompMax);
        wedge.closeSubPath();
        g.setGradientFill(juce::ColourGradient(ui::ember().withAlpha(0.26f), tx, ty,
                                               ui::accentGlow().withAlpha(0.08f), xMax, yMax, false));
        g.fillPath(wedge);
    }

    // Radiant heat bloom + Molten curve line
    g.setColour(ui::accentGlow().withAlpha(0.35f));
    g.strokePath(curve, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::accent());
    g.strokePath(curve, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Threshold point node (glowing molten furnace spark)
    g.setColour(ui::accentGlow().withAlpha(0.45f));
    g.fillEllipse(tx - 8.f, ty - 8.f, 16.f, 16.f);
    g.setColour(ui::accentHot());
    g.fillEllipse(tx - 4.f, ty - 4.f, 8.f, 8.f);
    g.setColour(juce::Colour(0xFF160E0C));
    g.drawEllipse(tx - 4.f, ty - 4.f, 8.f, 8.f, 1.f);

    // Readout caption
    g.setFont(uiFont(9.f, true));
    g.setColour(ui::accentHot().withAlpha(0.85f));
    g.drawText("RATIO 1:" + juce::String(ratio, 1), rc.reduced(8.f), juce::Justification::topRight);
}

void CompGraphView::mouseDown(const juce::MouseEvent& e)
{
    const float tx = dbToX(thresh);
    const float ty = dbToY(thresh);
    if (std::hypot(e.x - tx, e.y - ty) < 16.f || std::abs(e.x - tx) < 10.f)
        draggingThresh = true;
}

void CompGraphView::mouseDrag(const juce::MouseEvent& e)
{
    if (! draggingThresh)
        return;

    thresh = juce::jlimit(-60.f, 0.f, xToDb((float) e.x));
    if (auto* p = vts.getParameter(padParamId(pad, "cthr")))
        p->setValueNotifyingHost(p->convertTo0to1(thresh));

    if (onParamsChanged)
        onParamsChanged();

    repaint();
}

void CompGraphView::mouseUp(const juce::MouseEvent&)
{
    draggingThresh = false;
}

} // namespace f64
