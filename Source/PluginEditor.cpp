#include "PluginEditor.h"
#include "UI/UICommon.h"

namespace f64 {

class ToolIconButton : public juce::Button
{
public:
    enum IconType { Undo, Redo, MidiLearn };

    ToolIconButton(IconType t, const juce::String& name)
        : juce::Button(name), type(t)
    {
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.f);
        const bool active = (type == MidiLearn && getToggleState());

        juce::Colour bgCol = active ? juce::Colour(0xFF5A2208)
                           : (isDown ? ui::panelHover().brighter(0.12f)
                           : (isOver ? ui::panelHover() : ui::panelHi()));

        g.setColour(bgCol);
        g.fillRoundedRectangle(b, 4.f);

        if (active)
        {
            g.setColour(ui::accent());
            g.drawRoundedRectangle(b, 4.f, 1.5f);
        }
        else
        {
            g.setColour(isOver ? ui::line().brighter(0.2f) : ui::line());
            g.drawRoundedRectangle(b, 4.f, 1.f);
        }

        juce::Colour iconCol = active ? ui::accentHot()
                             : (isOver ? ui::accent() : ui::txt());

        const float cx = b.getCentreX();
        const float cy = b.getCentreY();

        if (type == Undo)
        {
            // Counter-clockwise curved undo arrow
            juce::Path arrow;
            arrow.startNewSubPath(cx - 2.0f, cy - 6.0f);
            arrow.lineTo(cx - 7.0f, cy - 2.5f);
            arrow.lineTo(cx - 2.0f, cy + 1.0f);

            arrow.startNewSubPath(cx - 6.0f, cy - 2.5f);
            arrow.quadraticTo(cx + 6.0f, cy - 5.5f, cx + 5.5f, cy + 5.0f);

            g.setColour(iconCol);
            g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (type == Redo)
        {
            // Clockwise curved redo arrow (horizontally mirrored)
            juce::Path arrow;
            arrow.startNewSubPath(cx + 2.0f, cy - 6.0f);
            arrow.lineTo(cx + 7.0f, cy - 2.5f);
            arrow.lineTo(cx + 2.0f, cy + 1.0f);

            arrow.startNewSubPath(cx + 6.0f, cy - 2.5f);
            arrow.quadraticTo(cx - 6.0f, cy - 5.5f, cx - 5.5f, cy + 5.0f);

            g.setColour(iconCol);
            g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (type == MidiLearn)
        {
            // 5-pin DIN connector icon on left + text "LEARN" on right + glowing status LED
            const float dinX = b.getX() + 13.0f;
            const float dinY = cy;
            const float dinR = 6.5f;

            // Outer DIN circle
            g.setColour(iconCol.withAlpha(0.85f));
            g.drawEllipse(dinX - dinR, dinY - dinR, dinR * 2.0f, dinR * 2.0f, 1.2f);

            // 5 pins in classic DIN arc
            static const float pinAngles[5] = { -2.356f, -1.571f, -0.785f, -0.2f, -2.94f };
            g.setColour(iconCol);
            for (float ang : pinAngles)
            {
                float px = dinX + std::cos(ang) * (dinR * 0.58f);
                float py = dinY + std::sin(ang) * (dinR * 0.58f);
                g.fillEllipse(px - 0.9f, py - 0.9f, 1.8f, 1.8f);
            }

            // Text "LEARN"
            g.setFont(uiFont(9.0f, true));
            g.drawText("LEARN", (int) (dinX + dinR + 3.0f), (int) (cy - 7.0f),
                       (int) (b.getRight() - dinX - dinR - 8.0f), 14,
                       juce::Justification::centredLeft);

            // LED indicator dot at top-right
            const float ledX = b.getRight() - 6.0f;
            const float ledY = b.getY() + 7.0f;
            if (active)
            {
                g.setColour(juce::Colour(0xFFFF6600).withAlpha(0.35f));
                g.fillEllipse(ledX - 4.5f, ledY - 4.5f, 9.0f, 9.0f);
                g.setColour(juce::Colour(0xFFFFD166));
                g.fillEllipse(ledX - 2.5f, ledY - 2.5f, 5.0f, 5.0f);
            }
            else
            {
                g.setColour(juce::Colour(0xFF35201A));
                g.fillEllipse(ledX - 2.0f, ledY - 2.0f, 4.0f, 4.0f);
                g.setColour(ui::line());
                g.drawEllipse(ledX - 2.0f, ledY - 2.0f, 4.0f, 4.0f, 0.8f);
            }
        }
    }

private:
    IconType type;
};

class PageNavLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool isOn = button.getToggleState();

        if (isOn)
        {
            // Active page: molten forge plate gradient
            juce::ColourGradient grad(juce::Colour(0xFF752B08), bounds.getX(), bounds.getY(),
                                      juce::Colour(0xFF381203), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 4.f);

            // Glowing ember rim
            g.setColour(ui::accent().withAlpha(0.9f));
            g.drawRoundedRectangle(bounds, 4.f, 1.4f);

            // White-hot accent line at bottom
            auto bottomLine = bounds.removeFromBottom(3.0f);
            g.setColour(ui::accentHot());
            g.fillRoundedRectangle(bottomLine.reduced(3.f, 0.f), 1.5f);
        }
        else
        {
            // Inactive page: cast iron plate with warm hover
            juce::Colour bg = isButtonDown ? ui::panelHover().brighter(0.12f)
                            : (isMouseOverButton ? ui::panelHover() : ui::panelHi());
            g.setColour(bg);
            g.fillRoundedRectangle(bounds, 4.f);

            g.setColour(isMouseOverButton ? ui::line().brighter(0.25f) : ui::line());
            g.drawRoundedRectangle(bounds, 4.f, 1.0f);

            if (isMouseOverButton)
            {
                g.setColour(ui::accent().withAlpha(0.45f));
                g.drawHorizontalLine((int) bounds.getY() + 1, bounds.getX() + 3.f, bounds.getRight() - 3.f);
            }
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isMouseOverButton, bool /*isButtonDown*/) override
    {
        const bool isOn = button.getToggleState();
        g.setFont(uiFont(11.5f, true));
        juce::Colour textCol = isOn ? juce::Colour(0xFFFFF7F0)
                             : (isMouseOverButton ? ui::accentHot() : ui::txt());
        g.setColour(textCol);
        auto r = button.getLocalBounds();
        if (isOn)
            r = r.withTrimmedBottom(2);
        g.drawText(button.getButtonText(), r, juce::Justification::centred, false);
    }
};

Forge64Editor::Forge64Editor(Forge64Processor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(980, 640, 2560, 1600);

    logo = ui::makeLabel("FORGE64", 22.f, ui::accentHot());
    logo->setFont(uiFont(22.f, true));
    addAndMakeVisible(logo.get());

    tagline = ui::makeLabel("A CREATION OF MTYAS", 8.0f, ui::dim().brighter(0.2f));
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

    pageNavLnF = std::make_unique<PageNavLookAndFeel>();

    auto makeNav = [&](std::unique_ptr<juce::TextButton>& btn, const char* name, ActivePage pg)
    {
        btn = std::make_unique<juce::TextButton>(name);
        ui::styleButton(*btn);
        btn->setLookAndFeel(pageNavLnF.get());
        btn->setClickingTogglesState(true);
        btn->onClick = [this, pg] { setPage(pg); };
        addAndMakeVisible(btn.get());
    };

    makeNav(gridNavBtn,   "PAD GRID",  Page_Grid);
    makeNav(editNavBtn,   "PAD EDIT",  Page_PadEdit);
    makeNav(mixerNavBtn,  "MIX & FX",   Page_MixerFX);
    makeNav(seqNavBtn,    "SEQUENCER", Page_Sequencer);
    makeNav(performNavBtn,"PERFORM",   Page_Performance);

    undoBtn = std::make_unique<ToolIconButton>(ToolIconButton::Undo, "UNDO");
    undoBtn->setTooltip("Undo parameter gesture (Ctrl+Z)");
    undoBtn->onClick = [this] { processor.getUndoManager().undo(); };
    addAndMakeVisible(undoBtn.get());

    redoBtn = std::make_unique<ToolIconButton>(ToolIconButton::Redo, "REDO");
    redoBtn->setTooltip("Redo parameter gesture (Ctrl+Y)");
    redoBtn->onClick = [this] { processor.getUndoManager().redo(); };
    addAndMakeVisible(redoBtn.get());

    midiLearnBtn = std::make_unique<ToolIconButton>(ToolIconButton::MidiLearn, "MIDI LEARN");
    midiLearnBtn->setClickingTogglesState(true);
    midiLearnBtn->setTooltip("Global MIDI CC Learn - toggle ON and touch any knob then move your hardware controller");
    midiLearnBtn->onClick = [this]
    {
        processor.getMidiLearn().setLearnActive(midiLearnBtn->getToggleState());
    };
    addAndMakeVisible(midiLearnBtn.get());

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
    addChildComponent(grid.get());

    mixerPage = std::make_unique<MixerFXPage>(processor, *this);
    addChildComponent(mixerPage.get());

    seqPage = std::make_unique<SequencerPage>(processor);
    addChildComponent(seqPage.get());

    performPage = std::make_unique<PerformancePage>(processor, *this);
    addChildComponent(performPage.get());

    sequencerDrawer = std::make_unique<SequencerDrawer>(processor);
    sequencerDrawer->getActivePad = [this]
    {
        if (currentPage == Page_PadEdit && padEdit != nullptr)
            return padEdit->padIndex();
        return activePad();
    };
    sequencerDrawer->onFoldStateChanged = [this](bool) { layoutCenter(); };
    sequencerDrawer->onStepClicked = [this](int trackIdx, int stepIdx, int padIdx)
    {
        selectedPad = padIdx;
        setPage(Page_PadEdit);
        if (padEdit)
            padEdit->enterPLockMode(trackIdx, stepIdx);
    };
    addAndMakeVisible(sequencerDrawer.get());

    modPanel = std::make_unique<ModPanel>(processor.mods(),
                                          processor.getKit().getChildWithName("MODSRC"),
                                          processor.getKit().getChildWithName("MODMAT"),
                                          &processor);
    addAndMakeVisible(modPanel.get());

    setSize(juce::jlimit(980, 2560, processor.lastUIWidth),
            juce::jlimit(640, 1600, processor.lastUIHeight));
    setBank(0);
    setPage(Page_Grid);
    startTimerHz(30);
    isInitialized = true;
}

Forge64Editor::~Forge64Editor()
{
    stopTimer();
    if (gridNavBtn)    gridNavBtn->setLookAndFeel(nullptr);
    if (editNavBtn)    editNavBtn->setLookAndFeel(nullptr);
    if (mixerNavBtn)   mixerNavBtn->setLookAndFeel(nullptr);
    if (seqNavBtn)     seqNavBtn->setLookAndFeel(nullptr);
    if (performNavBtn) performNavBtn->setLookAndFeel(nullptr);

    sequencerDrawer.reset();
    performPage.reset();
    seqPage.reset();
    mixerPage.reset();
    padEdit.reset();
    grid.reset();
    modPanel.reset();
    masterAtt.reset();
    masterKnob.reset();
}

void Forge64Editor::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());

    const float bannerH = 54.f;
    // Forged steel top banner with subtle furnace ambient gradient
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF1E1715), 0.f, 0.f,
                                           juce::Colour(0xFF130E0D), 0.f, bannerH, false));
    g.fillRect(0.f, 0.f, (float) getWidth(), bannerH);

    // Warm ember line along the very top
    g.setColour(juce::Colour(0xFFFF6600).withAlpha(0.12f));
    g.drawHorizontalLine(0, 0.f, (float) getWidth());

    // Vertical divider separating page navigation tabs from utility tool buttons
    if (toolDividerLeftX > 0.f)
    {
        g.setColour(juce::Colour(0xFF0A0706));
        g.drawVerticalLine((int) toolDividerLeftX, 12.f, 42.f);
        g.setColour(ui::line().brighter(0.2f));
        g.drawVerticalLine((int) toolDividerLeftX + 1, 12.f, 42.f);
    }

    // Vertical divider separating utility tool buttons from preset buttons
    if (toolDividerRightX > 0.f)
    {
        g.setColour(juce::Colour(0xFF0A0706));
        g.drawVerticalLine((int) toolDividerRightX, 12.f, 42.f);
        g.setColour(ui::line().brighter(0.2f));
        g.drawVerticalLine((int) toolDividerRightX + 1, 12.f, 42.f);
    }

    // Seam line separating top bar: Smoldering molten divider
    g.setColour(ui::line());
    g.drawHorizontalLine((int) bannerH, 0.f, (float) getWidth());
    g.setColour(ui::accent().withAlpha(0.18f));
    g.drawHorizontalLine((int) bannerH - 1, 0.f, (float) getWidth());
}

void Forge64Editor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    if (isInitialized)
    {
        processor.lastUIWidth = w;
        processor.lastUIHeight = h;
    }

    if (logo) logo->setBounds(12, 6, 120, 26);
    if (tagline) tagline->setBounds(14, 33, 130, 14);

    for (int i = 0; i < kNumBanks; ++i)
        if (bankBtns[(size_t) i])
            bankBtns[(size_t) i]->setBounds(136 + i * 29, 11, 26, 32);

    // Right-hand header controls (Master knob and Kit/Bank/Pad preset menus)
    if (masterKnob)  masterKnob->setBounds(w - 78, 4, 70, 46);
    if (padMenuBtn)  padMenuBtn->setBounds(w - 140, 11, 56, 32);
    if (bankMenuBtn) bankMenuBtn->setBounds(w - 202, 11, 58, 32);
    if (kitBtn)      kitBtn->setBounds(w - 262, 11, 56, 32);

    toolDividerRightX = (float) (w - 270);

    // Undo, Redo, Midi Learn icon buttons placed right before KIT
    int rx = w - 278;
    if (midiLearnBtn) { rx -= 64; midiLearnBtn->setBounds(rx, 11, 62, 32); rx -= 4; }
    if (redoBtn)      { rx -= 30; redoBtn->setBounds(rx, 11, 28, 32); rx -= 2; }
    if (undoBtn)      { rx -= 30; undoBtn->setBounds(rx, 11, 28, 32); rx -= 6; }

    toolDividerLeftX = (float) (rx + 2);

    // Main Page navigation tabs: prominent, bold, fully readable
    const int availableForNav = (int) toolDividerLeftX - 12 - 256;
    int gW = 88, eW = 88, mW = 84, sW = 106, pW = 88;
    if (availableForNav < 470 && availableForNav > 200)
    {
        const float scale = juce::jlimit(0.68f, 1.0f, (float) availableForNav / 470.f);
        gW = (int) (88 * scale);
        eW = (int) (88 * scale);
        mW = (int) (84 * scale);
        sW = (int) (106 * scale);
        pW = (int) (88 * scale);
    }

    int nx = 256;
    if (gridNavBtn)    { gridNavBtn->setBounds(nx, 11, gW, 32); nx += gW + 4; }
    if (editNavBtn)    { editNavBtn->setBounds(nx, 11, eW, 32); nx += eW + 4; }
    if (mixerNavBtn)   { mixerNavBtn->setBounds(nx, 11, mW, 32); nx += mW + 4; }
    if (seqNavBtn)     { seqNavBtn->setBounds(nx, 11, sW, 32); nx += sW + 4; }
    if (performNavBtn) { performNavBtn->setBounds(nx, 11, pW, 32); nx += pW + 4; }

    if (modPanel) modPanel->setBounds(w - 350, 54, 350, h - 54);
    layoutCenter();
}

bool Forge64Editor::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z')
        {
            if (key.getModifiers().isShiftDown())
                processor.getUndoManager().redo();
            else
                processor.getUndoManager().undo();
            return true;
        }
        if (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y')
        {
            processor.getUndoManager().redo();
            return true;
        }
    }
    return false;
}

juce::Rectangle<int> Forge64Editor::centerBounds() const
{
    return getLocalBounds().withTrimmedTop(54).withTrimmedRight(350);
}

void Forge64Editor::layoutCenter()
{
    auto r = centerBounds();
    if (sequencerDrawer)
    {
        const int dH = sequencerDrawer->getDesiredHeight();
        sequencerDrawer->setBounds(r.removeFromBottom(dH));
        sequencerDrawer->setVisible(currentPage != Page_Sequencer);
        sequencerDrawer->toFront(false);
    }
    if (grid && currentPage == Page_Grid)
        grid->setBounds(r);
    if (padEdit && currentPage == Page_PadEdit)
        padEdit->setBounds(r);
    if (mixerPage && currentPage == Page_MixerFX)
        mixerPage->setBounds(r);
    if (seqPage && currentPage == Page_Sequencer)
        seqPage->setBounds(r);
    if (performPage && currentPage == Page_Performance)
        performPage->setBounds(r);
}

void Forge64Editor::setPage(ActivePage p)
{
    currentPage = p;

    if (gridNavBtn)    gridNavBtn->setToggleState(p == Page_Grid, juce::dontSendNotification);
    if (editNavBtn)    editNavBtn->setToggleState(p == Page_PadEdit, juce::dontSendNotification);
    if (mixerNavBtn)   mixerNavBtn->setToggleState(p == Page_MixerFX, juce::dontSendNotification);
    if (seqNavBtn)     seqNavBtn->setToggleState(p == Page_Sequencer, juce::dontSendNotification);
    if (performNavBtn) performNavBtn->setToggleState(p == Page_Performance, juce::dontSendNotification);

    if (p == Page_PadEdit)
    {
        const int targetPad = activePad();
        if (padEdit == nullptr || padEdit->padIndex() != targetPad)
        {
            zoomedPad = targetPad;
            rebuildPadEditor();
        }
        else
        {
            padEdit->setVisible(true);
        }
    }
    else
    {
        zoomedPad = -1;
        if (padEdit)
        {
            if (padEdit->isPLockMode())
                padEdit->exitPLockMode();
            padEdit->setVisible(false);
        }
    }

    if (grid)
    {
        grid->setVisible(p == Page_Grid);
        if (p == Page_Grid)
            grid->refreshPads();
    }

    if (mixerPage)
        mixerPage->setVisible(p == Page_MixerFX);

    if (seqPage)
        seqPage->setVisible(p == Page_Sequencer);

    if (performPage)
        performPage->setVisible(p == Page_Performance);

    layoutCenter();
}

void Forge64Editor::timerCallback()
{
    if (grid && currentPage == Page_Grid)
        grid->tick();

    if (midiLearnBtn)
    {
        const bool learning = processor.getMidiLearn().isLearning();
        if (midiLearnBtn->getToggleState() != learning)
        {
            midiLearnBtn->setToggleState(learning, juce::dontSendNotification);
            midiLearnBtn->repaint();
        }
    }

    const int mb = processor.uiBank().load();
    if (mb != currentBank)
        setBank(mb);

    if (processor.mods().uiActive())
        for (auto* k : knobList)
            k->repaint();
}

void Forge64Editor::setBank(int b)
{
    currentBank = clampRange(b, 0, kNumBanks - 1);
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
    setPage(Page_PadEdit);
}

void Forge64Editor::rebuildPadEditor()
{
    padEdit = std::make_unique<PadEditor>(processor, *this, zoomedPad,
                                          [this] { showGrid(); });
    padEdit->onPadChanged = [this](int newPad)
    {
        selectedPad = newPad;
        zoomedPad = newPad;
        const int targetBank = newPad / kPadsPerBank;
        if (targetBank != currentBank)
            setBank(targetBank);
        rebuildPadEditor();
    };
    addAndMakeVisible(padEdit.get());
    if (grid)
        grid->setVisible(false);
    layoutCenter();
    if (sequencerDrawer)
        sequencerDrawer->toFront(false);
    padEdit->setAlpha(0.f);
    animator.fadeIn(padEdit.get(), 140);
}

void Forge64Editor::showGrid()
{
    setPage(Page_Grid);
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
                                               "FORGE64 by mtyas", processor.presets().lastError());
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
