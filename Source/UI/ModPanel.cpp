#include "ModPanel.h"
#include "ModRingKnob.h"

namespace f64 {

// ---------------------------------------------------------------------------
// ConnectionList
// ---------------------------------------------------------------------------
class ConnectionList::Row : public juce::Component
{
public:
    Row(ConnectionList& o, juce::ValueTree conn) : owner(o), tree(conn)
    {
        id = (int) conn.getProperty("id", 0);
        slot = (int) conn.getProperty("slot", 0);

        label.setText(slotName(slot) + "  ->  "
                      + destDisplayName(tree.getProperty("dest", "").toString()),
                      juce::dontSendNotification);
        label.setFont(uiFont(11.f));
        label.setColour(juce::Label::textColourId, ui::txt());
        addAndMakeVisible(label);

        amount.setSliderStyle(juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        amount.setRange(-1.0, 1.0, 0.001);
        amount.setValue((double) conn.getProperty("amount", 0.5), juce::dontSendNotification);
        amount.onValueChange = [this] { tree.setProperty("amount", amount.getValue(), nullptr); };
        amount.setColour(juce::Slider::backgroundColourId, ui::panelHi());
        amount.setColour(juce::Slider::trackColourId, ui::line());
        amount.setColour(juce::Slider::thumbColourId, slotColour(slot));
        amount.setTooltip("Modulation amount (bipolar)");
        addAndMakeVisible(amount);

        inv.setButtonText("INV");
        inv.setClickingTogglesState(true);
        inv.setToggleState(bool(conn.getProperty("invert", false)), juce::dontSendNotification);
        inv.onClick = [this] { tree.setProperty("invert", inv.getToggleState(), nullptr); };
        ui::styleButton(inv);
        addAndMakeVisible(inv);

        mute.setButtonText("M");
        mute.setClickingTogglesState(true);
        mute.setToggleState(bool(conn.getProperty("muted", false)), juce::dontSendNotification);
        mute.onClick = [this] { tree.setProperty("muted", mute.getToggleState(), nullptr); };
        ui::styleButton(mute);
        addAndMakeVisible(mute);

        del.setButtonText("x");
        del.onClick = [this] { owner.matrix.removeConnection(id); };
        ui::styleButton(del);
        addAndMakeVisible(del);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(2, 3);
        del.setBounds(r.removeFromRight(20));
        mute.setBounds(r.removeFromRight(26));
        inv.setBounds(r.removeFromRight(36));
        amount.setBounds(r.removeFromRight(86));
        r.removeFromLeft(12);
        label.setBounds(r);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(slotColour(slot));
        g.fillEllipse(4.f, (float) getHeight() * 0.5f - 3.5f, 7.f, 7.f);
    }

private:
    ConnectionList& owner;
    juce::ValueTree tree;
    int id = 0, slot = 0;
    juce::Label label;
    juce::Slider amount;
    juce::TextButton inv, mute, del;
};

class ConnectionList::Model : public juce::ListBoxModel
{
public:
    explicit Model(ConnectionList& o) : owner(o) {}
    int getNumRows() override { return (int) owner.items.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int, int, bool) override
    {
        g.fillAll(row % 2 ? ui::panel() : ui::panel().brighter(0.05f));
    }

    juce::Component* refreshComponentForRow(int row, bool, juce::Component* existing) override
    {
        std::unique_ptr<juce::Component> toDelete(existing);
        if (row >= 0 && row < (int) owner.items.size())
            return new Row(owner, owner.items[(size_t) row]);
        return nullptr;
    }

private:
    ConnectionList& owner;
};

ConnectionList::ConnectionList(ModMatrix& m, juce::ValueTree matTree, juce::String prefixIn)
    : matrix(m), tree(matTree), prefix(prefixIn)
{
    addAndMakeVisible(list);
    list.setModel(nullptr);
    model = std::make_unique<Model>(*this);
    list.setModel(model.get());
    list.setRowHeight(30);
    ui::styleListBox(list);
    tree.addListener(this);
    refresh();
}

ConnectionList::~ConnectionList()
{
    tree.removeListener(this);
    list.setModel(nullptr);
}

void ConnectionList::refresh()
{
    items.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto c = tree.getChild(i);
        const auto dest = c.getProperty("dest", "").toString();
        if (prefix.isEmpty() || dest.startsWith(prefix))
            items.push_back(c);
    }
    list.updateContent();
    list.repaint();
}

void ConnectionList::resized()
{
    list.setBounds(getLocalBounds());
}

void ConnectionList::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == tree) refresh();
}

void ConnectionList::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == tree) refresh();
}

// ---------------------------------------------------------------------------
// ModPanel - source rows
// ---------------------------------------------------------------------------
class ModPanel::SourceRow : public juce::Component
{
public:
    class Badge : public juce::Component
    {
    public:
        void setSlot(int s) { slot = s; repaint(); }

        void paint(juce::Graphics& g) override
        {
            auto rc = getLocalBounds().toFloat().reduced(1.f);
            g.setColour(slotColour(slot));
            g.fillEllipse(rc);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.setFont(uiFont(7.5f, true));
            g.drawText(slotShortName(slot), rc, juce::Justification::centred);
        }

        void mouseDrag(const juce::MouseEvent&) override
        {
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
                c->startDragging("f64mod:" + juce::String(slot), this);
        }
        void mouseDown(const juce::MouseEvent&) override {}
        void mouseUp(const juce::MouseEvent&) override {}

    private:
        int slot = 0;
    };

    explicit SourceRow(ModPanel& o) : owner(o)
    {
        addAndMakeVisible(badge);
        name.setFont(uiFont(11.5f));
        name.setColour(juce::Label::textColourId, ui::txt());
        addAndMakeVisible(name);

        ui::styleToggle(enable);
        enable.onClick = [this]
        {
            owner.matrixRef.setSourceParam(slot, "enabled", enable.getToggleState());
        };
        addAndMakeVisible(enable);

        edit.setButtonText(">");
        ui::styleButton(edit);
        edit.onClick = [this] { owner.openSourceEditor(slot, &edit); };
        addAndMakeVisible(edit);
    }

    void setSlot(int s)
    {
        slot = s;
        badge.setSlot(s);
        name.setText(slotName(s), juce::dontSendNotification);
        const auto st = owner.matrixRef.sourceState(s);
        const bool hasState = st.isValid();
        enable.setVisible(hasState);
        edit.setVisible(hasState);
        if (hasState)
            enable.setToggleState(bool(st.getProperty("enabled", false)), juce::dontSendNotification);
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(3, 3);
        badge.setBounds(r.removeFromLeft(18));
        edit.setBounds(r.removeFromRight(22));
        enable.setBounds(r.removeFromRight(24));
        r.removeFromLeft(4);
        name.setBounds(r);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ui::panel().brighter(slotClassOf(slot) == SC_MIDI || slotClassOf(slot) == SC_MACRO
                                          ? 0.0f : 0.03f));
    }

private:
    ModPanel& owner;
    int slot = 0;
    Badge badge;
    juce::Label name;
    juce::ToggleButton enable;
    juce::TextButton edit;
};

class ModPanel::SourceListModel : public juce::ListBoxModel
{
public:
    explicit SourceListModel(ModPanel& o) : owner(o)
    {
        struct Section { const char* title; int first; int count; };
        const Section secs[] = {
            { "LFO",                slotLFO(0),   kNumLFO },
            { "RANDOM / CHAOS",     slotRnd(0),   kNumRnd },
            { "ENVELOPE - DAHDSR",  slotEnv(0),   kNumEnv },
            { "STEP SEQUENCER",     slotSeq(0),   kNumSeq },
            { "MIDI / PERFORMANCE", slotMidi(0),  kNumMidiSrc },
            { "MACROS",             slotMacro(0), kNumMacros },
        };
        for (const auto& s : secs)
        {
            rows.push_back({ true, -1, s.title });
            for (int i = 0; i < s.count; ++i)
                rows.push_back({ false, s.first + i, {} });
        }
    }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;
        if (rows[(size_t) row].header)
        {
            g.fillAll(ui::bg());
            g.setColour(ui::dim());
            g.setFont(uiFont(10.f, true));
            g.drawText(rows[(size_t) row].title, 6, 0, w - 12, h - 4,
                       juce::Justification::centredLeft);
            g.setColour(ui::line());
            g.drawHorizontalLine(h - 3, 4.f, (float) w - 4.f);
        }
        else
        {
            g.fillAll(ui::panel());
        }
    }

    juce::Component* refreshComponentForRow(int row, bool, juce::Component* existing) override
    {
        std::unique_ptr<juce::Component> toDelete(existing);
        if (row >= 0 && row < (int) rows.size() && ! rows[(size_t) row].header)
        {
            auto* sr = new SourceRow(owner);
            sr->setSlot(rows[(size_t) row].slot);
            return sr;
        }
        return nullptr;
    }

private:
    struct RowInfo { bool header; int slot; juce::String title; };
    std::vector<RowInfo> rows;
    ModPanel& owner;
};

// ---------------------------------------------------------------------------
// ModPanel - per-source editor popups
// ---------------------------------------------------------------------------
class ModPanel::SourceEditorContent : public juce::Component
{
public:
    class StepGrid : public juce::Component
    {
    public:
        StepGrid(ModMatrix& m, int slot_) : matrix(m), slot(slot_) {}

        void paint(juce::Graphics& g) override
        {
            auto* seq = dynamic_cast<SeqSource*>(matrix.sourceAt(slot));
            if (seq == nullptr)
                return;
            const int N = clampRange(seq->numSteps.load(), 4, 32);
            const float w = (float) getWidth() / (float) N;
            const float h = (float) getHeight();

            g.fillAll(ui::bg());
            for (int i = 0; i < N; ++i)
            {
                const float v = juce::jlimit(0.f, 1.f, seq->steps[(size_t) i].load());
                const float bh = juce::jmax(2.f, v * (h - 4.f));
                g.setColour(slotColour(slot).withAlpha(0.35f + 0.6f * v));
                g.fillRect(w * (float) i + 1.f, h - bh - 2.f, w - 2.f, bh);
            }
            g.setColour(ui::line());
            g.drawRect(getLocalBounds(), 1);
        }

        void mouseDown(const juce::MouseEvent& e) override { setFromMouse(e); }
        void mouseDrag(const juce::MouseEvent& e) override { setFromMouse(e); }

    private:
        void setFromMouse(const juce::MouseEvent& e)
        {
            auto* seq = dynamic_cast<SeqSource*>(matrix.sourceAt(slot));
            if (seq == nullptr)
                return;
            const int N = clampRange(seq->numSteps.load(), 4, 32);
            const int idx = clampRange((int) ((float) e.x / (float) juce::jmax(1, getWidth()) * (float) N), 0, N - 1);
            const float v = 1.f - (float) e.y / (float) juce::jmax(1, getHeight());
            matrix.setSeqStep(slot, idx, v);
            repaint();
        }
        ModMatrix& matrix;
        int slot;
    };

    SourceEditorContent(ModMatrix& m, int slot_)
        : matrix(m), slot(slot_), st(m.sourceState(slot_))
    {
        const int cls = slotClassOf(slot);

        if (cls == SC_LFO)
        {
            addCombo("shape", "Shape", { "Sine", "Triangle", "Saw", "Square", "S+H", "S+H Glide" });
            addSlider("rate", "Rate Hz", 0.01, 40.0, 0.01, 0.4);
            addToggle("sync", "Tempo Sync");
            addDivCombo();
            addToggle("uni", "Unipolar");
            addSlider("glide", "Glide", 0.0, 1.0, 0.01, 1.0);
            addSlider("phase", "Phase", 0.0, 1.0, 0.01, 1.0);
        }
        else if (cls == SC_RND)
        {
            addCombo("kind", "Kind", { "Sample/Hold", "Smooth Random", "Drunk Walk", "Prob. Step", "Lorenz Chaos" });
            addSlider("rate", "Rate Hz", 0.01, 40.0, 0.01, 0.4);
            addToggle("sync", "Tempo Sync");
            addDivCombo();
            addSlider("p1", "Amount/Chance", 0.0, 1.0, 0.01, 1.0);
            addToggle("uni", "Unipolar");
            addTrigger("Reseed / Reset");
        }
        else if (cls == SC_ENV)
        {
            addSlider("dly", "Delay", 0.0, 10.0, 0.001, 0.35);
            addSlider("atk", "Attack", 0.0005, 10.0, 0.001, 0.3);
            addSlider("hold", "Hold", 0.0, 10.0, 0.001, 0.35);
            addSlider("dec", "Decay", 0.0005, 10.0, 0.001, 0.3);
            addSlider("sus", "Sustain", 0.0, 1.0, 0.001, 1.0);
            addSlider("rel", "Release", 0.0005, 10.0, 0.001, 0.3);
            addSlider("curve", "Curve", -1.0, 1.0, 0.01, 1.0);
            addToggle("loop", "Loop (cyclic)");
            addTrigger("Tap / Retrigger");
        }
        else if (cls == SC_SEQ)
        {
            addNumStepsCombo();
            addStepGridRow();
            addSlider("rate", "Rate Hz", 0.01, 40.0, 0.01, 0.4);
            addToggle("sync", "Tempo Sync");
            addDivCombo();
            addSlider("gate", "Gate", 0.05, 1.0, 0.01, 1.0);
            addSlider("slew", "Slew", 0.0, 1.0, 0.01, 1.0);
            addSlider("swing", "Swing", 0.0, 0.6, 0.01, 1.0);
            addCombo("dir", "Direction", { "Forward", "Reverse", "Ping-Pong", "Random" });
            addToggle("uni", "Unipolar");
            addTrigger("Restart");
        }

        setSize(330, layoutHeight());
    }

    int layoutHeight() const
    {
        int h = 8;
        for (const auto& r : rows)
            h += r.h;
        return h;
    }

    void resized() override
    {
        int y = 4;
        const int w = getWidth();
        for (const auto& r : rows)
        {
            if (r.l != nullptr)
                r.l->setBounds(6, y + 2, 88, r.h - 6);
            r.c->setBounds(98, y + 2, w - 104, r.h - 6);
            y += r.h;
        }
    }

private:
    void addRow(juce::Label* l, juce::Component* c, int h)
    {
        if (l != nullptr)
            addAndMakeVisible(l);
        addAndMakeVisible(c);
        rows.push_back({ l, c, h });
    }

    void addSlider(const juce::String& key, const juce::String& text,
                   double min, double max, double interval, double skew)
    {
        auto* l = new juce::Label();
        l->setText(text, juce::dontSendNotification);
        l->setFont(uiFont(11.f));
        l->setColour(juce::Label::textColourId, ui::dim());

        auto* s = new juce::Slider();
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
        s->setNormalisableRange(juce::NormalisableRange<double>(min, max, interval, skew));
        s->setValue((double) st.getProperty(key, min), juce::dontSendNotification);
        s->setColour(juce::Slider::backgroundColourId, ui::panelHi());
        s->setColour(juce::Slider::trackColourId, ui::line());
        s->setColour(juce::Slider::thumbColourId, slotColour(slot));
        s->setColour(juce::Slider::textBoxTextColourId, ui::txt());
        s->setColour(juce::Slider::textBoxBackgroundColourId, ui::panelHi());
        s->onValueChange = [this, key, s] { matrix.setSourceParam(slot, key, s->getValue()); };
        addRow(l, s, 26);
    }

    void addCombo(const juce::String& key, const juce::String& text, juce::StringArray items)
    {
        auto* l = new juce::Label();
        l->setText(text, juce::dontSendNotification);
        l->setFont(uiFont(11.f));
        l->setColour(juce::Label::textColourId, ui::dim());

        auto* cb = new juce::ComboBox();
        ui::styleCombo(*cb);
        for (int i = 0; i < items.size(); ++i)
            cb->addItem(items[i], i + 1);
        cb->setSelectedId((int) st.getProperty(key, 0) + 1, juce::dontSendNotification);
        cb->onChange = [this, key, cb] { matrix.setSourceParam(slot, key, cb->getSelectedId() - 1); };
        addRow(l, cb, 26);
    }

    void addDivCombo()
    {
        juce::StringArray divs;
        for (int i = 0; i < kNumSyncDivs; ++i)
            divs.add(kSyncDivNames[i]);
        addCombo("div", "Sync Div", divs);
    }

    void addNumStepsCombo()
    {
        auto* l = new juce::Label();
        l->setText("Steps", juce::dontSendNotification);
        l->setFont(uiFont(11.f));
        l->setColour(juce::Label::textColourId, ui::dim());

        auto* cb = new juce::ComboBox();
        ui::styleCombo(*cb);
        cb->addItem("16 Steps", 1);
        cb->addItem("32 Steps", 2);
        cb->setSelectedId((int) st.getProperty("numSteps", 16) >= 32 ? 2 : 1, juce::dontSendNotification);
        cb->onChange = [this, cb]
        {
            matrix.setSourceParam(slot, "numSteps", cb->getSelectedId() == 2 ? 32 : 16);
            if (stepGrid != nullptr)
                stepGrid->repaint();
        };
        addRow(l, cb, 26);
    }

    void addStepGridRow()
    {
        stepGrid = new StepGrid(matrix, slot);
        addRow(nullptr, stepGrid, 92);
    }

    void addToggle(const juce::String& key, const juce::String& text)
    {
        auto* tb = new juce::ToggleButton(text);
        ui::styleToggle(*tb);
        tb->setToggleState(bool(st.getProperty(key, false)), juce::dontSendNotification);
        tb->onClick = [this, key, tb] { matrix.setSourceParam(slot, key, tb->getToggleState()); };
        addRow(nullptr, tb, 24);
    }

    void addTrigger(const juce::String& text)
    {
        auto* b = new juce::TextButton(text);
        ui::styleButton(*b);
        b->onClick = [this] { matrix.retriggerSource(slot); };
        addRow(nullptr, b, 28);
    }

    struct RowSpec { juce::Label* l; juce::Component* c; int h; };

    ModMatrix& matrix;
    int slot;
    juce::ValueTree st;
    std::vector<RowSpec> rows;
    StepGrid* stepGrid = nullptr;
};

// ---------------------------------------------------------------------------
// ModPanel
// ---------------------------------------------------------------------------
ModPanel::ModPanel(ModMatrix& m, juce::ValueTree, juce::ValueTree matTree)
    : matrixRef(m)
{
    titleA = ui::makeLabel("MODULATION SOURCES  -  drag badges onto any knob", 10.5f, ui::dim());
    titleB = ui::makeLabel("MODULATION MATRIX", 10.5f, ui::dim());
    addAndMakeVisible(titleA.get());
    addAndMakeVisible(titleB.get());

    srcModel = std::make_unique<SourceListModel>(*this);
    sourceList.setModel(srcModel.get());
    sourceList.setRowHeight(26);
    ui::styleListBox(sourceList);
    addAndMakeVisible(sourceList);

    conns = std::make_unique<ConnectionList>(m, matTree);
    addAndMakeVisible(conns.get());
}

ModPanel::~ModPanel()
{
    sourceList.setModel(nullptr);
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced(6);
    titleA->setBounds(r.removeFromTop(18));
    const int listH = (int) ((float) r.getHeight() * 0.56f);
    sourceList.setBounds(r.removeFromTop(listH));
    r.removeFromTop(6);
    titleB->setBounds(r.removeFromTop(18));
    r.removeFromTop(2);
    conns->setBounds(r);
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
    g.setColour(ui::line());
    g.drawVerticalLine(getWidth() - 1, 0.f, (float) getHeight());
}

void ModPanel::openSourceEditor(int slot, juce::Component* near)
{
    if (! matrixRef.sourceState(slot).isValid())
        return;

    popup.reset();
    auto* content = new SourceEditorContent(matrixRef, slot);
    const int contentH = content->layoutHeight();

    auto dw = std::make_unique<juce::DialogWindow>(slotName(slot), ui::panelHi(), true);
    dw->setContentOwned(content, false);
    dw->setResizable(false, false);
    dw->setUsingNativeTitleBar(false);
    dw->centreAroundComponent(near, 330, contentH + 30);
    dw->setAlwaysOnTop(true);
    dw->enterModalState(false); // non-blocking: plugin-safe
    dw->setVisible(true);
    popup = std::move(dw);
}

} // namespace f64
