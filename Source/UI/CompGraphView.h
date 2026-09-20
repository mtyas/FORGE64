#pragma once
#include <JuceHeader.h>

namespace f64 {

class CompGraphView : public juce::Component, private juce::Timer
{
public:
    CompGraphView(juce::AudioProcessorValueTreeState& apvts, int padIndex);
    ~CompGraphView() override { stopTimer(); }

    void updateFromParams();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    std::function<void()> onParamsChanged;

private:
    void timerCallback() override;
    float dbToX(float db) const;
    float xToDb(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    juce::AudioProcessorValueTreeState& vts;
    int pad;

    float thresh = 0.f;  // -60 .. 0 dB
    float ratio  = 1.f;  // 1 .. 20

    bool draggingThresh = false;
};

} // namespace f64
