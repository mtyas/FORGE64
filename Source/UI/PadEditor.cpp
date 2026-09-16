#include "PadEditor.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

static const int kPadColours[8] = { 0xFF3A6EA5, 0xFF3E8E5A, 0xFFC2703A, 0xFF8E5BC7,
                                    0xFFC74B5B, 0xFF4BB8C7, 0xFFB8A63A, 0xFF7A7F8A };

static const char* kDefaultScript =
    "-- FORGE64 pad script: process() runs once per audio block\n"
    "-- while the pad sounds (pre-FX insert on the pad bus).\n"
    "--\n"
    "-- API:  inL(i) inR(i)        read input, i in [0, n)\n"
    "--       outL(i,v) outR(i,v)  write output\n"
    "--       n, sr                block size, sample rate\n"
    "--       vel, age             trigger velocity, seconds since hit\n"
    "--       param(\"level\"|\"tune\"|\"drive\"|\"fx1\"...)\n"
    "\n"
    "function process()\n"
    "  for i = 0, n - 1 do\n"
    "    outL(i, inL(i))\n"
    "    outR(i, inR(i))\n"
    "  end\n"
    "end\n";

// ---------------------------------------------------------------------------
class PadEditor::Content : public juce::Component
{
public:
    explicit Content(PadEditor& o) : owner(o) {}
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(ui::panel().darker(0.2f)); }

private:
    PadEditor& owner;
};

class PadEditor::ScriptDocListener : public juce::CodeDocument::Listener
{
public:
    explicit ScriptDocListener(PadEditor& o) : owner(o) {}
    void codeDocumentTextInserted(const juce::String&, int, int) override { owner.startTimer(600); }
    void codeDocumentTextDeleted(int, int) override { owner.startTimer(600); }

private:
    PadEditor& owner;
};

// ---------------------------------------------------------------------------
PadEditor::PadEditor(Forge64Processor& p, ModRingKnob::Services& s, int globalPad,
                     std::function<void()> onBack)
    : proc(p), svcs(s), pad(globalPad), backCb(std::move(onBack))
{
    content = new Content(*this);
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(content, true);
    addAndMakeVisible(viewport.get());

    const auto st = proc.grid().padState(pad);

    // ---- header -------------------------------------------------------
    backBtn = new juce::TextButton("<");
    backBtn->setTooltip("Back to pad grid");
    ui::styleButton(*backBtn);
    backBtn->onClick = [this] { if (backCb) backCb(); };
    content->addAndMakeVisible(backBtn);

    padLabel = ui::makeLabel("PAD " + juce::String(pad + 1), 16.f, ui::accent()).release();
    padLabel->setFont(uiFont(16.f, true));
    content->addAndMakeVisible(padLabel);

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
                         juce::Colour((juce::uint32) (int) st.getProperty("colour", 0xFF3A6EA5)));
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

    srcBtn = new juce::TextButton();
    srcBtn->setClickingTogglesState(true);
    ui::styleButton(*srcBtn);
    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(
            proc.getAPVTS().getParameter(padParamId(pad, "src"))))
        srcAtt = std::make_unique<juce::ButtonParameterAttachment>(*par, *srcBtn, nullptr);
    srcBtn->onStateChange = [this]
    {
        srcBtn->setButtonText(srcBtn->getToggleState() ? "SRC: LUA" : "SRC: SAMPLE");
    };
    srcBtn->setButtonText(srcBtn->getToggleState() ? "SRC: LUA" : "SRC: SAMPLE");
    srcBtn->setTooltip("Sound source: sample playback or Lua-generated");
    content->addAndMakeVisible(srcBtn);

    fileBtn = new juce::TextButton("Load Sample");
    ui::styleButton(*fileBtn);
    fileBtn->onClick = [this] { chooseSample(); };
    content->addAndMakeVisible(fileBtn);

    sampleLabel = ui::makeLabel("", 11.f, ui::dim()).release();
    content->addAndMakeVisible(sampleLabel);

    // ---- sections ------------------------------------------------------
    makeCaption("AMPLITUDE      voice-mod drop targets:");
    ampKnobs = { makeKnob("lvl", "LEVEL"), makeKnob("pan", "PAN"),
                 makeKnob("tune", "TUNE"), makeKnob("dec", "DECAY") };
    chips = { makeChip(kDestVoiceAmp, "AMP"), makeChip(kDestVoicePitch, "PITCH"),
              makeChip(kDestVoicePan, "PAN") };

    makeCaption("3-BAND PARAMETRIC EQ");
    eqKnobs = { makeKnob("eqlf", "LO HZ"), makeKnob("eqlg", "LO DB"),
                makeKnob("eqmf", "MID HZ"), makeKnob("eqmg", "MID DB"),
                makeKnob("eqhf", "HI HZ"), makeKnob("eqhg", "HI DB") };

    makeCaption("COMPRESSOR / SATURATION");
    dynKnobs = { makeKnob("cthr", "THR"), makeKnob("crat", "RATIO"),
                 makeKnob("catk", "ATK"), makeKnob("crel", "REL"),
                 makeKnob("drv", "DRIVE") };

    makeCaption("INSERT MULTI-FX / AUX SENDS");
    fxCombo = makeCombo("fx", { "Off", "Flanger", "Chorus", "Crusher", "Phaser" });
    fxKnobs = { makeKnob("fx1", "FX P1"), makeKnob("fx2", "FX P2"),
                makeKnob("fx3", "FX P3"), makeKnob("fx4", "FX P4"),
                makeKnob("snda", "SEND A"), makeKnob("sndb", "SEND B") };

    makeCaption("ROUTING");
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

    // ---- Lua script -----------------------------------------------------
    makeCaption("LUA SCRIPT  (sandboxed, per-pad)");

    scriptOnBtn = new juce::ToggleButton("ON");
    ui::styleToggle(*scriptOnBtn);
    scriptOnBtn->setToggleState(bool(st.getProperty("scriptOn", false)), juce::dontSendNotification);
    scriptOnBtn->onClick = [this]
    {
        proc.grid().padState(pad).setProperty("scriptOn", scriptOnBtn->getToggleState(), nullptr);
        recompileScript();
    };
    content->addAndMakeVisible(scriptOnBtn);

    scriptErrLabel = ui::makeLabel("", 10.f, ui::dim()).release();
    content->addAndMakeVisible(scriptErrLabel);

    scriptDoc = std::make_unique<juce::CodeDocument>();
    auto scriptText = st.getProperty("script", "").toString();
    if (scriptText.trim().isEmpty())
        scriptText = kDefaultScript;
    scriptDoc->replaceAllContent(scriptText);

    scriptEditor = new juce::CodeEditorComponent(*scriptDoc, nullptr);
    scriptEditor->setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xFF101216));
    scriptEditor->setColour(juce::CodeEditorComponent::defaultTextColourId, ui::txt());
    scriptEditor->setColour(juce::CodeEditorComponent::highlightColourId, ui::accent().withAlpha(0.25f));
    scriptEditor->setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xFF14161B));
    scriptEditor->setColour(juce::CodeEditorComponent::lineNumberTextId, ui::dim());
    scriptEditor->setFont(uiFont(12.f));
    content->addAndMakeVisible(scriptEditor);

    docListener = std::make_unique<ScriptDocListener>(*this);
    scriptDoc->addListener(docListener.get());

    // ---- local modulations ----------------------------------------------
    makeCaption("PAD MODULATIONS");
    connList = new ConnectionList(proc.mods(), proc.getKit().getChildWithName("MODMAT"),
                                  "p" + juce::String(pad) + "_");
    content->addAndMakeVisible(connList);

    updateSampleLabel();
    recompileScript();
}

PadEditor::~PadEditor()
{
    stopTimer();
    if (scriptDoc != nullptr && docListener != nullptr)
        scriptDoc->removeListener(docListener.get());
}

ModRingKnob* PadEditor::makeKnob(const char* base, const char* labelText)
{
    auto* k = new ModRingKnob(padParamId(pad, base), labelText, svcs);
    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(
            proc.getAPVTS().getParameter(k->dest)))
    {
        const auto r = par->getNormalisableRange();
        k->setRange(juce::NormalisableRange<double>((double) r.start, (double) r.end,
                    (double) (r.interval > 0.f ? r.interval : 0.001f), (double) r.getSkew()));
        sliderAtt.push_back(std::make_unique<juce::SliderParameterAttachment>(*par, *k, nullptr));
    }
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
    auto l = ui::makeLabel(text, 10.5f, ui::dim());
    l->setFont(uiFont(10.5f, true));
    captions.push_back(l.get());
    content->addAndMakeVisible(l.get());
    return l.release();
}

void PadEditor::recompileScript()
{
    proc.lua().setScript(pad, scriptDoc->getAllText(), scriptOnBtn->getToggleState());
    const auto e = proc.lua().errorFor(pad);
    scriptErrLabel->setText(e.isEmpty() ? "ready" : e, juce::dontSendNotification);
    scriptErrLabel->setColour(juce::Label::textColourId,
                              e.isEmpty() ? ui::dim().darker(0.1f) : juce::Colour(0xFFFF6B6B));
}

void PadEditor::updateSampleLabel()
{
    const auto path = proc.grid().padState(pad).getProperty("sample", "").toString();
    sampleLabel->setText(path.isEmpty() ? "<no sample>" : juce::File(path).getFileName(),
                         juce::dontSendNotification);
}

void PadEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Load sample for PAD " + juce::String(pad + 1), juce::File(),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", this);

    juce::Component::SafePointer<PadEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
        {
            safe->proc.grid().setSampleFile(safe->pad, f);
            safe->updateSampleLabel();
        }
    });
}

void PadEditor::timerCallback()
{
    stopTimer();
    proc.grid().padState(pad).setProperty("script", scriptDoc->getAllText(), nullptr);
    recompileScript();
}

void PadEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
}

void PadEditor::resized()
{
    viewport->setBounds(getLocalBounds());
    if (content != nullptr)
        content->setSize(viewport->getMaximumVisibleWidth(), 936);
}

// ---------------------------------------------------------------------------
void PadEditor::Content::resized()
{
    auto& ed = owner;
    const int w = getWidth();

    // header
    int x = 8;
    ed.backBtn->setBounds(x, 8, 34, 28);      x += 40;
    ed.padLabel->setBounds(x, 8, 74, 28);     x += 78;
    ed.nameEdit->setBounds(x, 8, 150, 28);    x += 156;
    ed.colourBtn->setBounds(x, 8, 30, 28);    x += 36;
    ed.srcBtn->setBounds(x, 8, 96, 28);       x += 102;
    ed.fileBtn->setBounds(x, 8, 100, 28);     x += 106;
    ed.sampleLabel->setBounds(x, 8, juce::jmax(40, w - x - 8), 28);

    auto cap = [&](size_t i, int y) { ed.captions[i]->setBounds(10, y, w - 20, 16); };
    auto knobRow = [&](std::vector<ModRingKnob*>& knobs, int y, int startX)
    {
        int kx = startX;
        for (auto* k : knobs)
        {
            k->setBounds(kx, y, 70, 84);
            kx += 76;
        }
    };

    cap(0, 44);
    knobRow(ed.ampKnobs, 62, 10);
    {
        int cx = 10 + 4 * 76 + 14;
        for (auto* c : ed.chips)
        {
            c->setBounds(cx, 84, 60, 42);
            cx += 66;
        }
    }

    cap(1, 152);
    knobRow(ed.eqKnobs, 170, 10);

    cap(2, 260);
    knobRow(ed.dynKnobs, 278, 10);

    cap(3, 368);
    knobRow(ed.fxKnobs, 386, 130);
    ed.fxCombo->setBounds(10, 415, 112, 26);

    cap(4, 478);
    {
        int rx = 10;
        const int widths[5] = { 96, 92, 118, 84, 110 };
        juce::ComboBox* combos[5] = { ed.chokeCombo, ed.busCombo, ed.modeCombo,
                                      ed.chanCombo, ed.noteCombo };
        for (int i = 0; i < 5; ++i)
        {
            ed.routeLabels[(size_t) i]->setBounds(rx, 496, widths[i], 14);
            combos[i]->setBounds(rx, 512, widths[i], 26);
            rx += widths[i] + 8;
        }
    }

    cap(5, 550);
    ed.scriptOnBtn->setBounds(w - 176, 546, 64, 24);
    ed.scriptErrLabel->setBounds(210, 550, juce::jmax(60, w - 400), 16);
    ed.scriptEditor->setBounds(10, 572, w - 20, 170);

    cap(6, 750);
    ed.connList->setBounds(10, 768, w - 20, 156);
}

} // namespace f64
