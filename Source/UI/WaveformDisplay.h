#pragma once
#include <JuceHeader.h>
#include "../Engine/PadGrid.h"
#include "../Engine/SampleManager.h"

namespace f64 {

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay(PadGrid& grid, int padIndex);
    ~WaveformDisplay() override = default;

    void setSample(SampleManager::Ptr s);
    void updateMarkers();
    void refresh();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setZoom(float newZoom, float centerFrac = 0.5f);
    float getZoom() const { return zoom; }

private:
    enum DragHandle { None = 0, HandleStart, HandleEnd, HandleLoopStart, HandleLoopEnd, HandleMiniMap };

    DragHandle getHandleAt(float x, float y) const;
    void recomputePeaks();
    float fracToX(float frac, float left, float width) const;
    float xToFrac(float x, float left, float width) const;

    PadGrid& padGrid;
    int pad;
    SampleManager::Ptr sample;

    std::vector<std::pair<float, float>> peaks; // min, max per pixel column
    float startFrac = 0.f;
    float endFrac = 1.f;
    float loopStartFrac = 0.f;
    float loopEndFrac = 1.f;
    bool isLoop = false;
    bool isRev = false;

    float zoom = 1.0f;          // 1.0x .. 32.0x
    float scrollOffset = 0.0f;  // 0.0 .. (1.0 - 1.0 / zoom)
    float miniMapDragStart = 0.0f;

    DragHandle activeDrag = None;

    std::unique_ptr<juce::ToggleButton> loopBtn;
    std::unique_ptr<juce::ToggleButton> revBtn;
    std::unique_ptr<juce::TextButton> zoomInBtn;
    std::unique_ptr<juce::TextButton> zoomOutBtn;
    std::unique_ptr<juce::TextButton> zoomResetBtn;
    std::unique_ptr<juce::Label> infoLabel;
    std::unique_ptr<juce::Label> zoomLabel;
};

} // namespace f64
