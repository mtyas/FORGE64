#include "PadGridView.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

// ---------------------------------------------------------------------------
class PadGridView::PadCell : public juce::Component, public juce::DragAndDropTarget
{
public:
    PadCell(PadGridView& o) : owner(o)
    {
        muteBtn.setButtonText("M");
        muteBtn.setTooltip("Mute pad");
        ui::styleButton(muteBtn);
        muteBtn.onClick = [this]
        {
            const bool m = ! (bool) owner.proc().grid().padState(pad).getProperty("mute", false);
            owner.proc().grid().padState(pad).setProperty("mute", m, nullptr);
            updateButtons();
        };
        addAndMakeVisible(muteBtn);

        soloBtn.setButtonText("S");
        soloBtn.setTooltip("Solo pad");
        ui::styleButton(soloBtn);
        soloBtn.onClick = [this]
        {
            const bool s = ! (bool) owner.proc().grid().padState(pad).getProperty("solo", false);
            owner.proc().grid().padState(pad).setProperty("solo", s, nullptr);
            owner.refreshPads();
        };
        addAndMakeVisible(soloBtn);

        zoomBtn.setButtonText("EDIT");
        zoomBtn.setTooltip("Open pad dashboard (or double-click pad)");
        ui::styleButton(zoomBtn);
        zoomBtn.onClick = [this] { owner.host.padClicked(pad); };
        addAndMakeVisible(zoomBtn);
    }

    void setGlobalPad(int gp) { pad = gp; updateButtons(); repaint(); }
    int  globalPad() const { return pad; }
    void setSelectedFlag(bool s) { selected = s; repaint(); }

    void updateButtons()
    {
        const bool m = (bool) owner.proc().grid().padState(pad).getProperty("mute", false);
        const bool s = (bool) owner.proc().grid().padState(pad).getProperty("solo", false);
        muteBtn.setColour(juce::TextButton::buttonColourId, m ? juce::Colour(0xFFE63946) : ui::panelHi().withAlpha(0.85f));
        muteBtn.setColour(juce::TextButton::textColourOffId, m ? juce::Colours::white : ui::dim());
        soloBtn.setColour(juce::TextButton::buttonColourId, s ? juce::Colour(0xFFFFB703) : ui::panelHi().withAlpha(0.85f));
        soloBtn.setColour(juce::TextButton::textColourOffId, s ? juce::Colour(0xFF111111) : ui::dim());
        muteBtn.repaint();
        soloBtn.repaint();
    }

    void resized() override
    {
        muteBtn.setBounds(getWidth() - 44, 4, 18, 16);
        soloBtn.setBounds(getWidth() - 24, 4, 18, 16);
        zoomBtn.setBounds(getWidth() - 38, getHeight() - 22, 34, 18);
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        auto& proc = owner.proc();
        auto& grid = proc.grid();
        const auto st = grid.padState(pad);

        const auto bounds = getLocalBounds().toFloat();
        // 1. Recessed chassis well
        g.setColour(panel().darker(0.7f));
        g.fillRoundedRectangle(bounds, 8.f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(bounds, 8.f, 1.0f);

        auto rc = bounds.reduced(3.5f);
        const juce::Colour base((juce::uint32) (int) st.getProperty("colour", (int) 0xFF3A6EA5));

        // 1. Base frame (anvil plate groove)
        g.setColour(ui::panel().darker(0.35f));
        g.fillRoundedRectangle(bounds, 7.f);

        // 2. Outer ambient rim
        if (selected)
        {
            g.setColour(accentGlow().withAlpha(0.45f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 7.f, 3.0f);
            g.setColour(accent());
            g.drawRoundedRectangle(bounds.reduced(1.5f), 7.f, 2.0f);
        }
        else
        {
            g.setColour(line().withAlpha(0.65f));
            g.drawRoundedRectangle(bounds.reduced(1.5f), 7.f, 1.2f);
        }

        // 3. Main pad surface: Dark cast iron plate with subtle warm ember undertone
        juce::Colour padTop = base.darker(0.55f);
        juce::Colour padBottom = base.darker(0.82f);
        if (selected)
        {
            padTop = base.darker(0.30f);
            padBottom = base.darker(0.60f);
        }

        g.setGradientFill(juce::ColourGradient(padTop, rc.getX(), rc.getY(),
                                               padBottom, rc.getX(), rc.getBottom(), false));
        g.fillRoundedRectangle(rc, 6.f);

        // Forged surface bevel line
        g.setColour(juce::Colour(0xFFFF7700).withAlpha(selected ? 0.25f : 0.08f));
        g.drawHorizontalLine((int) rc.getY(), rc.getX() + 4.f, rc.getRight() - 4.f);

        // 4. Hit Flash: Brilliant white-hot strike bloom fading into fiery molten orange & embers!
        if (flash > 0.01f)
        {
            const float flashAlpha = (float) (flash * juce::jmax(0.25f, vel));

            // Outer fiery heat bloom
            g.setColour(accentGlow().withAlpha(flashAlpha * 0.75f));
            g.fillRoundedRectangle(rc, 6.f);

            // Inner incandescent white-hot core
            g.setColour(accentHot().withAlpha(flashAlpha * 0.9f));
            g.fillRoundedRectangle(rc.reduced(2.f), 4.f);

            // Strike halo rim
            g.setColour(accentHot().withAlpha(flashAlpha));
            g.drawRoundedRectangle(rc, 6.f, 2.5f);
        }

        // 5. Pad outline rim
        g.setColour(selected ? accent() : base.withAlpha(0.6f));
        g.drawRoundedRectangle(rc, 6.f, selected ? 2.0f : 1.0f);

        // 6. Header: Coordinate & Category Pill Badge
        const int bankIdx = pad / kPadsPerBank;
        const char bankChar = (char) ('A' + bankIdx);
        const juce::String padCoord = juce::String::charToString(bankChar)
                                    + juce::String::formatted("%02d", (pad % kPadsPerBank) + 1);

        auto headerRc = rc.reduced(6.f, 5.f);
        auto topRow = headerRc.removeFromTop(18.f);

        g.setFont(uiFont(13.f, true));
        g.setColour(selected ? accent().brighter(0.4f) : txt());
        g.drawText(padCoord, topRow.removeFromLeft(42.f), juce::Justification::centredLeft);

        int srcType = SRC_SAMPLE;
        if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "src")))
            srcType = juce::jlimit(0, (int) SRC_COUNT - 1, (int) p->load());

        const juce::String catName = padCategoryName(srcType);
        topRow.removeFromRight(46.f); // Leave room for Mute & Solo buttons
        auto badgeRc = topRow.removeFromRight(44.f).reduced(0, 1);
        g.setColour(base.brighter(0.2f).withAlpha(0.28f));
        g.fillRoundedRectangle(badgeRc, 4.f);
        g.setColour(base.brighter(0.6f).withAlpha(0.85f));
        g.drawRoundedRectangle(badgeRc, 4.f, 0.8f);
        g.setFont(uiFont(9.f, true));
        g.setColour(base.brighter(0.75f));
        g.drawText(catName, badgeRc, juce::Justification::centred);

        int choke = 0;
        if (auto* p = proc.getAPVTS().getRawParameterValue(padParamId(pad, "chok")))
            choke = (int) p->load();
        if (choke > 0)
        {
            auto chokeRc = topRow.removeFromRight(36.f).reduced(2, 1);
            g.setFont(uiFont(8.5f, true));
            g.setColour(ui::dim().brighter(0.2f));
            g.drawText("CH" + juce::String(choke), chokeRc, juce::Justification::centred);
        }

        auto name = st.getProperty("name", "").toString();
        const bool hasSample = st.getProperty("sample", "").toString().isNotEmpty();
        if (name.isEmpty())
        {
            if (hasSample)
                name = juce::File(st.getProperty("sample", "").toString()).getFileNameWithoutExtension();
            else
                name = padSourceTypeName(srcType);
        }

        auto bottomRow = headerRc.removeFromBottom(22.f);
        bottomRow.removeFromRight(40.f); // make room for zoom button
        g.setFont(uiFont(11.5f, true));
        g.setColour(txt().withAlpha(0.92f));
        g.drawText(name, bottomRow, juce::Justification::centredLeft, true);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragged = false;
        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        if (e.mods.isLeftButtonDown())
        {
            const float posFactor = 1.0f - ((float) e.y / (float) juce::jmax(1, getHeight()));
            const float velVal = juce::jlimit(0.35f, 1.0f, 0.45f + posFactor * 0.55f);
            owner.proc().triggerAudition(pad, velVal);
            owner.setSelected(pad);
            owner.host.padSelected(pad);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            owner.host.padClicked(pad);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragged && e.getDistanceFromDragStart() > 6)
        {
            dragged = true;
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                const juce::String dragId = e.mods.isAltDown() ? ("f64padcopy:" + juce::String(pad))
                                                               : ("f64pad:" + juce::String(pad));
                c->startDragging(dragId, this);
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragged = false;
    }

    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        const auto d = details.description.toString();
        return d.startsWith("f64padcopy:") || d.startsWith("f64pad:") || details.description.isArray();
    }

    void itemDropped(const SourceDetails& details) override
    {
        const auto desc = details.description.toString();
        if (desc.startsWith("f64padcopy:"))
        {
            const int from = desc.fromFirstOccurrenceOf("f64padcopy:", false, false).getIntValue();
            owner.proc().grid().copyPad(from, pad);
        }
        else if (details.description.isArray())
        {
            for (int i = 0; i < details.description.size(); ++i)
            {
                const juce::File f(details.description[i].toString());
                if (f.existsAsFile())
                {
                    owner.proc().grid().setSampleFile(pad, f);
                    break;
                }
            }
        }
        owner.refreshPads();
    }

    void tickFlash()
    {
        auto& rt = owner.proc().grid().runtime(pad);
        const int64_t stamp = rt.lastHitStamp.load();
        if (stamp != lastStamp)
        {
            lastStamp = stamp;
            if (stamp > 0)
            {
                flash = 1.0;
                vel = rt.lastVel.load();
            }
        }
        if (flash > 0.01)
        {
            flash *= 0.86;
            repaint();
        }
    }

private:
    void showMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Load Sample...");
        menu.addItem(2, "Clear Sample");
        menu.addItem(3, "Clear Pad (keeps params)");
        menu.addSeparator();
        menu.addItem(4, "Unmute All Pads");
        menu.addItem(5, "Unsolo All Pads");
        static const int cols[8] = { (int) 0xFF3A6EA5, (int) 0xFF3E8E5A, (int) 0xFFC2703A, (int) 0xFF8E5BC7,
                                     (int) 0xFFC74B5B, (int) 0xFF4BB8C7, (int) 0xFFB8A63A, (int) 0xFF7A7F8A };
        for (int i = 0; i < 8; ++i)
            menu.addItem(10 + i, "Colour " + juce::String(i + 1));

        juce::Component::SafePointer<PadCell> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe](int result)
        {
            if (safe == nullptr)
                return;
            auto& cell = *safe;
            if (result == 1)
                cell.owner.chooseSampleFor(cell.pad);
            else if (result == 2)
            {
                cell.owner.proc().grid().clearSample(cell.pad);
                cell.owner.refreshPads();
            }
            else if (result == 3)
            {
                cell.owner.proc().grid().clearPad(cell.pad);
                cell.owner.refreshPads();
            }
            else if (result == 4)
            {
                for (int p = 0; p < kNumPads; ++p)
                    cell.owner.proc().grid().padState(p).setProperty("mute", false, nullptr);
                cell.owner.refreshPads();
            }
            else if (result == 5)
            {
                for (int p = 0; p < kNumPads; ++p)
                    cell.owner.proc().grid().padState(p).setProperty("solo", false, nullptr);
                cell.owner.refreshPads();
            }
            else if (result >= 10 && result < 18)
            {
                cell.owner.proc().grid().padState(cell.pad)
                    .setProperty("colour", cols[result - 10], nullptr);
                cell.repaint();
            }
        });
    }

    PadGridView& owner;
    int pad = 0;
    bool selected = false;
    bool dragged = false;
    double flash = 0.0;
    float vel = 0.f;
    int64_t lastStamp = 0;
    juce::TextButton muteBtn;
    juce::TextButton soloBtn;
    juce::TextButton zoomBtn;
};

// ---------------------------------------------------------------------------
PadGridView::PadGridView(Forge64Processor& p, Host& h)
    : processor(p), host(h)
{
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        cells[(size_t) i] = std::make_unique<PadCell>(*this);
        cells[(size_t) i]->setGlobalPad(i);
        addAndMakeVisible(cells[(size_t) i].get());
    }
}

PadGridView::~PadGridView() = default;

void PadGridView::setBank(int b)
{
    bank = clampRange(b, 0, kNumBanks - 1);
    for (int i = 0; i < kPadsPerBank; ++i)
        cells[(size_t) i]->setGlobalPad(bank * kPadsPerBank + i);
    setSelected(-1);
    refreshPads();
}

void PadGridView::setSelected(int globalPad)
{
    selectedPad = globalPad;
    for (auto& c : cells)
        c->setSelectedFlag(c->globalPad() == selectedPad);
}

void PadGridView::refreshPads()
{
    for (auto& c : cells)
    {
        c->updateButtons();
        c->repaint();
    }
}

void PadGridView::tick()
{
    for (auto& c : cells)
        c->tickFlash();
}

void PadGridView::resized()
{
    auto r = getLocalBounds().reduced(10);
    const int gap = 7;
    const int cw = (r.getWidth() - 3 * gap) / 4;
    const int ch = (r.getHeight() - 3 * gap) / 4;
    for (int i = 0; i < kPadsPerBank; ++i)
        cells[(size_t) i]->setBounds(r.getX() + (i % 4) * (cw + gap),
                                     r.getY() + (i / 4) * (ch + gap), cw, ch);
}

void PadGridView::chooseSampleFor(int pad)
{
    chooser = std::make_unique<juce::FileChooser>(
        "Load sample for PAD " + juce::String(pad + 1), juce::File(),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", this);

    juce::Component::SafePointer<PadGridView> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safe, pad](const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
        {
            safe->processor.grid().setSampleFile(pad, f);
            safe->refreshPads();
        }
    });
}

} // namespace f64
