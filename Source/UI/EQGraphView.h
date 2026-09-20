#pragma once
#include <JuceHeader.h>

namespace f64 {

class EQGraphView : public juce::Component, private juce::Timer
{
public:
    EQGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex);
    ~EQGraphView() override { stopTimer(); }

    void updateFromParams();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    std::function<void()> onParamsChanged;

private:
    void timerCallback() override;
    float freqToX(float f) const;
    float xToFreq(float x) const;
    float gainToY(float gDb) const;
    float yToGain(float y) const;
    int getNodeAt(float x, float y) const;

    juce::AudioProcessorValueTreeState& vts;
    int pad;

    float lf = 200.f, lg = 0.f;
    float mf = 1000.f, mg = 0.f;
    float hf = 8000.f, hg = 0.f;

    int activeNode = -1;
};

} // namespace f64
