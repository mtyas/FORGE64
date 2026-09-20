#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include "../Modulation/ModMatrix.h"
#include "UICommon.h"

namespace f64 {

// Rotary knob with live modulation rings: every connection targeting this
// control draws a coloured arc (per source class) showing its current
// contribution, plus a dot for the effective value. Also a drop target for
// modulation source badges ("f64mod:<slot>") - the drag & drop patching UX.
// In "chip" mode it renders as a small round drop zone (voice destinations).
class ModRingKnob : public juce::Slider, public juce::DragAndDropTarget
{
public:
    struct Services
    {
        virtual ~Services() = default;
        virtual ModMatrix* matrix() = 0;
        virtual void connectFromDrag(int slot, const juce::String& dest) = 0;
        virtual void registerKnob(ModRingKnob* k) = 0;
        virtual void unregisterKnob(ModRingKnob* k) = 0;
        virtual class MidiLearnManager* midiLearn() { return nullptr; }
        virtual juce::UndoManager* undoManager() { return nullptr; }
    };

    ModRingKnob(juce::StringRef destId, juce::StringRef labelText, Services& svcs, bool chipMode = false);
    ~ModRingKnob() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setLabel(juce::StringRef newLabel) { label = newLabel; repaint(); }
    juce::String getLabel() const { return label; }
    double getDefaultValue() const { return getDoubleClickReturnValue(); }

    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override { isDragOver = true; repaint(); }
    void itemDragExit(const SourceDetails& details) override { isDragOver = false; repaint(); }
    void itemDropped(const SourceDetails& details) override;

    const juce::String dest;

private:
    void showConnMenu();

    juce::String label;
    Services& services;
    bool chip;
    bool isDragOver = false;
};

} // namespace f64
