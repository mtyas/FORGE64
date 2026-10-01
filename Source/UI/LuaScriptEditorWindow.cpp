#include "LuaScriptEditorWindow.h"
#include "UICommon.h"
#include "../Scripting/ModuleScripts.h"
#include "../Scripting/AIPromptHelper.h"
#include "../Presets/ModulePresetManager.h"

namespace f64 {

LuaScriptEditorWindow::LuaScriptEditorWindow(Forge64Processor& processor, int padIndex)
    : juce::DocumentWindow("FORGE64 // LUA DSP SCRIPT EDITOR",
                          juce::Colour(0xFF140E0C),
                          juce::DocumentWindow::closeButton),
      proc(processor), pad(padIndex)
{
    setUsingNativeTitleBar(false);
    content = std::make_unique<EditorContent>(proc, pad, *this);
    setContentNonOwned(content.get(), true);
    setResizable(true, true);
    setResizeLimits(700, 520, 1920, 1200);
    centreWithSize(920, 700);
    setVisible(true);
}

void LuaScriptEditorWindow::setPad(int newPad)
{
    pad = newPad;
    if (content != nullptr)
        content->setPad(newPad);
}

void LuaScriptEditorWindow::closeButtonPressed()
{
    setVisible(false);
}

void LuaScriptEditorWindow::compileCurrentScript()
{
    if (content != nullptr)
        content->compileAndApply();
}

juce::String LuaScriptEditorWindow::getCurrentScriptCode() const
{
    return content != nullptr ? content->getCode() : juce::String();
}

// ---------------------------------------------------------------------------
// EditorContent Implementation
// ---------------------------------------------------------------------------
LuaScriptEditorWindow::EditorContent::EditorContent(Forge64Processor& p, int padIdx, LuaScriptEditorWindow& owner)
    : proc(p), pad(padIdx), ownerWindow(owner)
{
    titleLabel = ui::makeLabel("", 16.f, ui::accentHot());
    titleLabel->setFont(uiFont(16.f, true));
    addAndMakeVisible(titleLabel.get());

    statusLabel = ui::makeLabel("READY", 13.f, ui::accent());
    addAndMakeVisible(statusLabel.get());

    compileBtn = std::make_unique<juce::TextButton>("COMPILE & APPLY");
    ui::styleButton(*compileBtn);
    compileBtn->setColour(juce::TextButton::buttonColourId, ui::accent().darker(0.3f));
    compileBtn->onClick = [this] { compileAndApply(); };
    addAndMakeVisible(compileBtn.get());

    savePresetBtn = std::make_unique<juce::TextButton>("SAVE PRESET");
    ui::styleButton(*savePresetBtn);
    savePresetBtn->setColour(juce::TextButton::buttonColourId, ui::accent().darker(0.15f));
    savePresetBtn->setTooltip("Save current compiled script and pad settings as a sound preset");
    savePresetBtn->onClick = [this]
    {
        compileAndApply();
        if (ownerWindow.onSavePreset)
            ownerWindow.onSavePreset();
    };
    addAndMakeVisible(savePresetBtn.get());

    revertBtn = std::make_unique<juce::TextButton>("REVERT");
    ui::styleButton(*revertBtn);
    revertBtn->onClick = [this] { revertToFactory(); };
    addAndMakeVisible(revertBtn.get());

    clearBtn = std::make_unique<juce::TextButton>("CLEAR");
    ui::styleButton(*clearBtn);
    clearBtn->onClick = [this] { clearScript(); };
    addAndMakeVisible(clearBtn.get());

    exportBtn = std::make_unique<juce::TextButton>("EXPORT");
    ui::styleButton(*exportBtn);
    exportBtn->onClick = [this] { exportScript(); };
    addAndMakeVisible(exportBtn.get());

    importBtn = std::make_unique<juce::TextButton>("IMPORT");
    ui::styleButton(*importBtn);
    importBtn->onClick = [this] { importScript(); };
    addAndMakeVisible(importBtn.get());

    aiPromptBtn = std::make_unique<juce::TextButton>("AI PROMPT");
    ui::styleButton(*aiPromptBtn);
    aiPromptBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF6E3600));
    aiPromptBtn->setTooltip("Copy prompt & API guide to feed into Claude, ChatGPT, or Antigravity to generate new drum DSP scripts");
    aiPromptBtn->onClick = [this] { AIPromptHelper::showAIPromptDialog(this); };
    addAndMakeVisible(aiPromptBtn.get());

    codeEditor = std::make_unique<juce::TextEditor>();
    codeEditor->setMultiLine(true);
    codeEditor->setReturnKeyStartsNewLine(true);
    codeEditor->setTabKeyUsedAsCharacter(true);
    codeEditor->setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.5f, juce::Font::plain)));
    codeEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0C0807));
    codeEditor->setColour(juce::TextEditor::textColourId, juce::Colour(0xFFFFE5C0));
    codeEditor->setColour(juce::TextEditor::outlineColourId, ui::line());
    codeEditor->setColour(juce::TextEditor::focusedOutlineColourId, ui::accent());
    addAndMakeVisible(codeEditor.get());

    consoleOutput = std::make_unique<juce::TextEditor>();
    consoleOutput->setMultiLine(true);
    consoleOutput->setReadOnly(true);
    consoleOutput->setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    consoleOutput->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF080504));
    consoleOutput->setColour(juce::TextEditor::textColourId, ui::dim());
    consoleOutput->setColour(juce::TextEditor::outlineColourId, ui::line());
    addAndMakeVisible(consoleOutput.get());

    setPad(pad);
}

void LuaScriptEditorWindow::EditorContent::setPad(int newPad)
{
    pad = newPad;
    const int bankIdx = pad / 16;
    const char bankChar = (char) ('A' + bankIdx);
    const juce::String padCoord = juce::String::charToString(bankChar) + juce::String::formatted("%02d", (pad % 16) + 1);

    titleLabel->setText("PAD " + padCoord + " // LUA DSP", juce::dontSendNotification);

    auto st = proc.grid().padState(pad);
    juce::String curCode = st.getProperty("script", "").toString();
    if (curCode.trim().isEmpty())
    {
        int srcType = SRC_KICK;
        if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
            srcType = (int) p->load();
        curCode = ModuleScripts::scriptFor(srcType);
    }

    codeEditor->setText(curCode, juce::dontSendNotification);

    auto labels = parseMacroLabelsFromScript(curCode);
    if (labels.hasAny())
    {
        if (labels.p1.isNotEmpty() && ! st.hasProperty("p1Label")) st.setProperty("p1Label", labels.p1, nullptr);
        if (labels.p2.isNotEmpty() && ! st.hasProperty("p2Label")) st.setProperty("p2Label", labels.p2, nullptr);
        if (labels.p3.isNotEmpty() && ! st.hasProperty("p3Label")) st.setProperty("p3Label", labels.p3, nullptr);
        if (labels.p4.isNotEmpty() && ! st.hasProperty("p4Label")) st.setProperty("p4Label", labels.p4, nullptr);
        if (labels.p5.isNotEmpty() && ! st.hasProperty("p5Label")) st.setProperty("p5Label", labels.p5, nullptr);
    }

    const auto err = proc.lua().errorFor(pad);
    if (err.isEmpty())
    {
        statusLabel->setText("READY / ACTIVE", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, juce::Colour(0xFF70E000));
        consoleOutput->setText("Lua DSP engine running cleanly.\nAPI: outL(i,v), outR(i,v), inL(i), inR(i), param(\"tune\"|\"decay\"|\"drive\"|\"p1\"..\"p5\"), rnd(), vel, age, trig, n, sr",
                              juce::dontSendNotification);
    }
    else
    {
        statusLabel->setText("COMPILE/RUNTIME ERROR", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, ui::accentHot());
        consoleOutput->setText("ERROR: " + err, juce::dontSendNotification);
    }
}

juce::String LuaScriptEditorWindow::EditorContent::getCode() const
{
    return codeEditor != nullptr ? codeEditor->getText() : juce::String();
}

void LuaScriptEditorWindow::EditorContent::compileAndApply()
{
    const auto code = codeEditor->getText();
    proc.grid().padState(pad).setProperty("script", code, nullptr);
    proc.grid().padState(pad).setProperty("scriptOn", true, nullptr);
    proc.grid().runtime(pad).scriptOn.store(true);

    auto labels = parseMacroLabelsFromScript(code);
    if (labels.hasAny())
    {
        auto st = proc.grid().padState(pad);
        if (labels.p1.isNotEmpty()) st.setProperty("p1Label", labels.p1, nullptr);
        if (labels.p2.isNotEmpty()) st.setProperty("p2Label", labels.p2, nullptr);
        if (labels.p3.isNotEmpty()) st.setProperty("p3Label", labels.p3, nullptr);
        if (labels.p4.isNotEmpty()) st.setProperty("p4Label", labels.p4, nullptr);
        if (labels.p5.isNotEmpty()) st.setProperty("p5Label", labels.p5, nullptr);
    }

    proc.lua().setScript(pad, code, true);

    const auto err = proc.lua().errorFor(pad);
    if (err.isEmpty())
    {
        statusLabel->setText("COMPILED OK", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, juce::Colour(0xFF70E000));
        consoleOutput->setText("Compilation successful! Script applied to Pad audio engine.\nTimestamp: " + juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                              juce::dontSendNotification);
    }
    else
    {
        statusLabel->setText("COMPILE ERROR", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, ui::accentHot());
        consoleOutput->setText("COMPILATION FAILED:\n" + err, juce::dontSendNotification);
    }

    if (ownerWindow.onScriptChanged)
        ownerWindow.onScriptChanged();
}

void LuaScriptEditorWindow::EditorContent::revertToFactory()
{
    int srcType = SRC_KICK;
    if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
        srcType = (int) p->load();

    const auto code = ModuleScripts::scriptFor(srcType);
    codeEditor->setText(code, juce::dontSendNotification);
    compileAndApply();
}

void LuaScriptEditorWindow::EditorContent::clearScript()
{
    codeEditor->setText(
R"(-- Empty Template
function process()
    for i = 0, n - 1 do
        outL(i, 0.0)
        outR(i, 0.0)
    end
end
)", juce::dontSendNotification);
    compileAndApply();
}

void LuaScriptEditorWindow::EditorContent::exportScript()
{
    chooser = std::make_unique<juce::FileChooser>("Export Lua DSP Script...",
                                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                  "*.lua;*.f64lua");
    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result != juce::File{})
        {
            result.replaceWithText(codeEditor->getText());
            consoleOutput->setText("Exported script to: " + result.getFullPathName(), juce::dontSendNotification);
        }
    });
}

void LuaScriptEditorWindow::EditorContent::importScript()
{
    chooser = std::make_unique<juce::FileChooser>("Import Lua DSP Script...",
                                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                  "*.lua;*.f64lua");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.existsAsFile())
        {
            codeEditor->setText(result.loadFileAsString(), juce::dontSendNotification);
            compileAndApply();
        }
    });
}

void LuaScriptEditorWindow::EditorContent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF140E0C));
    ui::drawForgedPlate(g, getLocalBounds().toFloat().reduced(2.f), 6.f, false);
}

void LuaScriptEditorWindow::EditorContent::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    titleLabel->setBounds(12, 8, 200, 24);
    statusLabel->setBounds(14, 34, 200, 20);

    int bx = w - 10;
    auto placeBtn = [&](std::unique_ptr<juce::TextButton>& b, int bw)
    {
        bx -= (bw + 6);
        if (b) b->setBounds(bx, 10, bw, 28);
    };

    placeBtn(compileBtn, 116);
    placeBtn(savePresetBtn, 94);
    placeBtn(revertBtn, 68);
    placeBtn(exportBtn, 60);
    placeBtn(importBtn, 60);
    placeBtn(clearBtn, 50);
    placeBtn(aiPromptBtn, 84);

    const int consoleH = 92;
    codeEditor->setBounds(10, 60, w - 20, h - 60 - consoleH - 12);
    consoleOutput->setBounds(10, h - consoleH - 6, w - 20, consoleH);
}

} // namespace f64
