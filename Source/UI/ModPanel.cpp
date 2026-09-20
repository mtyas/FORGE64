#include "ModPanel.h"
#include "ModRingKnob.h"
#include "../PluginProcessor.h"

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
        amount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        amount.setRange(-1.0, 1.0, 0.005);
        amount.setDoubleClickReturnValue(true, 0.0);
        amount.setValue((double) conn.getProperty("amount", 0.5), juce::dontSendNotification);
        amount.textFromValueFunction = [](double v)
        {
            if (std::abs(v) < 0.01) return juce::String("0%");
            return (v > 0 ? "+" : "") + juce::String((int) std::round(v * 100.0)) + "%";
        };
        amount.valueFromTextFunction = [](const juce::String& text)
        {
            auto t = text.trim().dropLastCharacters(text.endsWith("%") ? 1 : 0);
            return juce::jlimit(-1.0, 1.0, t.getDoubleValue() * 0.01);
        };
        amount.onValueChange = [this]
        {
            double v = amount.getValue();
            if (std::abs(v) < 0.02)
                v = 0.0;
            tree.setProperty("amount", v, nullptr);
        };
        amount.setColour(juce::Slider::backgroundColourId, ui::panelHi());
        amount.setColour(juce::Slider::trackColourId, ui::line());
        amount.setColour(juce::Slider::thumbColourId, slotColour(slot));
        amount.setColour(juce::Slider::textBoxTextColourId, ui::txt());
        amount.setColour(juce::Slider::textBoxBackgroundColourId, ui::panelHi());
        amount.setColour(juce::Slider::textBoxOutlineColourId, ui::line());
        amount.setTooltip("Modulation depth (double-click to reset to 0%)");
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
        mute.setBounds(r.removeFromRight(24));
        inv.setBounds(r.removeFromRight(34));
        amount.setBounds(r.removeFromRight(108));
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
            {
                juce::Image snap(juce::Image::ARGB, 28, 28, true);
                {
                    juce::Graphics g(snap);
                    g.setColour(slotColour(slot));
                    g.fillEllipse(2.f, 2.f, 24.f, 24.f);
                    g.setColour(juce::Colours::black.withAlpha(0.85f));
                    g.setFont(uiFont(9.f, true));
                    g.drawText(slotShortName(slot), 2, 2, 24, 24, juce::Justification::centred);
                }
                c->startDragging("f64mod:" + juce::String(slot), this, juce::ScaledImage(snap), false, nullptr);
            }
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
        if (row >= 0 && row < (int) rows.size() && ! rows[(size_t) row].header)
        {
            auto* sr = dynamic_cast<SourceRow*>(existing);
            if (sr == nullptr)
            {
                delete existing;
                sr = new SourceRow(owner);
            }
            sr->setSlot(rows[(size_t) row].slot);
            return sr;
        }
        delete existing;
        return nullptr;
    }

private:
    struct RowInfo { bool header; int slot; juce::String title; };
    std::vector<RowInfo> rows;
    ModPanel& owner;
};

// ---------------------------------------------------------------------------
// ModPanel - Visual Waveform Displays
// ---------------------------------------------------------------------------
class LFOResponseView : public juce::Component, private juce::Timer
{
public:
    LFOResponseView(ModMatrix& m, int s, juce::ValueTree tree)
        : matrix(m), slot(s), st(tree)
    {
        startTimerHz(30);
    }
    ~LFOResponseView() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.f);
        g.setColour(ui::panelHi());
        g.fillRoundedRectangle(bounds, 4.f);
        g.setColour(ui::line());
        g.drawRoundedRectangle(bounds, 4.f, 1.f);

        const int shape = (int) st.getProperty("shape", 0);
        const bool uni = bool(st.getProperty("uni", false));
        const float phaseOffset = (float) (double) st.getProperty("phase", 0.0);
        const auto col = slotColour(slot);

        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        const float midY = bounds.getY() + h * 0.5f;

        if (! uni)
        {
            g.setColour(ui::line().withAlpha(0.4f));
            g.drawHorizontalLine((int) midY, bounds.getX() + 4.f, bounds.getRight() - 4.f);
        }

        juce::Path p;
        const int numPts = (int) w;
        for (int i = 0; i < numPts; ++i)
        {
            float t = (float) i / (float) numPts + phaseOffset;
            t = t - std::floor(t);
            float v = 0.f;
            switch (shape)
            {
                case 0: v = std::sin(t * juce::MathConstants<float>::twoPi); break;
                case 1: v = 1.0f - 4.0f * std::abs(std::round(t - 0.25f) - (t - 0.25f)); break;
                case 2: v = 1.0f - 2.0f * t; break;
                case 3: v = t < 0.5f ? 1.0f : -1.0f; break;
                case 4:
                {
                    int step = (int) (t * 8.0f);
                    static const float shVals[8] = { 0.8f, -0.4f, 0.6f, -0.9f, 0.2f, 0.7f, -0.5f, 0.1f };
                    v = shVals[step % 8];
                    break;
                }
                case 5:
                {
                    float pos = t * 8.0f;
                    int s0 = (int) pos;
                    int s1 = (s0 + 1) % 8;
                    float frac = pos - (float) s0;
                    static const float shVals[8] = { 0.8f, -0.4f, 0.6f, -0.9f, 0.2f, 0.7f, -0.5f, 0.1f };
                    v = shVals[s0 % 8] * (1.f - frac) + shVals[s1] * frac;
                    break;
                }
                default: v = std::sin(t * juce::MathConstants<float>::twoPi); break;
            }

            float py = uni ? (bounds.getBottom() - 6.f - juce::jlimit(0.f, 1.f, 0.5f + 0.5f * v) * (h - 12.f))
                           : (midY - v * (h * 0.42f));
            float px = bounds.getX() + (float) i;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }

        juce::Path fillP = p;
        fillP.lineTo(bounds.getRight(), uni ? bounds.getBottom() - 6.f : midY);
        fillP.lineTo(bounds.getX(), uni ? bounds.getBottom() - 6.f : midY);
        fillP.closeSubPath();
        g.setColour(col.withAlpha(0.18f));
        g.fillPath(fillP);

        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float liveVal = matrix.sourceAverage(slot);
        float beadY = uni ? (bounds.getBottom() - 6.f - liveVal * (h - 12.f))
                          : (midY - liveVal * (h * 0.42f));
        float beadX = bounds.getX() + bounds.getWidth() * 0.5f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(beadX - 4.f, beadY - 4.f, 8.f, 8.f);
        g.setColour(col.withAlpha(0.6f));
        g.drawEllipse(beadX - 6.f, beadY - 6.f, 12.f, 12.f, 1.5f);

        g.setColour(ui::dim());
        g.setFont(uiFont(9.5f, true));
        static const char* shapeNames[] = { "SINE", "TRIANGLE", "SAW", "SQUARE", "S+H", "S+H GLIDE" };
        juce::String shapeStr = (shape >= 0 && shape < 6) ? shapeNames[shape] : "LFO";
        g.drawText(shapeStr + (uni ? " [0..1]" : " [-1..+1]"), (int) bounds.getX() + 6, (int) bounds.getY() + 4, 160, 12, juce::Justification::left);
    }

private:
    void timerCallback() override { repaint(); }
    ModMatrix& matrix;
    int slot;
    juce::ValueTree st;
};

class EnvResponseView : public juce::Component, private juce::Timer
{
public:
    EnvResponseView(ModMatrix& m, int s, juce::ValueTree tree)
        : matrix(m), slot(s), st(tree)
    {
        startTimerHz(30);
    }
    ~EnvResponseView() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.f);
        g.setColour(ui::panelHi());
        g.fillRoundedRectangle(bounds, 4.f);
        g.setColour(ui::line());
        g.drawRoundedRectangle(bounds, 4.f, 1.f);

        const float dly  = (float) (double) st.getProperty("dly", 0.0);
        const float atk  = (float) (double) st.getProperty("atk", 0.01);
        const float hold = (float) (double) st.getProperty("hold", 0.0);
        const float dec  = (float) (double) st.getProperty("dec", 0.3);
        const float sus  = (float) (double) st.getProperty("sus", 0.5);
        const float rel  = (float) (double) st.getProperty("rel", 0.1);
        const auto col   = slotColour(slot);

        const float totalTime = juce::jmax(0.01f, dly + atk + hold + dec + 0.2f + rel);
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        const float bX = bounds.getX();
        const float bY = bounds.getY();
        const float baseline = bY + h - 8.f;
        const float peakY = bY + 12.f;
        const float susY = baseline - sus * (baseline - peakY);

        const float xDly = bX + (dly / totalTime) * w;
        const float xAtk = xDly + (atk / totalTime) * w;
        const float xHld = xAtk + (hold / totalTime) * w;
        const float xDec = xHld + (dec / totalTime) * w;
        const float xSus = xDec + (0.2f / totalTime) * w;
        const float xRel = juce::jmin(bX + w, xSus + (rel / totalTime) * w);

        juce::Path p;
        p.startNewSubPath(bX, baseline);
        p.lineTo(xDly, baseline);
        p.lineTo(xAtk, peakY);
        p.lineTo(xHld, peakY);
        p.lineTo(xDec, susY);
        p.lineTo(xSus, susY);
        p.lineTo(xRel, baseline);
        if (xRel < bX + w)
            p.lineTo(bX + w, baseline);

        juce::Path fillP = p;
        fillP.lineTo(bX + w, baseline);
        fillP.lineTo(bX, baseline);
        fillP.closeSubPath();
        g.setColour(col.withAlpha(0.2f));
        g.fillPath(fillP);

        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setFont(uiFont(8.5f, true));
        auto drawStage = [&](float x, const char* label)
        {
            g.setColour(ui::dim().withAlpha(0.5f));
            g.drawVerticalLine((int) x, peakY, baseline);
            g.setColour(ui::txt().withAlpha(0.7f));
            g.drawText(label, (int) x - 8, (int) baseline + 1, 16, 8, juce::Justification::centred);
        };
        if (dly > 0.001f) drawStage(xDly, "D");
        drawStage(xAtk, "A");
        if (hold > 0.001f) drawStage(xHld, "H");
        drawStage(xDec, "D");
        drawStage(xSus, "S");
        drawStage(xRel, "R");

        float liveVal = matrix.sourceAverage(slot);
        if (liveVal > 0.001f)
        {
            float curY = baseline - liveVal * (baseline - peakY);
            g.setColour(juce::Colours::white);
            g.fillEllipse(bX + w * 0.5f - 4.f, curY - 4.f, 8.f, 8.f);
            g.setColour(col.withAlpha(0.7f));
            g.drawEllipse(bX + w * 0.5f - 6.f, curY - 6.f, 12.f, 12.f, 1.5f);
        }

        g.setColour(ui::dim());
        g.setFont(uiFont(9.5f, true));
        g.drawText("DAHDSR ENVELOPE CURVE", (int) bX + 6, (int) bY + 4, 180, 12, juce::Justification::left);
    }

private:
    void timerCallback() override { repaint(); }
    ModMatrix& matrix;
    int slot;
    juce::ValueTree st;
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

    SourceEditorContent(ModMatrix& m, int slot_, Forge64Processor* proc)
        : matrix(m), slot(slot_), st(m.sourceState(slot_))
    {
        const int cls = slotClassOf(slot);

        if (cls == SC_LFO)
        {
            addRow(nullptr, new LFOResponseView(matrix, slot, st), 76);
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
            addRow(nullptr, new EnvResponseView(matrix, slot, st), 84);
            addEnvTriggerRow(proc);
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

    void addEnvTriggerRow(Forge64Processor* proc)
    {
        auto* l = new juce::Label();
        l->setText("Trigger Src", juce::dontSendNotification);
        l->setFont(uiFont(11.f));
        l->setColour(juce::Label::textColourId, ui::dim());

        class PadLearner : public juce::Component, private juce::Timer
        {
        public:
            PadLearner(Forge64Processor* p, ModMatrix& m, int sl, juce::ValueTree& tree)
                : proc(p), matrix(m), slot(sl), st(tree)
            {
                ui::styleCombo(combo);
                combo.addItem("All Pads (Omni)", 1);
                for (int i = 0; i < kNumPads; ++i)
                {
                    const int b = i / kPadsPerBank;
                    const char bc = (char) ('A' + b);
                    const juce::String name = "Pad " + juce::String::charToString(bc)
                                            + juce::String::formatted("%02d", (i % kPadsPerBank) + 1);
                    combo.addItem(name, i + 2);
                }
                combo.addItem("MIDI Note Omni", kNumPads + 2);

                int curPad = (int) st.getProperty("trigPad", -1);
                if (curPad >= 0 && curPad < kNumPads)
                    combo.setSelectedId(curPad + 2, juce::dontSendNotification);
                else if (curPad == -2)
                    combo.setSelectedId(kNumPads + 2, juce::dontSendNotification);
                else
                    combo.setSelectedId(1, juce::dontSendNotification);

                combo.onChange = [this]
                {
                    int id = combo.getSelectedId();
                    int pVal = -1;
                    if (id == 1) pVal = -1;
                    else if (id == kNumPads + 2) pVal = -2;
                    else pVal = id - 2;
                    matrix.setSourceParam(slot, "trigPad", pVal);
                };
                addAndMakeVisible(combo);

                learnBtn.setButtonText("LEARN");
                ui::styleButton(learnBtn);
                learnBtn.setTooltip("Touch or hit a pad to assign as envelope trigger");
                learnBtn.onClick = [this]
                {
                    learning = ! learning;
                    if (learning)
                    {
                        learnBtn.setButtonText("HIT PAD...");
                        learnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF6600));
                        if (proc) lastPad = proc->getLastTriggeredPad();
                        startTimerHz(30);
                    }
                    else
                    {
                        stopLearning();
                    }
                };
                addAndMakeVisible(learnBtn);
            }

            ~PadLearner() override { stopTimer(); }

            void stopLearning()
            {
                learning = false;
                stopTimer();
                learnBtn.setButtonText("LEARN");
                learnBtn.setColour(juce::TextButton::buttonColourId, ui::panelHi());
            }

            void timerCallback() override
            {
                if (! learning || ! proc) return;
                int current = proc->getLastTriggeredPad();
                if (current >= 0 && current < kNumPads && current != lastPad)
                {
                    combo.setSelectedId(current + 2, juce::sendNotificationSync);
                    stopLearning();
                }
            }

            void resized() override
            {
                auto r = getLocalBounds();
                learnBtn.setBounds(r.removeFromRight(64));
                r.removeFromRight(4);
                combo.setBounds(r);
            }

        private:
            Forge64Processor* proc;
            ModMatrix& matrix;
            int slot;
            juce::ValueTree& st;
            juce::ComboBox combo;
            juce::TextButton learnBtn;
            bool learning = false;
            int lastPad = -1;
        };

        auto* learner = new PadLearner(proc, matrix, slot, st);
        addRow(l, learner, 28);
    }

    struct RowSpec { juce::Label* l; juce::Component* c; int h; };

    ModMatrix& matrix;
    int slot;
    juce::ValueTree st;
    std::vector<RowSpec> rows;
    StepGrid* stepGrid = nullptr;
};

// ---------------------------------------------------------------------------
// ModPanel - In-window Source Inspector Overlay
// ---------------------------------------------------------------------------
class ModPanel::SourceInspectorOverlay : public juce::Component
{
public:
    SourceInspectorOverlay(ModPanel& o, ModMatrix& m, int slot_, Forge64Processor* proc)
        : owner(o), matrix(m), slot(slot_)
    {
        setWantsKeyboardFocus(true);

        backBtn.setButtonText("< BACK");
        backBtn.setTooltip("Close inspector");
        ui::styleButton(backBtn);
        backBtn.onClick = [this] { owner.closeInspector(); };
        addAndMakeVisible(backBtn);

        badge.setSlot(slot);
        addAndMakeVisible(badge);

        title.setText(slotName(slot), juce::dontSendNotification);
        title.setFont(uiFont(13.f, true));
        title.setColour(juce::Label::textColourId, ui::txt());
        addAndMakeVisible(title);

        const auto st = matrix.sourceState(slot);
        if (st.isValid())
        {
            ui::styleToggle(enableToggle);
            enableToggle.setButtonText("ON");
            enableToggle.setToggleState(bool(st.getProperty("enabled", false)), juce::dontSendNotification);
            enableToggle.onClick = [this]
            {
                matrix.setSourceParam(slot, "enabled", enableToggle.getToggleState());
            };
            addAndMakeVisible(enableToggle);
        }

        closeBtn.setButtonText(juce::String::charToString(0x00D7)); // ×
        ui::styleButton(closeBtn);
        closeBtn.onClick = [this] { owner.closeInspector(); };
        addAndMakeVisible(closeBtn);

        content = std::make_unique<SourceEditorContent>(matrix, slot, proc);
        viewport = std::make_unique<juce::Viewport>();
        viewport->setViewedComponent(content.get(), false);
        viewport->setScrollBarsShown(true, false);
        addAndMakeVisible(viewport.get());
    }

    ~SourceInspectorOverlay() override = default;

    void resized() override
    {
        auto r = getLocalBounds();
        auto topBar = r.removeFromTop(36).reduced(6, 4);
        backBtn.setBounds(topBar.removeFromLeft(64));
        topBar.removeFromLeft(6);
        badge.setBounds(topBar.removeFromLeft(22));
        topBar.removeFromLeft(6);
        closeBtn.setBounds(topBar.removeFromRight(26));
        topBar.removeFromRight(6);
        enableToggle.setBounds(topBar.removeFromRight(48));
        title.setBounds(topBar);

        r.removeFromTop(2);
        viewport->setBounds(r.reduced(4, 2));
        if (content != nullptr)
            content->setSize(viewport->getMaximumVisibleWidth(), content->layoutHeight());
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ui::panel().darker(0.18f));
        g.setColour(ui::panelHi());
        g.fillRect(0, 0, getWidth(), 36);
        g.setColour(slotColour(slot));
        g.fillRect(0, 34, getWidth(), 2);
        g.setColour(ui::line());
        g.drawRect(getLocalBounds(), 1);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            owner.closeInspector();
            return true;
        }
        return false;
    }

private:
    ModPanel& owner;
    ModMatrix& matrix;
    int slot = 0;
    juce::TextButton backBtn;
    SourceRow::Badge badge;
    juce::Label title;
    juce::ToggleButton enableToggle;
    juce::TextButton closeBtn;
    std::unique_ptr<SourceEditorContent> content;
    std::unique_ptr<juce::Viewport> viewport;
};

// ---------------------------------------------------------------------------
// ModPanel
// ---------------------------------------------------------------------------
ModPanel::ModPanel(ModMatrix& m, juce::ValueTree, juce::ValueTree matTree, Forge64Processor* proc)
    : matrixRef(m), procPtr(proc)
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
    inspector.reset();
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

    if (inspector != nullptr)
        inspector->setBounds(getLocalBounds());
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg());
    g.setColour(ui::line());
    g.drawVerticalLine(getWidth() - 1, 0.f, (float) getHeight());
}

void ModPanel::openSourceEditor(int slot, juce::Component* /*near*/)
{
    if (! matrixRef.sourceState(slot).isValid())
        return;

    inspector = std::make_unique<SourceInspectorOverlay>(*this, matrixRef, slot, procPtr);
    addAndMakeVisible(inspector.get());
    inspector->setBounds(getLocalBounds());
    inspector->toFront(true);
    inspector->grabKeyboardFocus();
}

void ModPanel::closeInspector()
{
    inspector.reset();
}

} // namespace f64
