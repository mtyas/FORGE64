#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/LuaScriptEditorWindow.h"
#include <iostream>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    auto proc = std::make_unique<f64::Forge64Processor>();
    proc->setPlayConfigDetails(0, 2, 44100.0, 512);
    proc->prepareToPlay(44100.0, 512);

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(proc->createEditor());
    auto* f64Ed = dynamic_cast<f64::Forge64Editor*>(editor.get());
    f64Ed->setSize(proc->lastUIWidth, proc->lastUIHeight);

    auto capturePage = [&](f64::Forge64Editor::ActivePage page, const juce::String& filename)
    {
        f64Ed->setPage(page);
        for (auto* child : f64Ed->getChildren())
            child->setAlpha(1.0f);
        f64Ed->repaint();
        auto img = f64Ed->createComponentSnapshot(f64Ed->getLocalBounds(), true, 1.0f);
        juce::File outFile(juce::File::getCurrentWorkingDirectory().getChildFile("docs/images/" + filename));
        outFile.getParentDirectory().createDirectory();
        if (outFile.existsAsFile())
            outFile.deleteFile();
        juce::FileOutputStream fos(outFile);
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
        std::cout << "Saved: docs/images/" << filename << " (" << img.getWidth() << "x" << img.getHeight() << ")" << std::endl;
    };

    f64Ed->setBank(0);
    capturePage(f64::Forge64Editor::Page_Grid, "forge64_grid.png");
    f64Ed->setBank(1);
    capturePage(f64::Forge64Editor::Page_Grid, "forge64_grid_bank_b.png");
    f64Ed->setBank(0);
    capturePage(f64::Forge64Editor::Page_PadEdit, "forge64_pad_edit.png");
    capturePage(f64::Forge64Editor::Page_Sequencer, "forge64_sequencer.png");
    capturePage(f64::Forge64Editor::Page_MixerFX, "forge64_mixer_fx.png");
    capturePage(f64::Forge64Editor::Page_Performance, "forge64_performance.png");

    {
        auto luaWin = std::make_unique<f64::LuaScriptEditorWindow>(*proc, 0);
        luaWin->setSize(880, 620);
        for (auto* child : luaWin->getChildren())
            child->setAlpha(1.0f);
        luaWin->repaint();
        auto img = luaWin->createComponentSnapshot(luaWin->getLocalBounds(), true, 1.0f);
        juce::File outFile(juce::File::getCurrentWorkingDirectory().getChildFile("docs/images/forge64_lua_editor.png"));
        outFile.getParentDirectory().createDirectory();
        if (outFile.existsAsFile())
            outFile.deleteFile();
        juce::FileOutputStream fos(outFile);
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
        std::cout << "Saved: docs/images/forge64_lua_editor.png (" << img.getWidth() << "x" << img.getHeight() << ")" << std::endl;
    }

    return 0;
}
