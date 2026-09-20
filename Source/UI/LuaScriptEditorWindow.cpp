#include "LuaScriptEditorWindow.h"
#include "UICommon.h"
#include "../Scripting/ModuleScripts.h"
#include "../Scripting/AIPromptHelper.h"

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

    revertBtn = std::make_unique<juce::TextButton>("REVERT SCRIPT");
    ui::styleButton(*revertBtn);
    revertBtn->onClick = [this] { revertToFactory(); };
    addAndMakeVisible(revertBtn.get());

    clearBtn = std::make_unique<juce::TextButton>("CLEAR");
    ui::styleButton(*clearBtn);
    clearBtn->onClick = [this] { clearScript(); };
    addAndMakeVisible(clearBtn.get());

    exportBtn = std::make_unique<juce::TextButton>("EXPORT...");
    ui::styleButton(*exportBtn);
    exportBtn->onClick = [this] { exportScript(); };
    addAndMakeVisible(exportBtn.get());

    importBtn = std::make_unique<juce::TextButton>("IMPORT...");
    ui::styleButton(*importBtn);
    importBtn->onClick = [this] { importScript(); };
    addAndMakeVisible(importBtn.get());

    aiPromptBtn = std::make_unique<juce::TextButton>("AI AGENT PROMPT");
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

    titleLabel->setText("PAD " + padCoord + " // LUA DSP ALGORITHM", juce::dontSendNotification);

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

    const auto err = proc.lua().errorFor(pad);
    if (err.isEmpty())
    {
        statusLabel->setText("READY / ACTIVE", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, juce::Colour(0xFF70E000));
        consoleOutput->setText("Lua DSP engine running cleanly.\nAPI: outL(i,v), outR(i,v), inL(i), inR(i), param(\"tune\"|\"decay\"|\"drive\"|\"fx1\"..\"fx4\"), rnd(), vel, age, trig, n, sr",
                              juce::dontSendNotification);
    }
    else
    {
        statusLabel->setText("COMPILE/RUNTIME ERROR", juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, ui::accentHot());
        consoleOutput->setText("ERROR: " + err, juce::dontSendNotification);
    }
}

void LuaScriptEditorWindow::EditorContent::compileAndApply()
{
    const auto code = codeEditor->getText();
    proc.grid().padState(pad).setProperty("script", code, nullptr);
    proc.grid().padState(pad).setProperty("scriptOn", true, nullptr);
    proc.grid().runtime(pad).scriptOn.store(true);

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

    titleLabel->setBounds(12, 8, 320, 24);
    statusLabel->setBounds(14, 34, 260, 20);

    int bx = w - 10;
    auto placeBtn = [&](std::unique_ptr<juce::TextButton>& b, int bw)
    {
        bx -= (bw + 6);
        if (b) b->setBounds(bx, 10, bw, 28);
    };

    placeBtn(compileBtn, 126);
    placeBtn(revertBtn, 102);
    placeBtn(exportBtn, 74);
    placeBtn(importBtn, 74);
    placeBtn(clearBtn, 58);
    placeBtn(aiPromptBtn, 130);

    const int consoleH = 92;
    codeEditor->setBounds(10, 60, w - 20, h - 60 - consoleH - 12);
    consoleOutput->setBounds(10, h - consoleH - 6, w - 20, consoleH);
}

} // namespace f64
