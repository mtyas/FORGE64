#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include <functional>

namespace f64 {

class VCFGraphView : public juce::Component, private juce::Timer
{
public:
    VCFGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex);
    ~VCFGraphView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    std::function<void()> onParamsChanged;

private:
    void timerCallback() override;
    float freqToX(float freq) const;
    float xToFreq(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;
    void updateFromMouse(const juce::MouseEvent& e);

    juce::AudioProcessorValueTreeState& state;
    int pad;

    int filterType = 0; // 0 Bypass, 1 LP, 2 HP, 3 BP, 4 Notch
    float cutoff = 20000.f;
    float resonance = 0.707f;
    float envAmt = 0.f;

    bool isDragging = false;
    juce::Rectangle<float> plotArea;
};

} // namespace f64
