#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/ModRingKnob.h"
#include "UI/PadGridView.h"
#include "UI/PadEditor.h"
#include "UI/ModPanel.h"
#include <array>
#include <vector>

namespace f64 {

class Forge64Editor : public juce::AudioProcessorEditor,
                      public ModRingKnob::Services,
                      public PadGridView::Host,
                      private juce::Timer
{
public:
    explicit Forge64Editor(Forge64Processor& p);
    ~Forge64Editor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ModRingKnob::Services
    ModMatrix* matrix() override { return &processor.mods(); }
    void connectFromDrag(int slot, const juce::String& dest) override;
    void registerKnob(ModRingKnob* k) override { knobList.push_back(k); }
    void unregisterKnob(ModRingKnob* k) override;

    // PadGridView::Host
    void padClicked(int globalPad) override;

    void setBank(int b);
    void showGrid();

private:
    void timerCallback() override;
    juce::Rectangle<int> centerBounds() const;
    void layoutCenter();
    void rebuildPadEditor();
    void showKitMenu();
    void showBankMenu();
    void showPadMenu();
    void doLoadKit();
    void doSaveKitAs();
    void doLoadBank();
    void doSaveBankAs();
    void doLoadPad();
    void doSavePadAs();
    void afterPresetOp(bool ok);
    int activePad() const { return selectedPad >= 0 ? selectedPad : currentBank * kPadsPerBank; }

    Forge64Processor& processor;

    std::unique_ptr<juce::Label> logo, tagline;
    std::array<std::unique_ptr<juce::TextButton>, kNumBanks> bankBtns;
    std::unique_ptr<ModRingKnob> masterKnob;
    std::unique_ptr<juce::SliderParameterAttachment> masterAtt;
    std::unique_ptr<juce::TextButton> kitBtn, bankMenuBtn, padMenuBtn;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastKit;

    std::unique_ptr<PadGridView> grid;
    std::unique_ptr<PadEditor> padEdit;
    std::unique_ptr<ModPanel> modPanel;
    std::vector<ModRingKnob*> knobList;
    juce::ComponentAnimator animator;

    int currentBank = 0;
    int zoomedPad = -1;
    int selectedPad = -1;
};

} // namespace f64
