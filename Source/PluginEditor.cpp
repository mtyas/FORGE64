#include "PluginEditor.h"
#include "UI/UICommon.h"

namespace f64 {

Forge64Editor::Forge64Editor(Forge64Processor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setResizable(true, false);
    setResizeLimits(1000, 620, 2400, 1500);
    setSize(1180, 720);

    logo = ui::makeLabel("FORGE64", 21.f, ui::accent());
    logo->setFont(uiFont(21.f, true));
    addAndMakeVisible(logo.get());

    tagline = ui::makeLabel("modular drum engine", 9.5f, ui::dim());
    addAndMakeVisible(tagline.get());

    static const char* bankNames[kNumBanks] = { "A", "B", "C", "D" };
    for (int i = 0; i < kNumBanks; ++i)
    {
        auto b = std::make_unique<juce::TextButton>(bankNames[i]);
        ui::styleButton(*b);
        b->setClickingTogglesState(true);
        b->setTooltip("Show bank " + juce::String(bankNames[i]) + " (pads "
                      + juce::String(i * kPadsPerBank + 1) + "-"
                      + juce::String((i + 1) * kPadsPerBank) + ")");
        b->onClick = [this, i] { setBank(i); };
        addAndMakeVisible(b.get());
        bankBtns[(size_t) i] = std::move(b);
    }

    masterKnob = std::make_unique<ModRingKnob>("master", "MASTER", *this);
    addAndMakeVisible(masterKnob.get());
    if (auto* mp = dynamic_cast<juce::RangedAudioParameter*>(
            processor.getAPVTS().getParameter("master")))
        masterAtt = std::make_unique<juce::SliderParameterAttachment>(*mp, *masterKnob, nullptr);

    kitBtn = std::make_unique<juce::TextButton>("KIT");
    bankMenuBtn = std::make_unique<juce::TextButton>("BANK");
    padMenuBtn = std::make_unique<juce::TextButton>("PAD");
    kitBtn->onClick = [this] { showKitMenu(); };
    bankMenuBtn->onClick = [this] { showBankMenu(); };
    padMenuBtn->onClick = [this] { showPadMenu(); };
    for (auto* b : { kitBtn.get(), bankMenuBtn.get(), padMenuBtn.get() })
    {
        ui::styleButton(*b);
        addAndMakeVisible(b);
    }
    kitBtn->setTooltip("Global kit presets (.kit)");
    bankMenuBtn->setTooltip("Bank presets (.bnk) - 16 pads");
    padMenuBtn->setTooltip("Pad presets (.pad) - single pad");

    grid = std::make_unique<PadGridView>(processor, *this);
    addAndMakeVisible(grid.get());

    modPanel = std::make_unique<ModPanel>(processor.mods(),
                                          processor.getKit().getChildWithName("MODSRC"),
                                          processor.getKit().getChildWithName("MODMAT"));
    addAndMakeVisible(modPanel.get());

    setBank(0);
    startTimerHz(30);
}

Forge64Editor::~Forge64Editor()
{
    stopTimer();
    // Destroy knob-owning components while knobList is still alive (they
    // unregister themselves in their destructors).
    padEdit.reset();
    grid.reset();
    modPanel.reset();
    masterAtt.reset();
    masterKnob.reset();
}

void Forge64Editor::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
    g.setColour(ui::line());
    g.drawHorizontalLine(54, 0.f, (float) getWidth());
}

void Forge64Editor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    logo->setBounds(12, 6, 170, 30);
    tagline->setBounds(14, 34, 170, 14);

    for (int i = 0; i < kNumBanks; ++i)
        bankBtns[(size_t) i]->setBounds(200 + i * 46, 12, 42, 30);

    kitBtn->setBounds(w - 360, 12, 88, 30);
    bankMenuBtn->setBounds(w - 266, 12, 84, 30);
    padMenuBtn->setBounds(w - 176, 12, 76, 30);
    masterKnob->setBounds(w - 92, 0, 86, 56);

    modPanel->setBounds(w - 350, 54, 350, h - 54);
    layoutCenter();
}

juce::Rectangle<int> Forge64Editor::centerBounds() const
{
    return getLocalBounds().withTrimmedTop(54).withTrimmedRight(350);
}

void Forge64Editor::layoutCenter()
{
    const auto r = centerBounds();
    if (grid)
        grid->setBounds(r);
    if (padEdit)
        padEdit->setBounds(r);
}

void Forge64Editor::timerCallback()
{
    if (grid)
        grid->tick();

    const int mb = processor.uiBank().load();
    if (mb != currentBank)
        setBank(mb);

    if (processor.mods().uiActive())
        for (auto* k : knobList)
            k->repaint();
}

void Forge64Editor::setBank(int b)
{
    currentBank = juce::limit(b, 0, kNumBanks - 1);
    processor.uiBank().store(currentBank);
    if (grid)
        grid->setBank(currentBank);
    for (int i = 0; i < kNumBanks; ++i)
        if (bankBtns[(size_t) i])
            bankBtns[(size_t) i]->setToggleState(i == currentBank, juce::dontSendNotification);
}

void Forge64Editor::padClicked(int globalPad)
{
    selectedPad = globalPad;
    zoomedPad = globalPad;
    rebuildPadEditor();
}

void Forge64Editor::rebuildPadEditor()
{
    padEdit = std::make_unique<PadEditor>(processor, *this, zoomedPad,
                                          [this] { showGrid(); });
    addAndMakeVisible(padEdit.get());
    if (grid)
        grid->setVisible(false);
    padEdit->setBounds(centerBounds());
    padEdit->setAlpha(0.f);
    animator.fadeIn(padEdit.get(), 140);
}

void Forge64Editor::showGrid()
{
    zoomedPad = -1;
    padEdit.reset();
    if (grid)
    {
        grid->setVisible(true);
        grid->refreshPads();
    }
    layoutCenter();
}

void Forge64Editor::connectFromDrag(int slot, const juce::String& dest)
{
    processor.mods().addConnection(slot, dest);
}

void Forge64Editor::unregisterKnob(ModRingKnob* k)
{
    knobList.erase(std::remove(knobList.begin(), knobList.end(), k), knobList.end());
}

void Forge64Editor::afterPresetOp(bool ok)
{
    if (! ok)
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "FORGE64", processor.presets().lastError());
    if (grid)
        grid->refreshPads();
    if (zoomedPad >= 0)
        rebuildPadEditor();
}

// ---------------------------------------------------------------------------
// Preset menus
// ---------------------------------------------------------------------------
void Forge64Editor::showKitMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Load Kit...");
    menu.addItem(2, lastKit == juce::File() ? "Save Kit As..."
                                            : "Save Kit (" + lastKit.getFileName() + ")");
    menu.addItem(3, "Save Kit As...");

    juce::Component::SafePointer<Forge64Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(kitBtn.get()),
                       [safe](int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->doLoadKit();
        else if (result == 2)
        {
            if (safe->lastKit != juce::File())
                safe->afterPresetOp(safe->processor.presets().saveKit(safe->lastKit));
            else
                safe->doSaveKitAs();
        }
        else if (result == 3)
            safe->doSaveKitAs();
    });
}

void Forge64Editor::showBankMenu()
{
    static const char* letters[kNumBanks] = { "A", "B", "C", "D" };
    juce::PopupMenu menu;
    menu.addItem(1, juce::String("Load Bank into ") + letters[currentBank] + "...");
    menu.addItem(2, juce::String("Save Bank ") + letters[currentBank] + " As...");

    juce::Component::SafePointer<Forge64Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(bankMenuBtn.get()),
                       [safe](int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1) safe->doLoadBank();
        else if (result == 2) safe->doSaveBankAs();
    });
}

void Forge64Editor::showPadMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Load Pad into PAD " + juce::String(activePad() + 1) + "...");
    menu.addItem(2, "Save PAD " + juce::String(activePad() + 1) + " As...");

    juce::Component::SafePointer<Forge64Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(padMenuBtn.get()),
                       [safe](int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1) safe->doLoadPad();
        else if (result == 2) safe->doSavePadAs();
    });
}

void Forge64Editor::doLoadKit()
{
    chooser = std::make_unique<juce::FileChooser>("Load Kit", juce::File(), "*.kit", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f == juce::File())
            return;
        safe->lastKit = f;
        safe->afterPresetOp(safe->processor.presets().loadKit(f));
    });
}

void Forge64Editor::doSaveKitAs()
{
    const auto start = lastKit != juce::File()
        ? lastKit
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
              .getChildFile("forge64.kit");
    chooser = std::make_unique<juce::FileChooser>("Save Kit", start, "*.kit", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        auto f = fc.getResult();
        if (f == juce::File())
            return;
        if (f.getFileExtension().isEmpty())
            f = f.withFileExtension(".kit");
        safe->lastKit = f;
        safe->afterPresetOp(safe->processor.presets().saveKit(f));
    });
}

void Forge64Editor::doLoadBank()
{
    chooser = std::make_unique<juce::FileChooser>("Load Bank", juce::File(), "*.bnk", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    const int bank = currentBank;
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe, bank](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
            safe->afterPresetOp(safe->processor.presets().loadBank(f, bank));
    });
}

void Forge64Editor::doSaveBankAs()
{
    chooser = std::make_unique<juce::FileChooser>("Save Bank", juce::File(), "*.bnk", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    const int bank = currentBank;
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe, bank](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        auto f = fc.getResult();
        if (f == juce::File())
            return;
        if (f.getFileExtension().isEmpty())
            f = f.withFileExtension(".bnk");
        safe->afterPresetOp(safe->processor.presets().saveBank(f, bank));
    });
}

void Forge64Editor::doLoadPad()
{
    chooser = std::make_unique<juce::FileChooser>("Load Pad", juce::File(), "*.pad", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    const int pad = activePad();
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe, pad](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
            safe->afterPresetOp(safe->processor.presets().loadPad(f, pad));
    });
}

void Forge64Editor::doSavePadAs()
{
    chooser = std::make_unique<juce::FileChooser>("Save Pad", juce::File(), "*.pad", this);
    juce::Component::SafePointer<Forge64Editor> safe(this);
    const int pad = activePad();
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe, pad](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        auto f = fc.getResult();
        if (f == juce::File())
            return;
        if (f.getFileExtension().isEmpty())
            f = f.withFileExtension(".pad");
        safe->afterPresetOp(safe->processor.presets().savePad(f, pad));
    });
}

} // namespace f64
