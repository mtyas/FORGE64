#include "WaveformDisplay.h"
#include "UICommon.h"

namespace f64 {

WaveformDisplay::WaveformDisplay(PadGrid& grid, int padIndex)
    : padGrid(grid), pad(padIndex)
{
    loopBtn = std::make_unique<juce::ToggleButton>("LOOP");
    ui::styleToggle(*loopBtn);
    loopBtn->setTooltip("Enable seamless looping between loop markers");
    loopBtn->onClick = [this]
    {
        isLoop = loopBtn->getToggleState();
        padGrid.padState(pad).setProperty("loopOn", isLoop, nullptr);
        padGrid.runtime(pad).loopOn.store(isLoop);
        repaint();
    };
    addAndMakeVisible(loopBtn.get());

    revBtn = std::make_unique<juce::ToggleButton>("REV");
    ui::styleToggle(*revBtn);
    revBtn->setTooltip("Play sample in reverse");
    revBtn->onClick = [this]
    {
        isRev = revBtn->getToggleState();
        padGrid.padState(pad).setProperty("reverseOn", isRev, nullptr);
        padGrid.runtime(pad).reverseOn.store(isRev);
        repaint();
    };
    addAndMakeVisible(revBtn.get());

    zoomInBtn = std::make_unique<juce::TextButton>("+");
    ui::styleButton(*zoomInBtn);
    zoomInBtn->setTooltip("Zoom in waveform (or use mouse wheel)");
    zoomInBtn->onClick = [this] { setZoom(zoom * 1.5f); };
    addAndMakeVisible(zoomInBtn.get());

    zoomOutBtn = std::make_unique<juce::TextButton>("-");
    ui::styleButton(*zoomOutBtn);
    zoomOutBtn->setTooltip("Zoom out waveform");
    zoomOutBtn->onClick = [this] { setZoom(zoom / 1.5f); };
    addAndMakeVisible(zoomOutBtn.get());

    zoomResetBtn = std::make_unique<juce::TextButton>("1:1");
    ui::styleButton(*zoomResetBtn);
    zoomResetBtn->setTooltip("Reset zoom to full view");
    zoomResetBtn->onClick = [this] { setZoom(1.0f); };
    addAndMakeVisible(zoomResetBtn.get());

    zoomLabel = ui::makeLabel("1.0x", 9.5f, ui::dim());
    addAndMakeVisible(zoomLabel.get());

    infoLabel = ui::makeLabel("No sample loaded", 10.f, ui::dim());
    addAndMakeVisible(infoLabel.get());

    refresh();
}

void WaveformDisplay::refresh()
{
    setSample(padGrid.runtime(pad).sample);
}

float WaveformDisplay::fracToX(float frac, float left, float width) const
{
    return left + ((frac - scrollOffset) * zoom) * width;
}

float WaveformDisplay::xToFrac(float x, float left, float width) const
{
    if (width <= 0.f || zoom <= 0.f) return 0.f;
    return juce::jlimit(0.f, 1.f, scrollOffset + ((x - left) / width) / zoom);
}

void WaveformDisplay::setZoom(float newZoom, float centerFrac)
{
    zoom = juce::jlimit(1.0f, 32.0f, newZoom);
    if (zoom <= 1.001f)
    {
        zoom = 1.0f;
        scrollOffset = 0.0f;
    }
    else
    {
        const float visibleFrac = 1.0f / zoom;
        scrollOffset = juce::jlimit(0.0f, 1.0f - visibleFrac, centerFrac - visibleFrac * 0.5f);
    }
    if (zoomLabel != nullptr)
        zoomLabel->setText(juce::String(zoom, 1) + "x", juce::dontSendNotification);
    recomputePeaks();
    repaint();
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float topBarH = 26.f;
    auto waveArea = getLocalBounds().toFloat().withTrimmedTop(topBarH).reduced(8.f, 4.f);
    if (! waveArea.contains((float) e.x, (float) e.y))
        return;

    const float mouseFrac = xToFrac((float) e.x, waveArea.getX(), waveArea.getWidth());
    if (std::abs(wheel.deltaY) > 0.001f)
    {
        const float factor = wheel.deltaY > 0 ? 1.25f : 0.8f;
        const float newZoom = juce::jlimit(1.0f, 32.0f, zoom * factor);
        if (newZoom <= 1.001f)
        {
            zoom = 1.0f;
            scrollOffset = 0.0f;
        }
        else
        {
            const float visibleFrac = 1.0f / newZoom;
            const float cursorRatio = ((float) e.x - waveArea.getX()) / waveArea.getWidth();
            scrollOffset = juce::jlimit(0.0f, 1.0f - visibleFrac, mouseFrac - cursorRatio * visibleFrac);
            zoom = newZoom;
        }
        if (zoomLabel != nullptr)
            zoomLabel->setText(juce::String(zoom, 1) + "x", juce::dontSendNotification);
        recomputePeaks();
        repaint();
    }
    else if (std::abs(wheel.deltaX) > 0.001f && zoom > 1.0f)
    {
        const float visibleFrac = 1.0f / zoom;
        scrollOffset = juce::jlimit(0.0f, 1.0f - visibleFrac, scrollOffset - wheel.deltaX * visibleFrac * 0.5f);
        recomputePeaks();
        repaint();
    }
}

void WaveformDisplay::setSample(SampleManager::Ptr s)
{
    sample = s;
    updateMarkers();
    recomputePeaks();
    if (sample != nullptr && sample->buffer.getNumSamples() > 0)
    {
        const double sec = (double) sample->buffer.getNumSamples() / sample->sampleRate;
        infoLabel->setText(sample->name + "  (" + juce::String(sec, 2) + "s, "
                           + juce::String((int) sample->sampleRate) + " Hz)",
                           juce::dontSendNotification);
    }
    else
    {
        infoLabel->setText("Drag audio file here or click 'Load Sample'", juce::dontSendNotification);
    }
    repaint();
}

void WaveformDisplay::updateMarkers()
{
    auto st = padGrid.padState(pad);
    if (st.isValid())
    {
        startFrac     = (float) (double) st.getProperty("sampleStart", 0.0);
        endFrac       = (float) (double) st.getProperty("sampleEnd", 1.0);
        loopStartFrac = (float) (double) st.getProperty("loopStart", 0.0);
        loopEndFrac   = (float) (double) st.getProperty("loopEnd", 1.0);
        isLoop        = bool(st.getProperty("loopOn", false));
        isRev         = bool(st.getProperty("reverseOn", false));

        loopBtn->setToggleState(isLoop, juce::dontSendNotification);
        revBtn->setToggleState(isRev, juce::dontSendNotification);
    }
}

void WaveformDisplay::recomputePeaks()
{
    peaks.clear();
    const int w = getWidth() - 16;
    if (w <= 0 || sample == nullptr || sample->buffer.getNumSamples() <= 0)
        return;

    const int numSamples = sample->buffer.getNumSamples();
    const float* dataL = sample->buffer.getReadPointer(0);
    const float* dataR = sample->buffer.getNumChannels() > 1 ? sample->buffer.getReadPointer(1) : dataL;

    peaks.resize((size_t) w);
    const double visibleStart = (double) scrollOffset * (double) numSamples;
    const double visibleLen   = ((double) numSamples / (double) zoom);
    const double step = visibleLen / (double) w;

    for (int x = 0; x < w; ++x)
    {
        const size_t s0 = (size_t) juce::jlimit(0.0, (double) numSamples - 1, visibleStart + (double) x * step);
        const size_t s1 = (size_t) juce::jlimit(0.0, (double) numSamples, visibleStart + (double) (x + 1) * step);
        float minV = 0.f, maxV = 0.f;
        for (size_t s = s0; s < s1; ++s)
        {
            const float v = (dataL[s] + dataR[s]) * 0.5f;
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
        peaks[(size_t) x] = { minV, maxV };
    }
}

void WaveformDisplay::resized()
{
    const int w = getWidth();
    infoLabel->setBounds(8, 4, w - 280, 20);
    zoomLabel->setBounds(w - 275, 4, 34, 20);
    zoomInBtn->setBounds(w - 240, 3, 24, 22);
    zoomOutBtn->setBounds(w - 214, 3, 24, 22);
    zoomResetBtn->setBounds(w - 188, 3, 30, 22);
    loopBtn->setBounds(w - 154, 3, 72, 22);
    revBtn->setBounds(w - 78, 3, 70, 22);
    recomputePeaks();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float topBarH = 26.f;
    const float miniMapH = (zoom > 1.05f) ? 8.f : 0.f;
    auto waveArea = r.withTrimmedTop(topBarH).reduced(8.f, 4.f);
    if (miniMapH > 0.f)
        waveArea = waveArea.withTrimmedBottom(miniMapH + 2.f);

    // Chassis card
    ui::drawForgedPlate(g, r, 6.f, false);

    // Waveform viewport: deep smelting crucible interior
    g.setColour(juce::Colour(0xFF0C0807));
    g.fillRoundedRectangle(waveArea, 4.f);
    g.setColour(ui::line().withAlpha(0.55f));
    g.drawRoundedRectangle(waveArea, 4.f, 1.f);

    if (waveArea.getWidth() <= 10.f || waveArea.getHeight() <= 10.f)
        return;

    const float midY = waveArea.getCentreY();
    const float halfH = waveArea.getHeight() * 0.46f;
    const float left = waveArea.getX();
    const float width = waveArea.getWidth();

    // Center zero baseline (smoked steel reference line)
    g.setColour(ui::line().withAlpha(0.4f));
    g.drawHorizontalLine((int) midY, left, waveArea.getRight());

    // Shaded loop region (furnace amber ambient glow)
    if (isLoop)
    {
        const float lx0 = fracToX(loopStartFrac, left, width);
        const float lx1 = fracToX(loopEndFrac, left, width);
        if (lx1 > left && lx0 < waveArea.getRight())
        {
            const float clx0 = juce::jlimit(left, waveArea.getRight(), lx0);
            const float clx1 = juce::jlimit(left, waveArea.getRight(), lx1);
            g.setColour(juce::Colour(0xFFFF7700).withAlpha(0.12f));
            g.fillRect(juce::Rectangle<float>(clx0, waveArea.getY(), std::max(2.f, clx1 - clx0), waveArea.getHeight()));
            g.setColour(juce::Colour(0xFFFF9900).withAlpha(0.35f));
            if (lx0 >= left && lx0 <= waveArea.getRight()) g.drawVerticalLine((int) lx0, waveArea.getY(), waveArea.getBottom());
            if (lx1 >= left && lx1 <= waveArea.getRight()) g.drawVerticalLine((int) lx1, waveArea.getY(), waveArea.getBottom());
        }
    }

    // Draw waveform peaks in molten metal spectrum
    if (! peaks.empty())
    {
        for (int x = 0; x < (int) peaks.size(); ++x)
        {
            const float px = left + (float) x;
            const float frac = xToFrac(px, left, width);
            const bool inActiveRegion = (frac >= startFrac && frac <= endFrac);

            const float y0 = midY + peaks[(size_t) x].first * halfH;
            const float y1 = midY + peaks[(size_t) x].second * halfH;
            const float topY = std::min(y0, y1);
            const float botY = std::max(y0, y1);

            if (inActiveRegion)
            {
                juce::Colour peakCol = (botY - topY > halfH * 0.7f) ? ui::accentHot() : ui::accent();
                g.setGradientFill(juce::ColourGradient(peakCol, px, topY,
                                                       ui::ember(), px, botY + 1.f, false));
            }
            else
            {
                g.setColour(juce::Colour(0xFF453833));
            }

            g.drawVerticalLine((int) px, topY, botY + 1.f);
        }
    }
    else if (sample == nullptr)
    {
        g.setColour(ui::dim().withAlpha(0.5f));
        g.setFont(uiFont(12.f));
        g.drawText("No sample loaded - drop or load sample", waveArea, juce::Justification::centred);
    }

    // Handles & vertical marker lines
    auto drawMarker = [&](float frac, juce::Colour col, const char* label, bool isTop)
    {
        const float mx = fracToX(frac, left, width);
        if (mx < left - 15.f || mx > waveArea.getRight() + 15.f)
            return;

        g.setColour(col.withAlpha(0.35f));
        g.drawVerticalLine((int) mx, waveArea.getY(), waveArea.getBottom());
        g.setColour(col);
        g.drawVerticalLine((int) mx, waveArea.getY(), waveArea.getBottom());

        const float tagW = 22.f, tagH = 14.f;
        const float tagY = isTop ? waveArea.getY() + 2.f : waveArea.getBottom() - tagH - 2.f;
        auto tagRc = juce::Rectangle<float>(mx - tagW * 0.5f, tagY, tagW, tagH);

        g.setColour(col.darker(0.2f));
        g.fillRoundedRectangle(tagRc, 2.f);
        g.setColour(ui::accentHot());
        g.drawRoundedRectangle(tagRc, 2.f, 1.f);

        g.setColour(juce::Colour(0xFF0E0B0A));
        g.setFont(uiFont(8.5f, true));
        g.drawText(label, tagRc, juce::Justification::centred);
    };

    if (isLoop)
    {
        drawMarker(loopStartFrac, juce::Colour(0xFFFFB703), "LS", false);
        drawMarker(loopEndFrac,   juce::Colour(0xFFFFA000), "LE", false);
    }

    drawMarker(startFrac, ui::accentHot(), "S", true);
    drawMarker(endFrac,   ui::accentGlow(), "E", true);

    // Mini-map overview scrollbar when zoomed in
    if (miniMapH > 0.f)
    {
        const float mmY = waveArea.getBottom() + 2.f;
        auto mmRc = juce::Rectangle<float>(left, mmY, width, miniMapH);
        g.setColour(juce::Colour(0xFF140F0E));
        g.fillRoundedRectangle(mmRc, 3.f);
        g.setColour(ui::line().withAlpha(0.4f));
        g.drawRoundedRectangle(mmRc, 3.f, 1.f);

        const float thumbX = left + scrollOffset * width;
        const float thumbW = (1.0f / zoom) * width;
        auto thumbRc = juce::Rectangle<float>(thumbX, mmY, std::max(4.f, thumbW), miniMapH);
        g.setColour(ui::accent().withAlpha(0.6f));
        g.fillRoundedRectangle(thumbRc, 2.f);
        g.setColour(ui::accentHot());
        g.drawRoundedRectangle(thumbRc, 2.f, 1.f);
    }
}

WaveformDisplay::DragHandle WaveformDisplay::getHandleAt(float x, float y) const
{
    const float topBarH = 26.f;
    const float miniMapH = (zoom > 1.05f) ? 8.f : 0.f;
    auto waveArea = getLocalBounds().toFloat().withTrimmedTop(topBarH).reduced(8.f, 4.f);
    const float left = waveArea.getX();
    const float width = waveArea.getWidth();

    if (miniMapH > 0.f && y >= waveArea.getBottom() - 2.f)
        return HandleMiniMap;

    auto nearX = [&](float frac)
    {
        const float sx = fracToX(frac, left, width);
        return std::abs(x - sx) < 10.f;
    };

    if (nearX(startFrac)) return HandleStart;
    if (nearX(endFrac))   return HandleEnd;
    if (isLoop)
    {
        if (nearX(loopStartFrac)) return HandleLoopStart;
        if (nearX(loopEndFrac))   return HandleLoopEnd;
    }
    return None;
}

void WaveformDisplay::mouseMove(const juce::MouseEvent& e)
{
    const auto h = getHandleAt((float) e.x, (float) e.y);
    if (h == HandleMiniMap)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (h != None)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    activeDrag = getHandleAt((float) e.x, (float) e.y);
    if (activeDrag == HandleMiniMap)
    {
        const float topBarH = 26.f;
        auto waveArea = getLocalBounds().toFloat().withTrimmedTop(topBarH).reduced(8.f, 4.f);
        const float frac = juce::jlimit(0.f, 1.f, ((float) e.x - waveArea.getX()) / waveArea.getWidth());
        const float visibleFrac = 1.0f / zoom;
        scrollOffset = juce::jlimit(0.0f, 1.0f - visibleFrac, frac - visibleFrac * 0.5f);
        recomputePeaks();
        repaint();
    }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == None)
        return;

    const float topBarH = 26.f;
    auto waveArea = getLocalBounds().toFloat().withTrimmedTop(topBarH).reduced(8.f, 4.f);
    const float left = waveArea.getX();
    const float width = waveArea.getWidth();

    if (activeDrag == HandleMiniMap)
    {
        const float frac = juce::jlimit(0.f, 1.f, ((float) e.x - left) / width);
        const float visibleFrac = 1.0f / zoom;
        scrollOffset = juce::jlimit(0.0f, 1.0f - visibleFrac, frac - visibleFrac * 0.5f);
        recomputePeaks();
        repaint();
        return;
    }

    const float frac = xToFrac((float) e.x, left, width);
    auto st = padGrid.padState(pad);

    if (activeDrag == HandleStart)
    {
        startFrac = std::min(frac, endFrac - 0.001f);
        st.setProperty("sampleStart", startFrac, nullptr);
        padGrid.runtime(pad).sampleStart.store(startFrac);
    }
    else if (activeDrag == HandleEnd)
    {
        endFrac = std::max(frac, startFrac + 0.001f);
        st.setProperty("sampleEnd", endFrac, nullptr);
        padGrid.runtime(pad).sampleEnd.store(endFrac);
    }
    else if (activeDrag == HandleLoopStart)
    {
        loopStartFrac = std::min(frac, loopEndFrac - 0.001f);
        st.setProperty("loopStart", loopStartFrac, nullptr);
        padGrid.runtime(pad).loopStart.store(loopStartFrac);
    }
    else if (activeDrag == HandleLoopEnd)
    {
        loopEndFrac = std::max(frac, loopStartFrac + 0.001f);
        st.setProperty("loopEnd", loopEndFrac, nullptr);
        padGrid.runtime(pad).loopEnd.store(loopEndFrac);
    }
    repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    activeDrag = None;
}

} // namespace f64
