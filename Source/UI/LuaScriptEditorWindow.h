#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include "../PluginProcessor.h"

namespace f64 {

class LuaScriptEditorWindow : public juce::DocumentWindow
{
public:
    LuaScriptEditorWindow(Forge64Processor& processor, int padIndex);
    ~LuaScriptEditorWindow() override = default;

    void setPad(int newPad);
    int  getPad() const { return pad; }
    void closeButtonPressed() override;

    std::function<void()> onScriptChanged;

private:
    class EditorContent : public juce::Component
    {
    public:
        EditorContent(Forge64Processor& proc, int pad, LuaScriptEditorWindow& owner);
        ~EditorContent() override = default;

        void setPad(int newPad);
        void resized() override;
        void paint(juce::Graphics& g) override;

        void compileAndApply();
        void revertToFactory();
        void clearScript();
        void exportScript();
        void importScript();

    private:
        Forge64Processor& proc;
        int pad;
        LuaScriptEditorWindow& ownerWindow;

        std::unique_ptr<juce::Label> titleLabel;
        std::unique_ptr<juce::Label> statusLabel;
        std::unique_ptr<juce::TextButton> compileBtn;
        std::unique_ptr<juce::TextButton> revertBtn;
        std::unique_ptr<juce::TextButton> clearBtn;
        std::unique_ptr<juce::TextButton> exportBtn;
        std::unique_ptr<juce::TextButton> importBtn;
        std::unique_ptr<juce::TextButton> aiPromptBtn;

        std::unique_ptr<juce::TextEditor> codeEditor;
        std::unique_ptr<juce::TextEditor> consoleOutput;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    Forge64Processor& proc;
    int pad;
    std::unique_ptr<EditorContent> content;
};

} // namespace f64
