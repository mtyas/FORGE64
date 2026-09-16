#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include "ModRingKnob.h"
#include "ModPanel.h"
#include <functional>
#include <memory>
#include <vector>

namespace f64 {

class Forge64Processor;

// Focused pad dashboard ("zoom" view): source (sample/Lua), amplitude,
// 3-band EQ, compressor, drive, insert FX, sends, routing, Lua editor and
// the pad's local modulation list.
class PadEditor : public juce::Component, private juce::Timer
{
public:
    PadEditor(Forge64Processor& p, ModRingKnob::Services& svcs, int globalPad,
              std::function<void()> onBack);
    ~PadEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    int padIndex() const { return pad; }

private:
    class Content;
    class ScriptDocListener;

    ModRingKnob* makeKnob(const char* base, const char* label);
    ModRingKnob* makeChip(const char* destId, const char* label);
    juce::ComboBox* makeCombo(const char* base, const juce::StringArray& items);
    juce::Label* makeCaption(const char* text);
    void recompileScript();
    void updateSampleLabel();
    void chooseSample();
    void timerCallback() override; // debounced script save + recompile

    Forge64Processor& proc;
    ModRingKnob::Services& svcs;
    int pad;
    std::function<void()> backCb;

    Content* content = nullptr;
    std::unique_ptr<juce::Viewport> viewport;

    juce::TextButton* backBtn = nullptr;
    juce::Label *padLabel = nullptr, *nameEdit = nullptr, *sampleLabel = nullptr,
                *scriptErrLabel = nullptr;
    juce::TextButton *colourBtn = nullptr, *srcBtn = nullptr, *fileBtn = nullptr;
    juce::ToggleButton* scriptOnBtn = nullptr;
    juce::ComboBox *fxCombo = nullptr, *chokeCombo = nullptr, *busCombo = nullptr,
                   *modeCombo = nullptr, *chanCombo = nullptr, *noteCombo = nullptr;
    juce::CodeEditorComponent* scriptEditor = nullptr;
    std::unique_ptr<juce::CodeDocument> scriptDoc;
    std::unique_ptr<ScriptDocListener> docListener;
    ConnectionList* connList = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<juce::Label*> captions;
    std::vector<juce::Label*> routeLabels;
    std::vector<ModRingKnob*> ampKnobs, eqKnobs, dynKnobs, fxKnobs, chips;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> srcAtt;
};

} // namespace f64
