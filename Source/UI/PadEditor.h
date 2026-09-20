#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include "ModRingKnob.h"
#include "ModPanel.h"
#include "WaveformDisplay.h"
#include "EQGraphView.h"
#include "CompGraphView.h"
#include "VCFGraphView.h"
#include "LuaScriptEditorWindow.h"
#include "../Presets/ModulePresetManager.h"
#include "../Scripting/ModuleScripts.h"
#include <functional>
#include <memory>
#include <vector>

namespace f64 {

class Forge64Processor;

class PadEditor : public juce::Component, private juce::Timer
{
public:
    PadEditor(Forge64Processor& p, ModRingKnob::Services& svcs, int globalPad,
              std::function<void()> onBack);
    ~PadEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    int padIndex() const { return pad; }

    // Step P-Lock Mode
    void enterPLockMode(int trackIdx, int stepIdx);
    void exitPLockMode();
    bool isPLockMode() const { return inPLockMode; }

    std::function<void(int newPad)> onPadChanged;

private:
    class Content;

    ModRingKnob* makeKnob(const char* base, const char* label);
    ModRingKnob* makeChip(const char* destId, const char* label);
    juce::ComboBox* makeCombo(const char* base, const juce::StringArray& items);
    juce::Label* makeCaption(const char* text);
    void updateModuleControls(int srcType);
    void updateSampleLabel();
    void chooseSample();
    void updateIfxLabels(int ifxType);

    struct PadBaseParams
    {
        float tune = 0.f;
        float decay = 0.5f;
        float drive = 0.f;
        float fx1 = 0.5f, fx2 = 0.5f, fx3 = 0.5f, fx4 = 0.5f, fx5 = 0.5f;
        float sendA = 0.f, sendB = 0.f, sendC = 0.f, sendD = 0.f;
        float level = 1.f;
        float pan = 0.f;
        float modAmt = 1.0f;

        // VCF Filter
        int   vcfType = 0;
        float vcfCut = 20000.f, vcfRes = 0.707f, vcfEnv = 0.f;

        // 3-Band Parametric EQ
        float eqLF = 200.f, eqLG = 0.f, eqMF = 1000.f, eqMG = 0.f, eqHF = 8000.f, eqHG = 0.f;

        // Compressor
        float cThr = 0.f, cRat = 1.f, cAtk = 5.f, cRel = 100.f;

        // Insert Multi-FX
        int   ifxType = 0;
        float ifx1 = 0.5f, ifx2 = 0.5f, ifx3 = 0.5f, ifx4 = 0.5f;
    };
    PadBaseParams preLockParams;
    bool hadLocksOnEntry = false;

    void syncModuleSelectionQuiet();
    void setAPVTSParam(const char* base, float v);
    float getAPVTSParam(const char* base) const;

    // Unified Category Presets
    void populateCategorySounds(const juce::String& category);
    void loadCategorySound(int soundIndex);
    void showSavePresetDialog();

    // Detached Lua Editor
    void openScriptEditorWindow();

    // P-Lock Actions
    void saveCurrentParamsAsPLock(bool shouldAudition = false);
    void clearPLocksForStep();
    void triggerAuditionForCurrentStep();
    void auditionOnControlTouch();

    void timerCallback() override;

    Forge64Processor& proc;
    ModRingKnob::Services& svcs;
    int pad;
    std::function<void()> backCb;

    Content* content = nullptr;
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<LuaScriptEditorWindow> scriptWindow;

    // Header & P-Lock Banner
    juce::TextButton *prevPadBtn = nullptr, *nextPadBtn = nullptr, *playBtn = nullptr, *previewToggleBtn = nullptr, *colourBtn = nullptr, *fileBtn = nullptr;
    juce::Label *padLabel = nullptr, *nameEdit = nullptr, *sampleLabel = nullptr;
    juce::Label *moduleCaption = nullptr;
    static inline bool auditionOnTouchEnabled = true;

    // P-Lock Banner
    std::unique_ptr<juce::Component> pLockBanner;
    juce::Label* pLockBannerTitle = nullptr;
    juce::TextButton* pLockSaveBtn = nullptr;
    juce::TextButton* pLockClearBtn = nullptr;
    juce::TextButton* pLockExitBtn = nullptr;
    bool inPLockMode = false;
    bool isSyncingPLockUI = false;
    int pLockTrack = -1, pLockStep = -1;

    // Source & Unified Presets
    juce::ComboBox *srcCombo = nullptr;
    juce::ComboBox *categoryCombo = nullptr;
    juce::ComboBox *soundPresetCombo = nullptr;
    juce::TextButton *savePresetBtn = nullptr, *editScriptBtn = nullptr, *aiPromptBtn = nullptr;
    juce::Label *scriptStatusBadge = nullptr;

    // VCF Filter
    juce::ComboBox *vcfTypeCombo = nullptr;
    ModRingKnob *vcfCutKnob = nullptr, *vcfResKnob = nullptr, *vcfEnvKnob = nullptr;
    std::vector<ModRingKnob*> vcfKnobs;

    // Routing & FX
    juce::ComboBox *chokeCombo = nullptr, *busCombo = nullptr,
                   *modeCombo = nullptr, *chanCombo = nullptr, *noteCombo = nullptr;
    juce::ComboBox *ifxCombo = nullptr;
    ConnectionList* connList = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;

    WaveformDisplay* waveformDisplay = nullptr;
    VCFGraphView* vcfGraph = nullptr;
    EQGraphView* eqGraph = nullptr;
    CompGraphView* compGraph = nullptr;

    // Module Knobs - Row 1 (Core) and Row 2 (Variables & Mod)
    ModRingKnob *tuneKnob = nullptr, *decKnob = nullptr, *drvKnob = nullptr,
                *lvlKnob = nullptr,  *panKnob = nullptr;
    ModRingKnob *p1Knob = nullptr, *p2Knob = nullptr, *p3Knob = nullptr,
                *p4Knob = nullptr, *p5Knob = nullptr;

    ModRingKnob *satkKnob = nullptr, *sdecKnob = nullptr, *ssusKnob = nullptr, *srelKnob = nullptr;
    ModRingKnob *ifx1Knob = nullptr, *ifx2Knob = nullptr, *ifx3Knob = nullptr, *ifx4Knob = nullptr;

    std::vector<juce::Label*> captions;
    std::vector<juce::Label*> routeLabels;
    std::vector<ModRingKnob*> synthKnobsRow1, synthKnobsRow2, smplKnobsRow1, smplKnobsRow2;
    std::vector<ModRingKnob*> eqKnobs, dynKnobs, sendKnobs, ifxKnobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAtt;

    juce::String currentModuleId = "kick_808";
    std::vector<ModulePresetManager::CategorySoundEntry> currentCategorySounds;
};

} // namespace f64
