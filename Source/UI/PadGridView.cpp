#include "PadGridView.h"
#include "UICommon.h"
#include "../PluginProcessor.h"

namespace f64 {

// ---------------------------------------------------------------------------
class PadGridView::PadCell : public juce::Component, public juce::DragAndDropTarget
{
public:
    PadCell(PadGridView& o) : owner(o) {}

    void setGlobalPad(int gp) { pad = gp; repaint(); }
    int  globalPad() const { return pad; }
    void setSelectedFlag(bool s) { selected = s; repaint(); }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        auto& proc = owner.proc();
        auto& grid = proc.grid();
        const auto st = grid.padState(pad);

        auto rc = getLocalBounds().reduced(3).toFloat();
        const juce::Colour base((juce::uint32) (int) st.getProperty("colour", (int) 0xFF3A6EA5));

        g.setGradientFill(juce::ColourGradient(base.darker(0.62f), rc.getX(), rc.getY(),
                                               base.darker(0.18f), rc.getX(), rc.getBottom(), false));
        g.fillRoundedRectangle(rc, 7.f);

        if (flash > 0.01f)
        {
            g.setColour(juce::Colours::white.withAlpha((float) (flash * 0.55f * juce::jmax(0.25f, vel))));
            g.fillRoundedRectangle(rc, 7.f);
        }

        g.setColour(selected ? accent() : base.brighter(0.55f));
        g.drawRoundedRectangle(rc, 7.f, selected ? 2.4f : 1.2f);

        // pad number
        g.setFont(uiFont(15.f, true));
        g.setColour(txt());
        g.drawText(juce::String(pad + 1), rc.removeFromTop(20.f).reduced(7, 0),
                   juce::Justification::centredLeft);

        // lua-source tag
        if (auto* srcPar = proc.getAPVTS().getParameter(padParamId(pad, "src")))
            if (srcPar->getValue() > 0.5f)
            {
                g.setFont(uiFont(9.f, true));
                g.setColour(accent());
                g.drawText("LUA", rc.removeFromTop(18.f).reduced(0, 3),
                           juce::Justification::centredRight);
            }

        // name / status
        auto name = st.getProperty("name", "").toString();
        const bool hasSample = st.getProperty("sample", "").toString().isNotEmpty();
        g.setFont(uiFont(10.5f));
        g.setColour(name.isNotEmpty() ? dim().brighter(0.35f) : dim().darker(0.25f));
        g.drawText(name.isNotEmpty() ? name : (hasSample ? "<sample>" : "<empty>"),
                   rc.removeFromBottom(18.f).reduced(7, 3), juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragged = false;
        if (e.mods.isPopupMenu())
            showMenu();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isAltDown() && ! dragged)
        {
            dragged = true;
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
                c->startDragging("f64padcopy:" + juce::String(pad), this);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (! dragged && ! e.mods.isPopupMenu() && e.mods.isLeftButtonDown())
        {
            owner.setSelected(pad);
            owner.host.padClicked(pad);
        }
        dragged = false;
    }

    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        const auto d = details.description.toString();
        return d.startsWith("f64padcopy:") || details.description.isArray();
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
        static const int cols[8] = { (int) 0xFF3A6EA5, (int) 0xFF3E8E5A, (int) 0xFFC2703A, (int) 0xFF8E5BC7,
                                     (int) 0xFFC74B5B, (int) 0xFF4BB8C7, (int) 0xFFB8A63A, (int) 0xFF7A7F8A };
        for (int i = 0; i < 8; ++i)
            menu.addItem(10 + i, "Colour " + juce::String(i + 1));

        juce::Component::SafePointer<PadCell> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe, cols](int result)
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
        c->repaint();
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
