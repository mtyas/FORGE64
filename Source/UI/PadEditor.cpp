#include "PadEditor.h"
#include "UICommon.h"
#include "../PluginProcessor.h"
#include "../Scripting/AIPromptHelper.h"

namespace f64 {

static const int kPadColours[8] = {
    (int) 0xFFFF6B00, // Molten Flame Orange
    (int) 0xFFFFAA00, // Forge Gold
    (int) 0xFFD62828, // Crimson Ember
    (int) 0xFFFFD166, // Incandescent Yellow
    (int) 0xFFC75D2C, // Scorched Copper
    (int) 0xFF3898EC, // Tempered Blued Steel
    (int) 0xFF8F5B34, // Raw Iron Rust
    (int) 0xFF584D47  // Smoked Anvil Charcoal
};

// ---------------------------------------------------------------------------
class PadEditor::Content : public juce::Component
{
public:
    explicit Content(PadEditor& o) : owner(o) {}
    ~Content() override { deleteAllChildren(); }
    void resized() override;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(ui::bg());

        auto drawCard = [&](int y, int h)
        {
            auto box = juce::Rectangle<float>(6.f, (float) y, (float) getWidth() - 12.f, (float) h);
            ui::drawForgedPlate(g, box, 6.f, false);
        };

        const int topOffset = owner.isPLockMode() ? 40 : 0;

        drawCard(40 + topOffset, 222);  // Card 1: Module Sound Engine + Presets + Knobs
        drawCard(270 + topOffset, 186); // Card 2: Unified Processing Strip (VCF + EQ + Compressor)
        drawCard(464 + topOffset, 96);  // Card 3: Aux Sends & Dedicated Multi-FX Insert
        drawCard(568 + topOffset, 62);  // Card 4: Routing & MIDI
        drawCard(638 + topOffset, 186); // Card 5: Pad Local Modulations
    }

private:
    PadEditor& owner;
};

// ---------------------------------------------------------------------------
PadEditor::PadEditor(Forge64Processor& p, ModRingKnob::Services& s, int globalPad,
                     std::function<void()> onBack)
    : proc(p), svcs(s), pad(globalPad), backCb(std::move(onBack))
{
    ModulePresetManager::initializeOnDisk();

    content = new Content(*this);
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(content, true);
    addAndMakeVisible(viewport.get());

    const auto st = proc.grid().padState(pad);

    // ---- P-Lock Bottom Bar (shown docked at bottom when editing a step's locks) -------
    class PLockBarComponent : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xFF1E1715));
            g.fillRoundedRectangle(r, 4.f);
            g.setColour(juce::Colour(0xFFFF9A3C).withAlpha(0.18f));
            g.fillRoundedRectangle(r.reduced(1.f), 3.f);
            g.setColour(juce::Colour(0xFFFF9A3C).withAlpha(0.65f));
            g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.2f);
        }
    };

    pLockBanner = std::make_unique<PLockBarComponent>();
    pLockBannerTitle = ui::makeLabel("P-LOCK EDIT MODE", 12.f, juce::Colour(0xFFFFD166)).release();
    pLockBannerTitle->setFont(uiFont(12.f, true));
    pLockBanner->addAndMakeVisible(pLockBannerTitle);

    pLockSaveBtn = new juce::TextButton("SAVE TO STEP");
    ui::styleButton(*pLockSaveBtn);
    pLockSaveBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF7A3E00));
    pLockSaveBtn->onClick = [this] { saveCurrentParamsAsPLock(false); };
    pLockBanner->addAndMakeVisible(pLockSaveBtn);

    pLockClearBtn = new juce::TextButton("CLEAR LOCKS");
    ui::styleButton(*pLockClearBtn);
    pLockClearBtn->onClick = [this] { clearPLocksForStep(); };
    pLockBanner->addAndMakeVisible(pLockClearBtn);

    pLockExitBtn = new juce::TextButton("EXIT");
    ui::styleButton(*pLockExitBtn);
    pLockExitBtn->onClick = [this] { exitPLockMode(); };
    pLockBanner->addAndMakeVisible(pLockExitBtn);

    pLockBanner->setVisible(false);
    addAndMakeVisible(pLockBanner.get());

    // ---- header -------------------------------------------------------
    prevPadBtn = new juce::TextButton("<");
    ui::styleButton(*prevPadBtn);
    prevPadBtn->setTooltip("Previous pad (A01..D16)");
    prevPadBtn->onClick = [this]
    {
        if (inPLockMode)
            exitPLockMode();
        const int prev = (pad - 1 + kNumPads) % kNumPads;
        if (onPadChanged)
            onPadChanged(prev);
    };
    content->addAndMakeVisible(prevPadBtn);

    nextPadBtn = new juce::TextButton(">");
    ui::styleButton(*nextPadBtn);
    nextPadBtn->setTooltip("Next pad (A01..D16)");
    nextPadBtn->onClick = [this]
    {
        if (inPLockMode)
            exitPLockMode();
        const int nxt = (pad + 1) % kNumPads;
        if (onPadChanged)
            onPadChanged(nxt);
    };
    content->addAndMakeVisible(nextPadBtn);

    const int bankIdx = pad / kPadsPerBank;
    const char bankChar = (char) ('A' + bankIdx);
    const juce::String padCoord = juce::String::charToString(bankChar)
                                + juce::String::formatted("%02d", (pad % kPadsPerBank) + 1);

    padLabel = ui::makeLabel(padCoord, 16.f, ui::accent()).release();
    padLabel->setFont(uiFont(16.f, true));
    content->addAndMakeVisible(padLabel);

    playBtn = new juce::TextButton("PLAY");
    playBtn->setTooltip("Audition this pad");
    ui::styleButton(*playBtn);
    playBtn->onClick = [this] { proc.triggerAudition(pad, 0.9f); };
    content->addAndMakeVisible(playBtn);

    previewToggleBtn = new juce::TextButton(auditionOnTouchEnabled ? "PREVIEW: ON" : "PREVIEW: OFF");
    ui::styleButton(*previewToggleBtn);
    previewToggleBtn->setTooltip("Toggle automatic sound preview when moving parameter knobs");
    previewToggleBtn->setColour(juce::TextButton::buttonColourId,
                                auditionOnTouchEnabled ? ui::accent() : ui::panelHi());
    previewToggleBtn->setToggleState(auditionOnTouchEnabled, juce::dontSendNotification);
    previewToggleBtn->onClick = [this]
    {
        auditionOnTouchEnabled = ! auditionOnTouchEnabled;
        previewToggleBtn->setToggleState(auditionOnTouchEnabled, juce::dontSendNotification);
        previewToggleBtn->setButtonText(auditionOnTouchEnabled ? "PREVIEW: ON" : "PREVIEW: OFF");
        previewToggleBtn->setColour(juce::TextButton::buttonColourId,
                                    auditionOnTouchEnabled ? ui::accent() : ui::panelHi());
    };
    content->addAndMakeVisible(previewToggleBtn);

    nameEdit = new juce::Label();
    nameEdit->setText(st.getProperty("name", "").toString(), juce::dontSendNotification);
    nameEdit->setFont(uiFont(13.f));
    nameEdit->setEditable(false, true);
    nameEdit->setColour(juce::Label::backgroundColourId, ui::panelHi());
    nameEdit->setColour(juce::Label::textColourId, ui::txt());
    nameEdit->setColour(juce::Label::outlineColourId, ui::line());
    nameEdit->setTooltip("Pad name (double-click to rename)");
    nameEdit->onTextChange = [this]
    {
        proc.grid().padState(pad).setProperty("name", nameEdit->getText(), nullptr);
    };
    content->addAndMakeVisible(nameEdit);

    colourBtn = new juce::TextButton();
    colourBtn->setTooltip("Pad colour");
    colourBtn->setColour(juce::TextButton::buttonColourId,
                         juce::Colour((juce::uint32) (int) st.getProperty("colour", (int) 0xFF3A6EA5)));
    colourBtn->onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < 8; ++i)
            menu.addItem(10 + i, "Colour " + juce::String(i + 1));
        juce::Component::SafePointer<PadEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(colourBtn),
                           [safe](int result)
        {
            if (safe == nullptr || result < 10 || result >= 18)
                return;
            const int c = kPadColours[result - 10];
            safe->proc.grid().padState(safe->pad).setProperty("colour", c, nullptr);
            safe->colourBtn->setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32) c));
        });
    };
    content->addAndMakeVisible(colourBtn);

    srcCombo = new juce::ComboBox();
    ui::styleCombo(*srcCombo);
    srcCombo->addItem("Sample Playback", 1);
    srcCombo->addItem("Lua DSP Synthesizer", 2);
    srcCombo->setTooltip("Sound generator engine type (Sample or Lua DSP)");
    srcCombo->onChange = [this]
    {
        const int sel = srcCombo->getSelectedId();
        const int newSrc = (sel == 1) ? (int) SRC_SAMPLE : (int) SRC_LUA;
        setAPVTSParam("src", (float) newSrc);
        updateModuleControls(newSrc);
    };
    content->addAndMakeVisible(srcCombo);

    // ---- Unified Category Presets Controls ------------------------------
    categoryCombo = new juce::ComboBox();
    ui::styleCombo(*categoryCombo);
    categoryCombo->setTooltip("Sound Category");
    const auto categories = ModulePresetManager::getModuleCategories();
    for (int i = 0; i < categories.size(); ++i)
        categoryCombo->addItem(categories[i], i + 1);
    categoryCombo->setSelectedId(1, juce::dontSendNotification);
    categoryCombo->onChange = [this]
    {
        populateCategorySounds(categoryCombo->getText());
    };
    content->addAndMakeVisible(categoryCombo);

    soundPresetCombo = new juce::ComboBox();
    ui::styleCombo(*soundPresetCombo);
    soundPresetCombo->setTooltip("Sound Preset / Algorithm");
    soundPresetCombo->onChange = [this]
    {
        const int idx = soundPresetCombo->getSelectedId() - 1;
        if (idx >= 0 && idx < (int) currentCategorySounds.size())
            loadCategorySound(idx);
    };
    content->addAndMakeVisible(soundPresetCombo);

    savePresetBtn = new juce::TextButton("SAVE PRESET");
    ui::styleButton(*savePresetBtn);
    savePresetBtn->setTooltip("Save current sound parameters and algorithm as a preset");
    savePresetBtn->onClick = [this] { showSavePresetDialog(); };
    content->addAndMakeVisible(savePresetBtn);

    editScriptBtn = new juce::TextButton("EDIT SCRIPT...");
    ui::styleButton(*editScriptBtn);
    editScriptBtn->setTooltip("Open floating Lua script editor window to view or edit this module's DSP algorithm");
    editScriptBtn->onClick = [this] { openScriptEditorWindow(); };
    content->addAndMakeVisible(editScriptBtn);

    aiPromptBtn = new juce::TextButton("AI PROMPT");
    ui::styleButton(*aiPromptBtn);
    aiPromptBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF6E3600));
    aiPromptBtn->setTooltip("Copy master prompt for coding agents (ChatGPT, Claude, Antigravity) to create custom Lua drum modules");
    aiPromptBtn->onClick = [this] { AIPromptHelper::showAIPromptDialog(this); };
    content->addAndMakeVisible(aiPromptBtn);

    scriptStatusBadge = ui::makeLabel("LUA: ACTIVE", 10.f, juce::Colour(0xFF70E000)).release();
    content->addAndMakeVisible(scriptStatusBadge);

    fileBtn = new juce::TextButton("Load Sample");
    ui::styleButton(*fileBtn);
    fileBtn->onClick = [this] { chooseSample(); };
    content->addAndMakeVisible(fileBtn);

    sampleLabel = ui::makeLabel("", 11.f, ui::dim()).release();
    content->addAndMakeVisible(sampleLabel);

    // ---- Card 1 Knobs ---------------------------------------------------
    moduleCaption = makeCaption("MODULE SYNTHESIS ENGINE (LUA DSP)");

    // Row 1: Core Sound Controls
    tuneKnob = makeKnob("tune", "PITCH");
    decKnob  = makeKnob("dec",  "DECAY");
    drvKnob  = makeKnob("drv",  "DRIVE");
    lvlKnob  = makeKnob("lvl",  "LEVEL");
    panKnob  = makeKnob("pan",  "PAN");
    synthKnobsRow1 = { tuneKnob, decKnob, drvKnob, lvlKnob, panKnob };

    // Row 2: Module Variable Parameters (P1 to P5)
    p1Knob = makeKnob("fx1", "P1");
    p2Knob = makeKnob("fx2", "P2");
    p3Knob = makeKnob("fx3", "P3");
    p4Knob = makeKnob("fx4", "P4");
    p5Knob = makeKnob("fx5", "P5");

    synthKnobsRow2 = { p1Knob, p2Knob, p3Knob, p4Knob, p5Knob };

    // Sampler knobs
    satkKnob = makeKnob("satk", "ATTACK");
    sdecKnob = makeKnob("sdec", "DECAY");
    ssusKnob = makeKnob("ssus", "SUSTAIN");
    srelKnob = makeKnob("srel", "RELEASE");
    smplKnobsRow1 = { satkKnob, sdecKnob, ssusKnob, srelKnob };
    smplKnobsRow2 = { tuneKnob, lvlKnob, panKnob, drvKnob };

    waveformDisplay = new WaveformDisplay(proc.grid(), pad);
    content->addAndMakeVisible(waveformDisplay);

    // ---- Card 2: Unified Processing Strip (VCF + EQ + Compressor) -------
    makeCaption("DYNAMICS & TONE PROCESSING: RESONANT VCF // 3-BAND PARAMETRIC EQ // COMPRESSOR");

    // VCF Filter (Left Column)
    vcfTypeCombo = makeCombo("vcft", { "Off (Bypass)", "Lowpass 12dB", "Highpass 12dB", "Bandpass 12dB", "Notch 12dB" });
    vcfTypeCombo->setTooltip("Resonant multi-mode filter positioned before EQ and compression");
    vcfTypeCombo->onChange = [this]
    {
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(true);
    };

    vcfGraph = new VCFGraphView(proc.getAPVTS(), pad);
    content->addAndMakeVisible(vcfGraph);
    vcfGraph->onParamsChanged = [this]
    {
        for (auto* k : vcfKnobs)
            k->repaint();
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(false);
    };

    vcfCutKnob = makeKnob("vcfc", "CUTOFF");
    vcfResKnob = makeKnob("vcfr", "RESON");
    vcfEnvKnob = makeKnob("vcfe", "ENV AMT");
    vcfKnobs = { vcfCutKnob, vcfResKnob, vcfEnvKnob };

    // 3-Band Parametric EQ (Center Column)
    eqGraph = new EQGraphView(proc.getAPVTS(), pad);
    content->addAndMakeVisible(eqGraph);
    eqGraph->onParamsChanged = [this]
    {
        for (auto* k : eqKnobs)
            k->repaint();
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(false);
    };
    eqKnobs = { makeKnob("eqlf", "LO FREQ"), makeKnob("eqlg", "LO GAIN"),
                makeKnob("eqmf", "MID FREQ"), makeKnob("eqmg", "MID GAIN"),
                makeKnob("eqhf", "HI FREQ"), makeKnob("eqhg", "HI GAIN") };

    // Dynamics Compressor (Right Column)
    compGraph = new CompGraphView(proc.getAPVTS(), pad);
    content->addAndMakeVisible(compGraph);
    compGraph->onParamsChanged = [this]
    {
        for (auto* k : dynKnobs)
            k->repaint();
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(false);
    };
    dynKnobs = { makeKnob("cthr", "THRESH"), makeKnob("crat", "RATIO"),
                 makeKnob("catk", "ATTACK"), makeKnob("crel", "RELEASE"),
                 makeKnob("cmg",  "MAKEUP") };

    // ---- Card 3: Aux Sends & Insert FX ----------------------------------
    makeCaption("AUX SENDS & DEDICATED MULTI-FX INSERT");
    ifxCombo = makeCombo("ifx", { "Off", "Flanger", "Chorus", "Crusher", "Phaser",
                                  "Overdrive", "Fuzz", "Tape Echo", "Plate Reverb",
                                  "Pitch Shift", "Formant", "Ring Mod",
                                  "Hall Reverb", "Chamber Reverb", "Spring Reverb", "Gated Reverb",
                                  "Ping-Pong Delay", "Dub Delay", "Tube Saturator", "Wavefolder",
                                  "Freq Shifter", "Stereo Detuner" });
    ifxCombo->setTooltip("Dedicated pad insert multi-effect");
    ifx1Knob = makeKnob("ifx1", "RATE");
    ifx2Knob = makeKnob("ifx2", "DEPTH");
    ifx3Knob = makeKnob("ifx3", "FEEDBACK");
    ifx4Knob = makeKnob("ifx4", "MIX");
    ifxKnobs = { ifx1Knob, ifx2Knob, ifx3Knob, ifx4Knob };
    ifxCombo->onChange = [this]
    {
        int cur = ifxCombo->getSelectedItemIndex();
        if (cur < 0)
        {
            if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "ifx")))
                cur = (int) p->load();
        }
        updateIfxLabels(cur);
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(true);
    };

    sendKnobs = { makeKnob("snda", "SEND A"), makeKnob("sndb", "SEND B"),
                  makeKnob("sndc", "SEND C"), makeKnob("sndd", "SEND D") };

    // ---- Card 4: Routing & MIDI -----------------------------------------
    makeCaption("ROUTING & MIDI");
    for (const char* t : { "CHOKE", "OUT BUS", "MODE", "MIDI CH", "NOTE" })
    {
        auto l = ui::makeLabel(t, 9.f, ui::dim());
        routeLabels.push_back(l.get());
        content->addAndMakeVisible(l.release());
    }

    juce::StringArray chokeItems;
    chokeItems.add("Off");
    for (int i = 1; i <= kNumChokes; ++i) chokeItems.add(juce::String(i));
    chokeCombo = makeCombo("chok", chokeItems);

    juce::StringArray busItems;
    for (int i = 1; i <= kNumBuses; ++i) busItems.add("Bus " + juce::String(i));
    busCombo = makeCombo("obus", busItems);

    modeCombo = makeCombo("pmode", { "Drum Trigger", "Chromatic" });

    juce::StringArray chanItems;
    chanItems.add("Any");
    for (int i = 1; i <= 16; ++i) chanItems.add(juce::String(i));
    chanCombo = makeCombo("mchan", chanItems);

    juce::StringArray noteItems;
    for (int i = 0; i < 128; ++i)
        noteItems.add(juce::MidiMessage::getMidiNoteName(i, true, true, 3));
    noteCombo = makeCombo("mnote", noteItems);

    // ---- Card 5: Local Modulations --------------------------------------
    makeCaption("PAD MODULATIONS");
    connList = new ConnectionList(proc.mods(), proc.getKit().getChildWithName("MODMAT"),
                                  "p" + juce::String(pad) + "_");
    content->addAndMakeVisible(connList);

    // Initial setup - sync module quietly without overwriting pad parameters!
    syncModuleSelectionQuiet();
    int initialSrc = 0;
    if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
        initialSrc = (int) p->load();
    updateModuleControls(initialSrc);
    updateSampleLabel();

    startTimerHz(10);
}

PadEditor::~PadEditor()
{
    stopTimer();
    exitPLockMode();
}

float PadEditor::getAPVTSParam(const char* base) const
{
    if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, base)))
        return p->load();
    return 0.f;
}

void PadEditor::setAPVTSParam(const char* base, float v)
{
    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(
            proc.getAPVTS().getParameter(padParamId(pad, base))))
    {
        par->setValueNotifyingHost(par->convertTo0to1(v));
    }
}

void PadEditor::timerCallback()
{
    const auto err = proc.lua().errorFor(pad);
    if (scriptStatusBadge != nullptr)
    {
        if (err.isEmpty())
        {
            scriptStatusBadge->setText("LUA: COMPILED OK", juce::dontSendNotification);
            scriptStatusBadge->setColour(juce::Label::textColourId, juce::Colour(0xFF70E000));
        }
        else
        {
            scriptStatusBadge->setText("LUA ERR: " + err, juce::dontSendNotification);
            scriptStatusBadge->setColour(juce::Label::textColourId, juce::Colour(0xFFFF5252));
        }
    }
}

// ---------------------------------------------------------------------------
// Step P-Lock Mode
// ---------------------------------------------------------------------------
void PadEditor::enterPLockMode(int trackIdx, int stepIdx)
{
    // 1. Snapshot pad base parameters ONLY when first entering P-Lock mode from normal mode!
    if (! inPLockMode)
    {
        proc.fillPadParams(pad, preLockParams, 0.0f);
        inPLockMode = true;
    }
    proc.setPadPLockPreviewActive(pad, true, preLockParams);

    pLockTrack = trackIdx;
    pLockStep = stepIdx;

    isSyncingPLockUI = true;

    // 2. If the step already has locks, load them into the APVTS knobs!
    auto& seq = proc.getSequencer();
    const auto& sd = seq.currentPattern().tracks[(size_t) trackIdx].steps[(size_t) stepIdx];
    hadLocksOnEntry = sd.hasLocks;
    if (sd.hasLocks)
    {
        setAPVTSParam("tune", sd.pLockPitch);
        setAPVTSParam("dec",  sd.pLockDecay);
        setAPVTSParam("drv",  sd.pLockDrive);
        setAPVTSParam("fx1",  sd.pLockTone);
        setAPVTSParam("fx2",  sd.pLockP2);
        setAPVTSParam("fx3",  sd.pLockP3);
        setAPVTSParam("fx4",  sd.pLockP4);
        setAPVTSParam("fx5",  sd.pLockP5);
        setAPVTSParam("snda", sd.pLockSendA);
        setAPVTSParam("sndb", sd.pLockSendB);
        setAPVTSParam("sndc", sd.pLockSendC);
        setAPVTSParam("sndd", sd.pLockSendD);
        setAPVTSParam("lvl",  sd.pLockLevel);
        setAPVTSParam("pan",  sd.pLockPan);

        setAPVTSParam("vcft", (float) sd.pLockVcfType);
        setAPVTSParam("vcfc", sd.pLockVcfCut);
        setAPVTSParam("vcfr", sd.pLockVcfRes);
        setAPVTSParam("vcfe", sd.pLockVcfEnv);

        setAPVTSParam("eqlf", sd.pLockEqLF);
        setAPVTSParam("eqlg", sd.pLockEqLG);
        setAPVTSParam("eqmf", sd.pLockEqMF);
        setAPVTSParam("eqmg", sd.pLockEqMG);
        setAPVTSParam("eqhf", sd.pLockEqHF);
        setAPVTSParam("eqhg", sd.pLockEqHG);

        setAPVTSParam("cthr", sd.pLockCThr);
        setAPVTSParam("crat", sd.pLockCRat);
        setAPVTSParam("catk", sd.pLockCAtk);
        setAPVTSParam("crel", sd.pLockCRel);
        setAPVTSParam("cmg",  sd.pLockCMg);

        setAPVTSParam("ifx",  (float) sd.pLockIfxType);
        setAPVTSParam("ifx1", sd.pLockIfx1);
        setAPVTSParam("ifx2", sd.pLockIfx2);
        setAPVTSParam("ifx3", sd.pLockIfx3);
        setAPVTSParam("ifx4", sd.pLockIfx4);
    }
    else
    {
        // Clean unlocked step - ensure knobs start at the pad's base values!
        setAPVTSParam("tune", preLockParams.tune);
        setAPVTSParam("dec",  preLockParams.decay);
        setAPVTSParam("drv",  preLockParams.drive);
        setAPVTSParam("fx1",  preLockParams.fx1);
        setAPVTSParam("fx2",  preLockParams.fx2);
        setAPVTSParam("fx3",  preLockParams.fx3);
        setAPVTSParam("fx4",  preLockParams.fx4);
        setAPVTSParam("fx5",  preLockParams.fx5);
        setAPVTSParam("snda", preLockParams.sendA);
        setAPVTSParam("sndb", preLockParams.sendB);
        setAPVTSParam("sndc", preLockParams.sendC);
        setAPVTSParam("sndd", preLockParams.sendD);
        setAPVTSParam("lvl",  preLockParams.level);
        setAPVTSParam("pan",  preLockParams.pan);

        setAPVTSParam("vcft", (float) preLockParams.vcfType);
        setAPVTSParam("vcfc", preLockParams.vcfCut);
        setAPVTSParam("vcfr", preLockParams.vcfRes);
        setAPVTSParam("vcfe", preLockParams.vcfEnv);

        setAPVTSParam("eqlf", preLockParams.eqLF);
        setAPVTSParam("eqlg", preLockParams.eqLG);
        setAPVTSParam("eqmf", preLockParams.eqMF);
        setAPVTSParam("eqmg", preLockParams.eqMG);
        setAPVTSParam("eqhf", preLockParams.eqHF);
        setAPVTSParam("eqhg", preLockParams.eqHG);

        setAPVTSParam("cthr", preLockParams.cThr);
        setAPVTSParam("crat", preLockParams.cRat);
        setAPVTSParam("catk", preLockParams.cAtk);
        setAPVTSParam("crel", preLockParams.cRel);
        setAPVTSParam("cmg",  preLockParams.cMg);

        setAPVTSParam("ifx",  (float) preLockParams.ifxType);
        setAPVTSParam("ifx1", preLockParams.ifx1);
        setAPVTSParam("ifx2", preLockParams.ifx2);
        setAPVTSParam("ifx3", preLockParams.ifx3);
        setAPVTSParam("ifx4", preLockParams.ifx4);

        setAPVTSParam("satk", preLockParams.smplAtk);
        setAPVTSParam("sdec", preLockParams.smplDec);
        setAPVTSParam("ssus", preLockParams.smplSus);
        setAPVTSParam("srel", preLockParams.smplRel);
    }

    isSyncingPLockUI = false;

    // Audition this specific step with its current sound parameters (only if preview mode is ON)!
    if (auditionOnTouchEnabled)
        proc.triggerStepAudition(pad, sd.velocity > 0.05f ? sd.velocity : 0.85f, sd);

    if (pLockBanner != nullptr)
    {
        pLockBanner->setVisible(true);
        if (pLockBannerTitle != nullptr)
            pLockBannerTitle->setText("P-LOCK EDIT MODE // TRACK " + juce::String(trackIdx + 1)
                                      + ", STEP " + juce::String(stepIdx + 1), juce::dontSendNotification);
        if (pLockSaveBtn != nullptr)
            pLockSaveBtn->setButtonText(sd.hasLocks ? "LOCKED" : "SAVE TO STEP");
    }
    resized();
    if (content)
        content->resized();
    repaint();
}

void PadEditor::exitPLockMode()
{
    if (inPLockMode)
    {
        proc.setPadPLockPreviewActive(pad, false, preLockParams);
        isSyncingPLockUI = true;
        // Restore pre-lock pad base parameters
        setAPVTSParam("tune", preLockParams.tune);
        setAPVTSParam("dec",  preLockParams.decay);
        setAPVTSParam("drv",  preLockParams.drive);
        setAPVTSParam("fx1",  preLockParams.fx1);
        setAPVTSParam("fx2",  preLockParams.fx2);
        setAPVTSParam("fx3",  preLockParams.fx3);
        setAPVTSParam("fx4",  preLockParams.fx4);
        setAPVTSParam("fx5",  preLockParams.fx5);
        setAPVTSParam("snda", preLockParams.sendA);
        setAPVTSParam("sndb", preLockParams.sendB);
        setAPVTSParam("sndc", preLockParams.sendC);
        setAPVTSParam("sndd", preLockParams.sendD);
        setAPVTSParam("lvl",  preLockParams.level);
        setAPVTSParam("pan",  preLockParams.pan);

        setAPVTSParam("vcft", (float) preLockParams.vcfType);
        setAPVTSParam("vcfc", preLockParams.vcfCut);
        setAPVTSParam("vcfr", preLockParams.vcfRes);
        setAPVTSParam("vcfe", preLockParams.vcfEnv);

        setAPVTSParam("eqlf", preLockParams.eqLF);
        setAPVTSParam("eqlg", preLockParams.eqLG);
        setAPVTSParam("eqmf", preLockParams.eqMF);
        setAPVTSParam("eqmg", preLockParams.eqMG);
        setAPVTSParam("eqhf", preLockParams.eqHF);
        setAPVTSParam("eqhg", preLockParams.eqHG);

        setAPVTSParam("cthr", preLockParams.cThr);
        setAPVTSParam("crat", preLockParams.cRat);
        setAPVTSParam("catk", preLockParams.cAtk);
        setAPVTSParam("crel", preLockParams.cRel);
        setAPVTSParam("cmg",  preLockParams.cMg);

        setAPVTSParam("ifx",  (float) preLockParams.ifxType);
        setAPVTSParam("ifx1", preLockParams.ifx1);
        setAPVTSParam("ifx2", preLockParams.ifx2);
        setAPVTSParam("ifx3", preLockParams.ifx3);
        setAPVTSParam("ifx4", preLockParams.ifx4);

        isSyncingPLockUI = false;
    }

    inPLockMode = false;
    pLockTrack = -1;
    pLockStep = -1;
    if (pLockBanner != nullptr)
        pLockBanner->setVisible(false);
    resized();
    if (content)
        content->resized();
    repaint();
}

void PadEditor::visibilityChanged()
{
    if (! isVisible() && inPLockMode)
        exitPLockMode();
}

void PadEditor::saveCurrentParamsAsPLock(bool shouldAudition)
{
    if (pLockTrack < 0 || pLockStep < 0) return;
    auto& seq = proc.getSequencer();
    auto sd = seq.currentPattern().tracks[(size_t) pLockTrack].steps[(size_t) pLockStep];

    sd.active = true;
    if (sd.padOverride < 0 && pad != seq.currentPattern().tracks[(size_t) pLockTrack].defaultPad)
        sd.padOverride = pad;
    sd.pLockPitch  = getAPVTSParam("tune");
    sd.pLockDecay  = getAPVTSParam("dec");
    sd.pLockTone   = getAPVTSParam("fx1");
    sd.pLockP2     = getAPVTSParam("fx2");
    sd.pLockP3     = getAPVTSParam("fx3");
    sd.pLockP4     = getAPVTSParam("fx4");
    sd.pLockP5     = getAPVTSParam("fx5");
    sd.pLockDrive  = getAPVTSParam("drv");
    sd.pLockSendA  = getAPVTSParam("snda");
    sd.pLockSendB  = getAPVTSParam("sndb");
    sd.pLockSendC  = getAPVTSParam("sndc");
    sd.pLockSendD  = getAPVTSParam("sndd");
    sd.pLockLevel  = getAPVTSParam("lvl");
    sd.pLockPan    = getAPVTSParam("pan");

    sd.pLockVcfType = (int) getAPVTSParam("vcft");
    sd.pLockVcfCut  = getAPVTSParam("vcfc");
    sd.pLockVcfRes  = getAPVTSParam("vcfr");
    sd.pLockVcfEnv  = getAPVTSParam("vcfe");

    sd.pLockEqLF = getAPVTSParam("eqlf");
    sd.pLockEqLG = getAPVTSParam("eqlg");
    sd.pLockEqMF = getAPVTSParam("eqmf");
    sd.pLockEqMG = getAPVTSParam("eqmg");
    sd.pLockEqHF = getAPVTSParam("eqhf");
    sd.pLockEqHG = getAPVTSParam("eqhg");

    sd.pLockCThr = getAPVTSParam("cthr");
    sd.pLockCRat = getAPVTSParam("crat");
    sd.pLockCAtk = getAPVTSParam("catk");
    sd.pLockCRel = getAPVTSParam("crel");
    sd.pLockCMg  = getAPVTSParam("cmg");

    sd.pLockIfxType = (int) getAPVTSParam("ifx");
    sd.pLockIfx1    = getAPVTSParam("ifx1");
    sd.pLockIfx2    = getAPVTSParam("ifx2");
    sd.pLockIfx3    = getAPVTSParam("ifx3");
    sd.pLockIfx4    = getAPVTSParam("ifx4");

    uint64_t mask = 0;
    if (std::abs(sd.pLockPitch - preLockParams.tune) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_PITCH;
    if (std::abs(sd.pLockDecay - preLockParams.decay) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_DECAY;
    if (std::abs(sd.pLockDrive - preLockParams.drive) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_DRIVE;
    if (std::abs(sd.pLockTone - preLockParams.fx1) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_TONE;
    if (std::abs(sd.pLockP2 - preLockParams.fx2) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_P2;
    if (std::abs(sd.pLockP3 - preLockParams.fx3) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_P3;
    if (std::abs(sd.pLockP4 - preLockParams.fx4) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_P4;
    if (std::abs(sd.pLockP5 - preLockParams.fx5) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_P5;
    if (std::abs(sd.pLockSendA - preLockParams.sendA) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_SEND_A;
    if (std::abs(sd.pLockSendB - preLockParams.sendB) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_SEND_B;
    if (std::abs(sd.pLockSendC - preLockParams.sendC) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_SEND_C;
    if (std::abs(sd.pLockSendD - preLockParams.sendD) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_SEND_D;
    if (std::abs(sd.pLockLevel - preLockParams.level) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_LEVEL;
    if (std::abs(sd.pLockPan - preLockParams.pan) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_PAN;
    if (sd.pLockVcfType != preLockParams.vcfType) mask |= StepLockFlags::LOCK_FLAG_VCF_TYPE;
    if (std::abs(sd.pLockVcfCut - preLockParams.vcfCut) > 10.f) mask |= StepLockFlags::LOCK_FLAG_VCF_CUT;
    if (std::abs(sd.pLockVcfRes - preLockParams.vcfRes) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_VCF_RES;
    if (std::abs(sd.pLockVcfEnv - preLockParams.vcfEnv) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_VCF_ENV;
    if (std::abs(sd.pLockEqLF - preLockParams.eqLF) > 5.f) mask |= StepLockFlags::LOCK_FLAG_EQ_LF;
    if (std::abs(sd.pLockEqLG - preLockParams.eqLG) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_EQ_LG;
    if (std::abs(sd.pLockEqMF - preLockParams.eqMF) > 10.f) mask |= StepLockFlags::LOCK_FLAG_EQ_MF;
    if (std::abs(sd.pLockEqMG - preLockParams.eqMG) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_EQ_MG;
    if (std::abs(sd.pLockEqHF - preLockParams.eqHF) > 20.f) mask |= StepLockFlags::LOCK_FLAG_EQ_HF;
    if (std::abs(sd.pLockEqHG - preLockParams.eqHG) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_EQ_HG;
    if (std::abs(sd.pLockCThr - preLockParams.cThr) > 0.1f) mask |= StepLockFlags::LOCK_FLAG_COMP_THR;
    if (std::abs(sd.pLockCRat - preLockParams.cRat) > 0.05f) mask |= StepLockFlags::LOCK_FLAG_COMP_RAT;
    if (std::abs(sd.pLockCAtk - preLockParams.cAtk) > 0.1f) mask |= StepLockFlags::LOCK_FLAG_COMP_ATK;
    if (std::abs(sd.pLockCRel - preLockParams.cRel) > 1.0f) mask |= StepLockFlags::LOCK_FLAG_COMP_REL;
    if (std::abs(sd.pLockCMg - preLockParams.cMg) > 0.1f) mask |= StepLockFlags::LOCK_FLAG_COMP_MG;
    if (sd.pLockIfxType != preLockParams.ifxType) mask |= StepLockFlags::LOCK_FLAG_IFX_TYPE;
    if (std::abs(sd.pLockIfx1 - preLockParams.ifx1) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_IFX1;
    if (std::abs(sd.pLockIfx2 - preLockParams.ifx2) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_IFX2;
    if (std::abs(sd.pLockIfx3 - preLockParams.ifx3) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_IFX3;
    if (std::abs(sd.pLockIfx4 - preLockParams.ifx4) > 0.01f) mask |= StepLockFlags::LOCK_FLAG_IFX4;

    const uint64_t paramLockMask = mask;
    if (sd.padOverride >= 0 && sd.padOverride != seq.currentPattern().tracks[(size_t) pLockTrack].defaultPad)
        mask |= StepLockFlags::LOCK_FLAG_PAD_OVERRIDE;
    sd.lockMask = mask;
    sd.hasLocks = (paramLockMask != 0 || sd.ratchet > 1);

    seq.setStepData(pLockTrack, pLockStep, sd);
    seq.bumpPatternVersion();
    hadLocksOnEntry = sd.hasLocks;

    if (shouldAudition && auditionOnTouchEnabled)
        proc.triggerStepAudition(pad, sd.velocity > 0.05f ? sd.velocity : 0.85f, sd);

    if (pLockSaveBtn != nullptr)
        pLockSaveBtn->setButtonText(sd.hasLocks ? "LOCKED" : "SAVE TO STEP");

    if (auto* top = getTopLevelComponent())
        top->repaint();
    else
        repaint();
}

void PadEditor::clearPLocksForStep()
{
    if (pLockTrack < 0 || pLockStep < 0) return;
    auto& seq = proc.getSequencer();
    auto sd = seq.currentPattern().tracks[(size_t) pLockTrack].steps[(size_t) pLockStep];
    sd.clearLocks();
    seq.setStepData(pLockTrack, pLockStep, sd);
    seq.bumpPatternVersion();
    hadLocksOnEntry = false;

    isSyncingPLockUI = true;
    // Restore pad base parameters
    setAPVTSParam("tune", preLockParams.tune);
    setAPVTSParam("dec",  preLockParams.decay);
    setAPVTSParam("drv",  preLockParams.drive);
    setAPVTSParam("fx1",  preLockParams.fx1);
    setAPVTSParam("fx2",  preLockParams.fx2);
    setAPVTSParam("fx3",  preLockParams.fx3);
    setAPVTSParam("fx4",  preLockParams.fx4);
    setAPVTSParam("fx5",  preLockParams.fx5);
    setAPVTSParam("snda", preLockParams.sendA);
    setAPVTSParam("sndb", preLockParams.sendB);
    setAPVTSParam("sndc", preLockParams.sendC);
    setAPVTSParam("sndd", preLockParams.sendD);
    setAPVTSParam("lvl",  preLockParams.level);
    setAPVTSParam("pan",  preLockParams.pan);

    setAPVTSParam("vcft", (float) preLockParams.vcfType);
    setAPVTSParam("vcfc", preLockParams.vcfCut);
    setAPVTSParam("vcfr", preLockParams.vcfRes);
    setAPVTSParam("vcfe", preLockParams.vcfEnv);

    setAPVTSParam("eqlf", preLockParams.eqLF);
    setAPVTSParam("eqlg", preLockParams.eqLG);
    setAPVTSParam("eqmf", preLockParams.eqMF);
    setAPVTSParam("eqmg", preLockParams.eqMG);
    setAPVTSParam("eqhf", preLockParams.eqHF);
    setAPVTSParam("eqhg", preLockParams.eqHG);

    setAPVTSParam("cthr", preLockParams.cThr);
    setAPVTSParam("crat", preLockParams.cRat);
    setAPVTSParam("catk", preLockParams.cAtk);
    setAPVTSParam("crel", preLockParams.cRel);
    setAPVTSParam("cmg",  preLockParams.cMg);

    setAPVTSParam("ifx",  (float) preLockParams.ifxType);
    setAPVTSParam("ifx1", preLockParams.ifx1);
    setAPVTSParam("ifx2", preLockParams.ifx2);
    setAPVTSParam("ifx3", preLockParams.ifx3);
    setAPVTSParam("ifx4", preLockParams.ifx4);

    setAPVTSParam("satk", preLockParams.smplAtk);
    setAPVTSParam("sdec", preLockParams.smplDec);
    setAPVTSParam("ssus", preLockParams.smplSus);
    setAPVTSParam("srel", preLockParams.smplRel);

    isSyncingPLockUI = false;

    if (auditionOnTouchEnabled)
        proc.triggerStepAudition(pad, sd.velocity > 0.05f ? sd.velocity : 0.85f, sd);

    if (pLockSaveBtn != nullptr)
        pLockSaveBtn->setButtonText("SAVE TO STEP");

    if (auto* top = getTopLevelComponent())
        top->repaint();
    else
        repaint();
}

void PadEditor::triggerAuditionForCurrentStep()
{
    auditionOnControlTouch();
}

void PadEditor::auditionOnControlTouch()
{
    if (! auditionOnTouchEnabled)
        return;

    if (inPLockMode && pLockTrack >= 0 && pLockStep >= 0)
    {
        auto& seq = proc.getSequencer();
        const auto& sd = seq.currentPattern().tracks[(size_t) pLockTrack].steps[(size_t) pLockStep];
        proc.triggerStepAudition(pad, sd.velocity > 0.05f ? sd.velocity : 0.85f, sd);
    }
    else
    {
        proc.triggerAudition(pad, 0.9f);
    }
}

// ---------------------------------------------------------------------------
// Unified Category Presets
// ---------------------------------------------------------------------------
void PadEditor::syncModuleSelectionQuiet()
{
    auto st = proc.grid().padState(pad);
    juce::String padModId = st.getProperty("moduleId", "").toString();
    if (padModId.isEmpty())
    {
        static const char* kDefaultModuleForSlot[kNumPads] = {
            // Bank A: Core Electronic & Acoustic (0..15)
            "kick_808", "snare_808", "hat_closed", "hat_open",
            "clap_808", "tom_dual", "tom_dual", "tom_dual",
            "snare_rimshot", "perc_cowbell", "hat_noise", "cymbal_crash",
            "cymbal_ride", "perc_conga", "synth_zap", "kick_rock",
            // Bank B: Heavy / Electro / Industrial Club (16..31)
            "kick_909", "snare_909", "hat_fm", "hat_fm",
            "clap_room", "tom_simmons", "tom_simmons", "tom_simmons",
            "kick_hardstyle", "clap_trash", "snare_rock", "cymbal_china",
            "kick_electro", "synth_noise", "synth_acid", "kick_sub_fm",
            // Bank C: World & Acoustic Percussion (32..47)
            "tom_floor", "perc_rimshot", "perc_agogo", "perc_agogo",
            "perc_conga", "perc_conga", "perc_rimshot", "perc_rimshot",
            "perc_cowbell", "perc_cowbell", "cymbal_ride", "cymbal_china",
            "cymbal_crash", "hat_noise", "snare_rock", "kick_rock",
            // Bank D: Melodic Synths, Acid, Plucks & Cyber FX (48..63)
            "synth_karplus", "synth_karplus", "synth_karplus", "synth_acid",
            "synth_acid", "synth_acid", "synth_zap", "synth_zap",
            "synth_zap", "synth_noise", "synth_noise", "kick_sub_fm",
            "kick_hardstyle", "kick_electro", "clap_room", "cymbal_crash"
        };
        padModId = kDefaultModuleForSlot[juce::jlimit(0, kNumPads - 1, pad)];
        proc.grid().padState(pad).setProperty("moduleId", padModId, nullptr);
    }

    currentModuleId = padModId;
    auto mod = ModulePresetManager::getModuleById(padModId);
    const auto embeddedScript = st.getProperty("script").toString();
    if (mod.id != padModId && embeddedScript.isNotEmpty())
    {
        // Unknown bank modules must not inherit the first factory module's identity.
        mod.id = padModId;
        mod.name = st.getProperty("name", "Custom Lua").toString();
        mod.category = st.getProperty("category", "Custom Lua").toString();
        mod.p1Label = mod.p2Label = mod.p3Label = mod.p4Label = mod.p5Label = {};
    }

    // Sync Category combo
    if (categoryCombo != nullptr)
    {
        bool found = false;
        for (int i = 0; i < categoryCombo->getNumItems(); ++i)
        {
            if (categoryCombo->getItemText(i) == mod.category)
            {
                categoryCombo->setSelectedId(i + 1, juce::dontSendNotification);
                found = true;
                break;
            }
        }
        if (! found)
        {
            const int id = categoryCombo->getNumItems() + 1;
            categoryCombo->addItem(mod.category, id);
            categoryCombo->setSelectedId(id, juce::dontSendNotification);
        }
    }

    // Populate category sounds without loading or overwriting APVTS
    currentCategorySounds = ModulePresetManager::getSoundsForCategory(mod.category);
    if (embeddedScript.isNotEmpty())
    {
        // Keep a portable preset in the pad state itself: no installed module is
        // required to display or reselect a bank's personal DSP algorithm.
        ModulePresetManager::CategorySoundEntry entry;
        entry.moduleId = padModId;
        entry.displayName = st.getProperty("name", "Custom Lua").toString();
        auto& preset = entry.preset;
        preset.name = entry.displayName;
        preset.moduleId = padModId;
        preset.category = mod.category;
        preset.scriptCode = embeddedScript;
        auto param = [this](const char* id, float fallback)
        {
            auto* value = proc.getAPVTS().getRawParameterValue(padParamId(pad, id));
            return value != nullptr ? value->load() : fallback;
        };
        preset.tune = param("tune", 0.f);
        preset.decay = param("dec", 0.5f);
        preset.drive = param("drv", 0.f);
        preset.p1 = param("fx1", 0.5f); preset.p2 = param("fx2", 0.5f);
        preset.p3 = param("fx3", 0.5f); preset.p4 = param("fx4", 0.5f); preset.p5 = param("fx5", 0.5f);
        preset.vcfType = (int) param("vcft", 0.f);
        preset.vcfCut = param("vcfc", 20000.f);
        preset.vcfRes = param("vcfr", 0.707f);
        preset.vcfEnv = param("vcfe", 0.f);
        preset.p1Label = st.getProperty("p1Label").toString();
        preset.p2Label = st.getProperty("p2Label").toString();
        preset.p3Label = st.getProperty("p3Label").toString();
        preset.p4Label = st.getProperty("p4Label").toString();
        preset.p5Label = st.getProperty("p5Label").toString();
        currentCategorySounds.push_back(std::move(entry));
    }
    if (soundPresetCombo != nullptr)
    {
        soundPresetCombo->clear(juce::dontSendNotification);
        int selectedIdx = 1;
        for (int i = 0; i < (int) currentCategorySounds.size(); ++i)
        {
            soundPresetCombo->addItem(currentCategorySounds[(size_t) i].displayName, i + 1);
            if (currentCategorySounds[(size_t) i].moduleId == mod.id)
                selectedIdx = i + 1;
        }
        if (embeddedScript.isNotEmpty()) selectedIdx = (int) currentCategorySounds.size();
        soundPresetCombo->setSelectedId(selectedIdx, juce::dontSendNotification);
    }

    // Update variable knob labels to match module / custom script without overwriting parameter values!
    auto padSt = proc.grid().padState(pad);
    auto sCode = padSt.getProperty("script", "").toString();
    auto sLabels = parseMacroLabelsFromScript(sCode);

    auto getQuietLabel = [](const juce::String& stVal, const juce::String& sVal, const juce::String& mVal, const char* defVal) -> juce::String
    {
        if (stVal.isNotEmpty()) return stVal;
        if (sVal.isNotEmpty()) return sVal;
        if (mVal.isNotEmpty()) return mVal;
        return defVal;
    };

    if (p1Knob) p1Knob->setLabel(getQuietLabel(padSt.getProperty("p1Label", "").toString(), sLabels.p1, mod.p1Label, "P1"));
    if (p2Knob) p2Knob->setLabel(getQuietLabel(padSt.getProperty("p2Label", "").toString(), sLabels.p2, mod.p2Label, "P2"));
    if (p3Knob) p3Knob->setLabel(getQuietLabel(padSt.getProperty("p3Label", "").toString(), sLabels.p3, mod.p3Label, "P3"));
    if (p4Knob) p4Knob->setLabel(getQuietLabel(padSt.getProperty("p4Label", "").toString(), sLabels.p4, mod.p4Label, "P4"));
    if (p5Knob) p5Knob->setLabel(getQuietLabel(padSt.getProperty("p5Label", "").toString(), sLabels.p5, mod.p5Label, "P5"));

    // Sync srcCombo
    int curSrc = 0;
    if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
        curSrc = (int) p->load();
    if (srcCombo)
        srcCombo->setSelectedId((curSrc == SRC_SAMPLE) ? 1 : 2, juce::dontSendNotification);
}

void PadEditor::populateCategorySounds(const juce::String& category)
{
    currentCategorySounds = ModulePresetManager::getSoundsForCategory(category);
    if (soundPresetCombo != nullptr)
    {
        soundPresetCombo->clear(juce::dontSendNotification);
        for (int i = 0; i < (int) currentCategorySounds.size(); ++i)
            soundPresetCombo->addItem(currentCategorySounds[(size_t) i].displayName, i + 1);
        if (! currentCategorySounds.empty())
        {
            soundPresetCombo->setSelectedId(1, juce::dontSendNotification);
            loadCategorySound(0);
        }
    }
}

void PadEditor::loadCategorySound(int soundIndex)
{
    if (soundIndex < 0 || soundIndex >= (int) currentCategorySounds.size())
        return;

    if (inPLockMode)
        exitPLockMode();

    const auto& entry = currentCategorySounds[(size_t) soundIndex];
    currentModuleId = entry.moduleId;
    auto mod = ModulePresetManager::getModuleById(entry.moduleId);
    if (mod.id != entry.moduleId)
        mod.p1Label = mod.p2Label = mod.p3Label = mod.p4Label = mod.p5Label = {};

    // Determine script to use: prefer preset's scriptCode, fallback to mod's scriptCode
    juce::String scriptToUse = entry.preset.scriptCode;
    if (scriptToUse.trim().isEmpty())
        scriptToUse = mod.scriptCode;

    auto scriptLabels = parseMacroLabelsFromScript(scriptToUse);
    auto getLabel = [](const juce::String& pVal, const juce::String& sVal, const juce::String& mVal, const char* defVal) -> juce::String
    {
        if (pVal.isNotEmpty()) return pVal;
        if (sVal.isNotEmpty()) return sVal;
        if (mVal.isNotEmpty()) return mVal;
        return defVal;
    };

    juce::String l1 = getLabel(entry.preset.p1Label, scriptLabels.p1, mod.p1Label, "P1");
    juce::String l2 = getLabel(entry.preset.p2Label, scriptLabels.p2, mod.p2Label, "P2");
    juce::String l3 = getLabel(entry.preset.p3Label, scriptLabels.p3, mod.p3Label, "P3");
    juce::String l4 = getLabel(entry.preset.p4Label, scriptLabels.p4, mod.p4Label, "P4");
    juce::String l5 = getLabel(entry.preset.p5Label, scriptLabels.p5, mod.p5Label, "P5");

    if (p1Knob) p1Knob->setLabel(l1);
    if (p2Knob) p2Knob->setLabel(l2);
    if (p3Knob) p3Knob->setLabel(l3);
    if (p4Knob) p4Knob->setLabel(l4);
    if (p5Knob) p5Knob->setLabel(l5);

    // Save moduleId and labels to pad state
    auto padSt = proc.grid().padState(pad);
    padSt.setProperty("moduleId", entry.moduleId, nullptr);
    padSt.setProperty("category", entry.preset.category.isNotEmpty() ? entry.preset.category : mod.category, nullptr);
    padSt.setProperty("p1Label", l1, nullptr);
    padSt.setProperty("p2Label", l2, nullptr);
    padSt.setProperty("p3Label", l3, nullptr);
    padSt.setProperty("p4Label", l4, nullptr);
    padSt.setProperty("p5Label", l5, nullptr);

    // Push Lua script to pad
    if (! scriptToUse.isEmpty())
    {
        padSt.setProperty("script", scriptToUse, nullptr);
        padSt.setProperty("scriptOn", true, nullptr);
        proc.grid().runtime(pad).scriptOn.store(true);
        proc.lua().setScript(pad, scriptToUse, true);
    }

    // Switch generator source to Lua engine
    setAPVTSParam("src", (float) SRC_LUA);
    if (srcCombo)
        srcCombo->setSelectedId(2, juce::dontSendNotification);
    updateModuleControls(SRC_LUA);

    // Apply sound preset values
    const auto& sp = entry.preset;
    setAPVTSParam("tune", sp.tune);
    setAPVTSParam("dec",  sp.decay);
    setAPVTSParam("drv",  sp.drive);
    setAPVTSParam("fx1",  sp.p1);
    setAPVTSParam("fx2",  sp.p2);
    setAPVTSParam("fx3",  sp.p3);
    setAPVTSParam("fx4",  sp.p4);
    setAPVTSParam("fx5",  sp.p5);
    setAPVTSParam("vcft", (float) sp.vcfType);
    setAPVTSParam("vcfc", sp.vcfCut);
    setAPVTSParam("vcfr", sp.vcfRes);
    setAPVTSParam("vcfe", sp.vcfEnv);

    if (scriptStatusBadge != nullptr)
    {
        const auto err = proc.lua().errorFor(pad);
        scriptStatusBadge->setText(err.isEmpty() ? "LUA: COMPILED OK" : "LUA ERR: " + err, juce::dontSendNotification);
        scriptStatusBadge->setColour(juce::Label::textColourId, err.isEmpty() ? juce::Colour(0xFF70E000) : juce::Colour(0xFFFF5252));
    }

    if (scriptWindow != nullptr)
        scriptWindow->setPad(pad);
}

void PadEditor::showSavePresetDialog()
{
    // If Lua script editor is open, ensure any typed changes are compiled first
    if (scriptWindow != nullptr)
        scriptWindow->compileCurrentScript();

    auto* w = new juce::AlertWindow("Save Sound Preset",
                                    "Save current sound parameters and Lua DSP algorithm:",
                                    juce::AlertWindow::NoIcon);

    juce::String defaultName = "Custom Sound";
    if (soundPresetCombo && soundPresetCombo->getText().isNotEmpty())
    {
        auto curText = soundPresetCombo->getText();
        if (curText.contains(" - "))
            defaultName = curText.fromLastOccurrenceOf(" - ", false, false);
        else
            defaultName = curText;
    }
    w->addTextEditor("name", defaultName, "Preset Name:");

    const auto categories = ModulePresetManager::getModuleCategories();
    w->addComboBox("category", categories, "Category:");
    if (auto* cb = w->getComboBoxComponent("category"))
    {
        int selIdx = 1;
        juce::String curCat = categoryCombo ? categoryCombo->getText() : "Custom";

        auto curMod = ModulePresetManager::getModuleById(currentModuleId);
        auto curScript = proc.grid().padState(pad).getProperty("script", "").toString();
        bool isCustomScript = (curScript.trim().isNotEmpty() && curScript.trim() != curMod.scriptCode.trim());
        if (isCustomScript && (curCat.isEmpty() || curCat == "All Categories"))
            curCat = "Custom";

        for (int i = 0; i < categories.size(); ++i)
        {
            if (categories[i].equalsIgnoreCase(curCat))
            {
                selIdx = i + 1;
                break;
            }
        }
        cb->setSelectedId(selIdx, juce::dontSendNotification);
    }

    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->toFront(true);

    juce::Component::SafePointer<PadEditor> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe, w](int result)
    {
        std::unique_ptr<juce::AlertWindow> deleter(w);
        if (safe == nullptr || result != 1) return;

        auto name = w->getTextEditorContents("name").trim();
        if (name.isEmpty()) return;

        juce::String chosenCat = "Custom";
        if (auto* cb = w->getComboBoxComponent("category"))
            chosenCat = cb->getText().trim();
        if (chosenCat.isEmpty())
            chosenCat = "Custom";

        safe->saveCurrentSoundPreset(name, chosenCat);
    }));
}

void PadEditor::saveCurrentSoundPreset(const juce::String& name, const juce::String& category)
{
    // 1. Current script
    juce::String curScript;
    if (scriptWindow != nullptr)
        curScript = scriptWindow->getCurrentScriptCode();
    if (curScript.trim().isEmpty())
    {
        auto st = proc.grid().padState(pad);
        curScript = st.getProperty("script", "").toString();
    }
    if (curScript.trim().isEmpty())
    {
        auto curMod = ModulePresetManager::getModuleById(currentModuleId);
        curScript = curMod.scriptCode;
    }
    if (curScript.trim().isEmpty())
    {
        int srcType = SRC_KICK;
        if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
            srcType = (int) p->load();
        curScript = ModuleScripts::scriptFor(srcType);
    }
    if (curScript.trim().isEmpty())
    {
        curScript = ModuleScripts::scriptFor(SRC_KICK);
    }

    // 2. Knob labels: prefer current knob label if user-defined, fallback to parsed from script, fallback to "P1".."P5"
    auto parsedLabels = parseMacroLabelsFromScript(curScript);
    auto getFinalLabel = [](ModRingKnob* k, const juce::String& parsed, const char* defVal) -> juce::String
    {
        if (k != nullptr)
        {
            auto l = k->getLabel().trim();
            if (l.isNotEmpty() && l != "P1" && l != "P2" && l != "P3" && l != "P4" && l != "P5")
                return l;
        }
        if (parsed.isNotEmpty()) return parsed;
        return defVal;
    };
    juce::String p1 = getFinalLabel(p1Knob, parsedLabels.p1, "P1");
    juce::String p2 = getFinalLabel(p2Knob, parsedLabels.p2, "P2");
    juce::String p3 = getFinalLabel(p3Knob, parsedLabels.p3, "P3");
    juce::String p4 = getFinalLabel(p4Knob, parsedLabels.p4, "P4");
    juce::String p5 = getFinalLabel(p5Knob, parsedLabels.p5, "P5");

    if (p1Knob) p1Knob->setLabel(p1);
    if (p2Knob) p2Knob->setLabel(p2);
    if (p3Knob) p3Knob->setLabel(p3);
    if (p4Knob) p4Knob->setLabel(p4);
    if (p5Knob) p5Knob->setLabel(p5);

    // 3. Generate unique user module ID
    juce::String safeName = juce::File::createLegalFileName(name).toLowerCase().replace(" ", "_");
    if (safeName.isEmpty()) safeName = "custom_sound";
    juce::String userModId = "user_" + safeName;

    // 4. Save User Module (Tier 1)
    ModuleInfo mod;
    mod.id = userModId;
    mod.name = name;
    mod.category = category.isNotEmpty() ? category : "Custom";
    mod.author = "User";
    mod.description = "User Preset: " + name;
    mod.p1Label = p1;
    mod.p2Label = p2;
    mod.p3Label = p3;
    mod.p4Label = p4;
    mod.p5Label = p5;
    mod.defTune = getAPVTSParam("tune");
    mod.defDecay = getAPVTSParam("dec");
    mod.defDrive = getAPVTSParam("drv");
    mod.defP1 = getAPVTSParam("fx1");
    mod.defP2 = getAPVTSParam("fx2");
    mod.defP3 = getAPVTSParam("fx3");
    mod.defP4 = getAPVTSParam("fx4");
    mod.defP5 = getAPVTSParam("fx5");
    mod.defVcfType = (int) getAPVTSParam("vcft");
    mod.defVcfCut = getAPVTSParam("vcfc");
    mod.defVcfRes = getAPVTSParam("vcfr");
    mod.scriptCode = curScript;
    ModulePresetManager::saveUserModule(mod);

    // 5. Save Sound Preset (Tier 2)
    SoundPreset sp;
    sp.name = name;
    sp.moduleId = userModId;
    sp.category = category.isNotEmpty() ? category : "Custom";
    sp.tune = mod.defTune;
    sp.decay = mod.defDecay;
    sp.drive = mod.defDrive;
    sp.p1 = mod.defP1;
    sp.p2 = mod.defP2;
    sp.p3 = mod.defP3;
    sp.p4 = mod.defP4;
    sp.p5 = mod.defP5;
    sp.vcfType = mod.defVcfType;
    sp.vcfCut = mod.defVcfCut;
    sp.vcfRes = mod.defVcfRes;
    sp.vcfEnv = getAPVTSParam("vcfe");
    sp.p1Label = p1;
    sp.p2Label = p2;
    sp.p3Label = p3;
    sp.p4Label = p4;
    sp.p5Label = p5;
    sp.scriptCode = curScript;
    sp.isUserPreset = true;
    ModulePresetManager::saveSoundPreset(sp);

    // 6. Update current pad state
    auto st = proc.grid().padState(pad);
    st.setProperty("moduleId", userModId, nullptr);
    st.setProperty("script", curScript, nullptr);
    st.setProperty("scriptOn", true, nullptr);
    st.setProperty("p1Label", p1, nullptr);
    st.setProperty("p2Label", p2, nullptr);
    st.setProperty("p3Label", p3, nullptr);
    st.setProperty("p4Label", p4, nullptr);
    st.setProperty("p5Label", p5, nullptr);
    proc.grid().runtime(pad).scriptOn.store(true);
    proc.lua().setScript(pad, curScript, true);
    currentModuleId = userModId;

    // 7. Update category combo and populate sounds
    if (categoryCombo != nullptr)
    {
        for (int i = 0; i < categoryCombo->getNumItems(); ++i)
        {
            if (categoryCombo->getItemText(i).equalsIgnoreCase(category))
            {
                categoryCombo->setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    currentCategorySounds = ModulePresetManager::getSoundsForCategory(category);
    if (soundPresetCombo != nullptr)
    {
        soundPresetCombo->clear(juce::dontSendNotification);
        int selIdx = 1;
        for (int i = 0; i < (int) currentCategorySounds.size(); ++i)
        {
            soundPresetCombo->addItem(currentCategorySounds[(size_t) i].displayName, i + 1);
            if (currentCategorySounds[(size_t) i].moduleId == userModId ||
                currentCategorySounds[(size_t) i].preset.name == name)
            {
                selIdx = i + 1;
            }
        }
        soundPresetCombo->setSelectedId(selIdx, juce::dontSendNotification);
    }

    if (scriptStatusBadge != nullptr)
    {
        scriptStatusBadge->setText("SAVED AS PRESET", juce::dontSendNotification);
        scriptStatusBadge->setColour(juce::Label::textColourId, juce::Colour(0xFF70E000));
    }
}

// ---------------------------------------------------------------------------
// Detached Lua Editor
// ---------------------------------------------------------------------------
void PadEditor::openScriptEditorWindow()
{
    if (scriptWindow != nullptr)
    {
        scriptWindow->setPad(pad);
        scriptWindow->toFront(true);
        scriptWindow->setVisible(true);
        return;
    }
    scriptWindow = std::make_unique<LuaScriptEditorWindow>(proc, pad);
    scriptWindow->onScriptChanged = [this]
    {
        auto padSt = proc.grid().padState(pad);
        auto sCode = padSt.getProperty("script", "").toString();
        auto parsed = parseMacroLabelsFromScript(sCode);

        auto getL = [](const juce::String& sVal, const juce::String& pVal, const char* defVal) -> juce::String
        {
            if (sVal.isNotEmpty()) return sVal;
            if (pVal.isNotEmpty()) return pVal;
            return juce::String(defVal);
        };

        if (p1Knob) p1Knob->setLabel(getL(padSt.getProperty("p1Label", "").toString(), parsed.p1, "P1"));
        if (p2Knob) p2Knob->setLabel(getL(padSt.getProperty("p2Label", "").toString(), parsed.p2, "P2"));
        if (p3Knob) p3Knob->setLabel(getL(padSt.getProperty("p3Label", "").toString(), parsed.p3, "P3"));
        if (p4Knob) p4Knob->setLabel(getL(padSt.getProperty("p4Label", "").toString(), parsed.p4, "P4"));
        if (p5Knob) p5Knob->setLabel(getL(padSt.getProperty("p5Label", "").toString(), parsed.p5, "P5"));

        if (scriptStatusBadge != nullptr)
        {
            const auto err = proc.lua().errorFor(pad);
            scriptStatusBadge->setText(err.isEmpty() ? "LUA: COMPILED OK" : "LUA ERR: " + err, juce::dontSendNotification);
            scriptStatusBadge->setColour(juce::Label::textColourId, err.isEmpty() ? juce::Colour(0xFF70E000) : juce::Colour(0xFFFF5252));
        }
    };
    scriptWindow->onSavePreset = [this]
    {
        showSavePresetDialog();
    };
    scriptWindow->setVisible(true);
}

// ---------------------------------------------------------------------------
ModRingKnob* PadEditor::makeKnob(const char* base, const char* labelText)
{
    auto* k = new ModRingKnob(padParamId(pad, base), labelText, svcs);
    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(
            proc.getAPVTS().getParameter(k->dest)))
        sliderAtt.push_back(std::make_unique<juce::SliderParameterAttachment>(*par, *k, nullptr));

    k->onDragStart = [this]
    {
        auditionOnControlTouch();
    };
    k->onDragEnd = [this]
    {
    };
    k->onValueChange = [this]
    {
        if (inPLockMode && ! isSyncingPLockUI)
            saveCurrentParamsAsPLock(false);
    };
    content->addAndMakeVisible(k);
    return k;
}

ModRingKnob* PadEditor::makeChip(const char* destId, const char* labelText)
{
    auto* k = new ModRingKnob(destId, labelText, svcs, true);
    k->setTooltip("Per-voice modulation target - drop a source badge here");
    content->addAndMakeVisible(k);
    return k;
}

juce::ComboBox* PadEditor::makeCombo(const char* base, const juce::StringArray& items)
{
    auto* cb = new juce::ComboBox();
    ui::styleCombo(*cb);
    for (int i = 0; i < items.size(); ++i)
        cb->addItem(items[i], i + 1);
    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(
            proc.getAPVTS().getParameter(padParamId(pad, base))))
        comboAtt.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*par, *cb, nullptr));
    content->addAndMakeVisible(cb);
    return cb;
}

juce::Label* PadEditor::makeCaption(const char* text)
{
    auto l = ui::makeLabel(text, 10.5f, ui::accentHot());
    l->setFont(uiFont(10.5f, true));
    captions.push_back(l.get());
    content->addAndMakeVisible(l.get());
    return l.release();
}

void PadEditor::updateModuleControls(int srcType)
{
    const bool isSampler = (srcType == SRC_SAMPLE);

    if (moduleCaption != nullptr)
        moduleCaption->setText(isSampler ? "SAMPLE PLAYBACK & WAVEFORM ENGINE" : "MODULE SYNTHESIS ENGINE (LUA DSP)",
                               juce::dontSendNotification);

    if (isSampler)
    {
        if (waveformDisplay) waveformDisplay->setVisible(true);
        if (fileBtn) fileBtn->setVisible(true);
        if (sampleLabel) sampleLabel->setVisible(true);

        if (categoryCombo) categoryCombo->setVisible(false);
        if (soundPresetCombo) soundPresetCombo->setVisible(false);
        if (savePresetBtn) savePresetBtn->setVisible(false);
        if (editScriptBtn) editScriptBtn->setVisible(false);
        if (aiPromptBtn) aiPromptBtn->setVisible(false);
        if (scriptStatusBadge) scriptStatusBadge->setVisible(false);

        for (auto* k : smplKnobsRow1) if (k) k->setVisible(true);
        for (auto* k : smplKnobsRow2) if (k) k->setVisible(true);
        if (decKnob) decKnob->setVisible(false);
        if (p1Knob) p1Knob->setVisible(false);
        if (p2Knob) p2Knob->setVisible(false);
        if (p3Knob) p3Knob->setVisible(false);
        if (p4Knob) p4Knob->setVisible(false);
        if (p5Knob) p5Knob->setVisible(false);
    }
    else
    {
        if (waveformDisplay) waveformDisplay->setVisible(false);
        if (fileBtn) fileBtn->setVisible(false);
        if (sampleLabel) sampleLabel->setVisible(false);

        if (categoryCombo) categoryCombo->setVisible(true);
        if (soundPresetCombo) soundPresetCombo->setVisible(true);
        if (savePresetBtn) savePresetBtn->setVisible(true);
        if (editScriptBtn) editScriptBtn->setVisible(true);
        if (aiPromptBtn) aiPromptBtn->setVisible(true);
        if (scriptStatusBadge) scriptStatusBadge->setVisible(true);

        for (auto* k : smplKnobsRow1) if (k) k->setVisible(false);
        for (auto* k : synthKnobsRow1) if (k) k->setVisible(true);
        for (auto* k : synthKnobsRow2) if (k) k->setVisible(true);
    }

    if (content)
        content->resized();
}

void PadEditor::updateSampleLabel()
{
    if (sampleLabel == nullptr) return;
    const auto p = proc.grid().padState(pad).getProperty("samplePath", "").toString();
    if (p.isEmpty())
        sampleLabel->setText("No sample loaded", juce::dontSendNotification);
    else
        sampleLabel->setText(juce::File(p).getFileName(), juce::dontSendNotification);
}

void PadEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser>("Load sample for pad " + juce::String(pad + 1),
                                                   juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", this);
    juce::Component::SafePointer<PadEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile())
        {
            safe->proc.grid().setSampleFile(safe->pad, f);
            safe->updateSampleLabel();
            if (safe->waveformDisplay != nullptr)
                safe->waveformDisplay->refresh();
        }
    });
}

void PadEditor::updateIfxLabels(int ifxType)
{
    if (ifxKnobs.size() < 4) return;
    switch (ifxType)
    {
        case 1: // Flanger
            ifxKnobs[0]->setLabel("RATE");
            ifxKnobs[1]->setLabel("DEPTH");
            ifxKnobs[2]->setLabel("FEEDBK");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 2: // Chorus
            ifxKnobs[0]->setLabel("RATE");
            ifxKnobs[1]->setLabel("DEPTH");
            ifxKnobs[2]->setLabel("WIDTH");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 3: // Crusher
            ifxKnobs[0]->setLabel("DOWNSMPL");
            ifxKnobs[1]->setLabel("BITS");
            ifxKnobs[2]->setLabel("DRIVE");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 4: // Phaser
            ifxKnobs[0]->setLabel("RATE");
            ifxKnobs[1]->setLabel("DEPTH");
            ifxKnobs[2]->setLabel("RESON");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 5: // Overdrive
            ifxKnobs[0]->setLabel("DRIVE");
            ifxKnobs[1]->setLabel("TONE");
            ifxKnobs[2]->setLabel("DYNA");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 6: // Fuzz
            ifxKnobs[0]->setLabel("GAIN");
            ifxKnobs[1]->setLabel("BITE");
            ifxKnobs[2]->setLabel("LOW CUT");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 7: // Tape Echo
            ifxKnobs[0]->setLabel("TIME");
            ifxKnobs[1]->setLabel("FEEDBK");
            ifxKnobs[2]->setLabel("DAMP");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 8: // Plate Reverb
            ifxKnobs[0]->setLabel("SIZE");
            ifxKnobs[1]->setLabel("DECAY");
            ifxKnobs[2]->setLabel("DAMP");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 9: // Pitch Shift
            ifxKnobs[0]->setLabel("PITCH");
            ifxKnobs[1]->setLabel("FINE");
            ifxKnobs[2]->setLabel("FEEDBK");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 10: // Formant
            ifxKnobs[0]->setLabel("VOWEL");
            ifxKnobs[1]->setLabel("RESON");
            ifxKnobs[2]->setLabel("BRIGHT");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 11: // Ring Mod
            ifxKnobs[0]->setLabel("FREQ");
            ifxKnobs[1]->setLabel("SHAPE");
            ifxKnobs[2]->setLabel("DRIVE");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 12: // Hall Reverb
            ifxKnobs[0]->setLabel("SIZE");
            ifxKnobs[1]->setLabel("DAMP");
            ifxKnobs[2]->setLabel("PRE-DLY");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 13: // Chamber Reverb
            ifxKnobs[0]->setLabel("DECAY");
            ifxKnobs[1]->setLabel("SIZE");
            ifxKnobs[2]->setLabel("DAMP");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 14: // Spring Reverb
            ifxKnobs[0]->setLabel("TENSION");
            ifxKnobs[1]->setLabel("BOING");
            ifxKnobs[2]->setLabel("TONE");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 15: // Gated Reverb
            ifxKnobs[0]->setLabel("GATE");
            ifxKnobs[1]->setLabel("DENSITY");
            ifxKnobs[2]->setLabel("TONE");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 16: // Ping-Pong Delay
            ifxKnobs[0]->setLabel("TIME");
            ifxKnobs[1]->setLabel("FEEDBK");
            ifxKnobs[2]->setLabel("DAMP");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 17: // Dub Delay
            ifxKnobs[0]->setLabel("TIME");
            ifxKnobs[1]->setLabel("FEEDBK");
            ifxKnobs[2]->setLabel("CUTOFF");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 18: // Tube Saturator
            ifxKnobs[0]->setLabel("DRIVE");
            ifxKnobs[1]->setLabel("BIAS");
            ifxKnobs[2]->setLabel("WARMTH");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 19: // Wavefolder
            ifxKnobs[0]->setLabel("FOLDS");
            ifxKnobs[1]->setLabel("SYMMETRY");
            ifxKnobs[2]->setLabel("SMOOTH");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 20: // Frequency Shifter
            ifxKnobs[0]->setLabel("SHIFT");
            ifxKnobs[1]->setLabel("FEEDBK");
            ifxKnobs[2]->setLabel("DIR");
            ifxKnobs[3]->setLabel("MIX");
            break;
        case 21: // Stereo Detuner
            ifxKnobs[0]->setLabel("DETUNE");
            ifxKnobs[1]->setLabel("SPREAD");
            ifxKnobs[2]->setLabel("FEEDBK");
            ifxKnobs[3]->setLabel("MIX");
            break;
        default: // Off
            ifxKnobs[0]->setLabel("P1");
            ifxKnobs[1]->setLabel("P2");
            ifxKnobs[2]->setLabel("P3");
            ifxKnobs[3]->setLabel("MIX");
            break;
    }
}

void PadEditor::resized()
{
    auto r = getLocalBounds();
    auto bannerBounds = r.removeFromBottom(34);
    if (pLockBanner != nullptr)
    {
        pLockBanner->setBounds(bannerBounds.reduced(6, 2));
        auto pb = pLockBanner->getLocalBounds().reduced(4);
        if (pLockBannerTitle) pLockBannerTitle->setBounds(pb.removeFromLeft(280));
        if (pLockExitBtn)     pLockExitBtn->setBounds(pb.removeFromRight(60));
        pb.removeFromRight(8);
        if (pLockClearBtn)    pLockClearBtn->setBounds(pb.removeFromRight(96));
        pb.removeFromRight(8);
        if (pLockSaveBtn)     pLockSaveBtn->setBounds(pb.removeFromRight(106));

        pLockBanner->setVisible(inPLockMode);
        if (inPLockMode)
            pLockBanner->toFront(false);
    }

    if (viewport != nullptr)
        viewport->setBounds(r);
    if (content != nullptr)
        content->setSize(r.getWidth(), 836);
}

void PadEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
}

void PadEditor::Content::resized()
{
    const int w = getWidth();
    int curY = 6;

    // Header row
    auto hr = juce::Rectangle<int>(8, curY, w - 16, 28);
    if (owner.prevPadBtn) owner.prevPadBtn->setBounds(hr.removeFromLeft(26));
    hr.removeFromLeft(4);
    if (owner.nextPadBtn) owner.nextPadBtn->setBounds(hr.removeFromLeft(26));
    hr.removeFromLeft(8);
    if (owner.padLabel)   owner.padLabel->setBounds(hr.removeFromLeft(46));
    hr.removeFromLeft(6);
    if (owner.playBtn)    owner.playBtn->setBounds(hr.removeFromLeft(48));
    hr.removeFromLeft(6);
    if (owner.previewToggleBtn) owner.previewToggleBtn->setBounds(hr.removeFromLeft(92));
    hr.removeFromLeft(8);
    if (owner.nameEdit)   owner.nameEdit->setBounds(hr.removeFromLeft(110));
    hr.removeFromLeft(6);
    if (owner.colourBtn)  owner.colourBtn->setBounds(hr.removeFromLeft(24));
    hr.removeFromLeft(12);
    if (owner.srcCombo)   owner.srcCombo->setBounds(hr.removeFromLeft(140));

    curY += 34;

    // Card 1: Module Sound Engine + Presets + Knobs (h: 222)
    const int c1Y = curY;
    if (! owner.captions.empty() && owner.captions[0])
        owner.captions[0]->setBounds(12, c1Y + 4, 260, 16);

    // Presets Row inside Card 1: Category + Sound Preset + Save + Script Editor + AI Prompt + Lua Badge
    auto pRow = juce::Rectangle<int>(12, c1Y + 22, w - 24, 24);
    if (owner.categoryCombo)     owner.categoryCombo->setBounds(pRow.removeFromLeft(110));
    pRow.removeFromLeft(6);
    if (owner.soundPresetCombo)  owner.soundPresetCombo->setBounds(pRow.removeFromLeft(150));
    pRow.removeFromLeft(6);
    if (owner.savePresetBtn)     owner.savePresetBtn->setBounds(pRow.removeFromLeft(96));
    pRow.removeFromLeft(10);
    if (owner.editScriptBtn)     owner.editScriptBtn->setBounds(pRow.removeFromLeft(100));
    pRow.removeFromLeft(6);
    if (owner.aiPromptBtn)       owner.aiPromptBtn->setBounds(pRow.removeFromLeft(86));
    pRow.removeFromLeft(10);
    if (owner.scriptStatusBadge) owner.scriptStatusBadge->setBounds(pRow);

    int s = 0;
    if (owner.srcCombo) s = owner.srcCombo->getSelectedItemIndex();
    const bool isSampler = (s == SRC_SAMPLE);

    if (isSampler)
    {
        auto sRow = juce::Rectangle<int>(12, c1Y + 22, w - 24, 22);
        if (owner.fileBtn)     owner.fileBtn->setBounds(sRow.removeFromLeft(90));
        sRow.removeFromLeft(8);
        if (owner.sampleLabel) owner.sampleLabel->setBounds(sRow);

        if (owner.waveformDisplay)
            owner.waveformDisplay->setBounds(12, c1Y + 48, w - 24, 76);

        // Sampler ADSR + Playback knobs
        const int numKnobs = 8;
        const int smplKnobW = (w - 24) / numKnobs;
        std::array<ModRingKnob*, 8> allSmplKnobs = {
            owner.satkKnob, owner.sdecKnob, owner.ssusKnob, owner.srelKnob,
            owner.tuneKnob, owner.lvlKnob, owner.panKnob, owner.drvKnob
        };
        for (int i = 0; i < numKnobs; ++i)
        {
            if (allSmplKnobs[i])
                allSmplKnobs[i]->setBounds(12 + i * smplKnobW, c1Y + 132, smplKnobW - 4, 76);
        }
    }
    else
    {
        // 5 Synth Knobs in Row 1 (Pitch, Decay, Drive, Level, Pan)
        const int knobW = (w - 24) / 5;
        for (size_t i = 0; i < owner.synthKnobsRow1.size(); ++i)
        {
            if (owner.synthKnobsRow1[i])
                owner.synthKnobsRow1[i]->setBounds(12 + (int) i * knobW, c1Y + 54, knobW - 6, 76);
        }

        // 5 Module Variable & Modulation Knobs in Row 2 (P1, P2, P3, P4, Mod Amt)
        for (size_t i = 0; i < owner.synthKnobsRow2.size(); ++i)
        {
            if (owner.synthKnobsRow2[i])
                owner.synthKnobsRow2[i]->setBounds(12 + (int) i * knobW, c1Y + 138, knobW - 6, 76);
        }
    }

    curY = c1Y + 230;

    // =======================================================================
    // Card 2: Unified Processing Strip (VCF + EQ + Compressor SIDE-BY-SIDE!)
    // =======================================================================
    const int c2Y = curY;
    if (owner.captions.size() > 1 && owner.captions[1])
        owner.captions[1]->setBounds(12, c2Y + 4, w - 24, 16);

    const int totalStripW = w - 24;
    const int vcfW = (int) (totalStripW * 0.28f);
    const int eqW  = (int) (totalStripW * 0.44f);
    const int compW = totalStripW - vcfW - eqW - 16;

    // --- Section 1: Resonant VCF (Left Column) ---
    const int vcfX = 12;
    if (owner.vcfTypeCombo)
        owner.vcfTypeCombo->setBounds(vcfX, c2Y + 22, vcfW, 22);

    if (owner.vcfGraph)
        owner.vcfGraph->setBounds(vcfX, c2Y + 46, vcfW, 68);

    const int vcfKnobW = vcfW / 3;
    for (size_t i = 0; i < owner.vcfKnobs.size(); ++i)
        if (owner.vcfKnobs[i])
            owner.vcfKnobs[i]->setBounds(vcfX + (int) i * vcfKnobW, c2Y + 116, vcfKnobW - 4, 62);

    // --- Section 2: 3-Band Parametric EQ (Center Column) ---
    const int eqX = vcfX + vcfW + 8;
    if (owner.eqGraph)
        owner.eqGraph->setBounds(eqX, c2Y + 22, eqW, 92);

    const int eqKnobW = eqW / 6;
    for (size_t i = 0; i < owner.eqKnobs.size(); ++i)
        if (owner.eqKnobs[i])
            owner.eqKnobs[i]->setBounds(eqX + (int) i * eqKnobW, c2Y + 116, eqKnobW - 4, 62);

    // --- Section 3: Dynamics Compressor (Right Column) ---
    const int compX = eqX + eqW + 8;
    if (owner.compGraph)
        owner.compGraph->setBounds(compX, c2Y + 22, compW, 92);

    const int compKnobW = compW / (int) owner.dynKnobs.size();
    for (size_t i = 0; i < owner.dynKnobs.size(); ++i)
        if (owner.dynKnobs[i])
            owner.dynKnobs[i]->setBounds(compX + (int) i * compKnobW, c2Y + 116, compKnobW - 4, 62);

    curY = c2Y + 194;

    // =======================================================================
    // Card 3: Aux Sends & Insert Multi-FX (h: 96)
    // =======================================================================
    const int c3Y = curY;
    if (owner.captions.size() > 2 && owner.captions[2])
        owner.captions[2]->setBounds(12, c3Y + 4, 380, 16);

    if (owner.ifxCombo)
        owner.ifxCombo->setBounds(14, c3Y + 36, 120, 24);

    for (size_t i = 0; i < owner.ifxKnobs.size(); ++i)
        if (owner.ifxKnobs[i])
            owner.ifxKnobs[i]->setBounds(144 + (int) i * 66, c3Y + 20, 62, 68);

    for (size_t i = 0; i < owner.sendKnobs.size(); ++i)
        if (owner.sendKnobs[i])
            owner.sendKnobs[i]->setBounds(w - 4 * 66 - 14 + (int) i * 66, c3Y + 20, 62, 68);

    curY = c3Y + 104;

    // =======================================================================
    // Card 4: Routing & MIDI (h: 62)
    // =======================================================================
    const int c4Y = curY;
    if (owner.captions.size() > 3 && owner.captions[3])
        owner.captions[3]->setBounds(12, c4Y + 4, 200, 16);

    const int rW = (w - 28) / 5;
    if (owner.chokeCombo) owner.chokeCombo->setBounds(14 + 0 * rW, c4Y + 26, rW - 6, 24);
    if (owner.busCombo)   owner.busCombo->setBounds(14 + 1 * rW, c4Y + 26, rW - 6, 24);
    if (owner.modeCombo)  owner.modeCombo->setBounds(14 + 2 * rW, c4Y + 26, rW - 6, 24);
    if (owner.chanCombo)  owner.chanCombo->setBounds(14 + 3 * rW, c4Y + 26, rW - 6, 24);
    if (owner.noteCombo)  owner.noteCombo->setBounds(14 + 4 * rW, c4Y + 26, rW - 6, 24);

    for (size_t i = 0; i < owner.routeLabels.size(); ++i)
        if (owner.routeLabels[i])
            owner.routeLabels[i]->setBounds(14 + (int) i * rW, c4Y + 12, rW - 6, 14);

    curY = c4Y + 70;

    // =======================================================================
    // Card 5: Pad Local Modulations (h: 186)
    // =======================================================================
    const int c5Y = curY;
    if (owner.captions.size() > 4 && owner.captions[4])
        owner.captions[4]->setBounds(12, c5Y + 4, 200, 16);

    if (owner.connList)
        owner.connList->setBounds(14, c5Y + 22, w - 28, 154);
}

} // namespace f64
