#pragma once
#include <JuceHeader.h>
#include "../Modulation/ModMatrix.h"
#include "UICommon.h"
#include <memory>
#include <vector>

namespace f64 {

// Table of matrix connections (source -> destination) with per-connection
// amount / invert / mute / delete. Optionally filtered by dest prefix
// (used for the per-pad "local modulations" list).
class ConnectionList : public juce::Component, private juce::ValueTree::Listener
{
public:
    ConnectionList(ModMatrix& m, juce::ValueTree matTree, juce::String prefix = {});
    ~ConnectionList() override;
    void resized() override;
    void refresh();

private:
    class Row;
    class Model;

    void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    ModMatrix& matrix;
    juce::ValueTree tree;
    juce::String prefix;
    juce::ListBox list;
    std::unique_ptr<Model> model;
    std::vector<juce::ValueTree> items;
};

class Forge64Processor;

// Right-hand panel: the 53 modulation sources (draggable badges, enable,
// per-source editors) plus the global modulation matrix table.
class ModPanel : public juce::Component, private juce::Timer
{
public:
    ModPanel(ModMatrix& m, juce::ValueTree srcTree, juce::ValueTree matTree, Forge64Processor* proc = nullptr);
    ~ModPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void openSourceEditor(int slot, juce::Component* near);
    void closeInspector();
    ModMatrix& matrix() { return matrixRef; }
    Forge64Processor* getProcessor() const { return procPtr; }

private:
    void timerCallback() override;

    class SourceListModel;
    class SourceRow;
    class SourceEditorContent;
    class SourceInspectorOverlay;

    ModMatrix& matrixRef;
    Forge64Processor* procPtr = nullptr;
    juce::ListBox sourceList;
    std::unique_ptr<SourceListModel> srcModel;
    std::unique_ptr<ConnectionList> conns;
    std::unique_ptr<juce::Label> titleA, titleB;
    std::unique_ptr<SourceInspectorOverlay> inspector;
};

} // namespace f64
