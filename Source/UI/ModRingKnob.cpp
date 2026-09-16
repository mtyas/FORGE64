#include "ModRingKnob.h"

namespace f64 {

ModRingKnob::ModRingKnob(juce::StringRef destId, juce::StringRef labelText,
                         Services& svcs, bool chipMode)
    : dest(destId), label(labelText), services(svcs), chip(chipMode)
{
    services.registerKnob(this);
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    if (chip)
        setRange(0.0, 1.0, 0.01);
    setTooltip(destId);
}

ModRingKnob::~ModRingKnob()
{
    services.unregisterKnob(this);
}

void ModRingKnob::paint(juce::Graphics& g)
{
    using namespace ui;
    auto rc = getLocalBounds().toFloat();

    const float knobSize = chip
        ? juce::jmin(rc.getWidth(), rc.getHeight()) - 2.f
        : juce::jmin(rc.getWidth() - 2.f, rc.getHeight() - 15.f);

    auto kr = chip ? rc.withSizeKeepingCentre(knobSize, knobSize)
                   : rc.removeFromTop(knobSize + 2.f).withSizeKeepingCentre(knobSize, knobSize);

    const float radius = kr.getWidth() * 0.5f;
    const auto centre = kr.getCentre();

    auto ang = [](float v)
    {
        return (v * 270.f - 225.f) * juce::MathConstants<float>::pi / 180.f;
    };

    // body
    g.setColour(chip ? panelHi() : panelHi().overlaidWith(panel().withAlpha(0.4f)));
    g.fillEllipse(kr.reduced(4.f));
    g.setColour(line());
    g.drawEllipse(kr.reduced(4.f), 1.f);

    const float v01 = chip ? 0.5f : (float) valueToProportionOfLength(getValue());

    // value arc
    {
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius - 2.f, radius - 2.f, 0.f,
                          ang(0.f), ang(juce::jmax(0.004f, v01)), false);
        g.setColour(chip ? dim() : accent());
        g.strokePath(arc, juce::PathStrokeType(2.4f));
    }

    // modulation rings
    float totalOff = 0.f;
    if (auto* m = services.matrix())
    {
        const auto rings = m->ringsFor(dest);
        int k = 0;
        for (const auto& ring : rings)
        {
            const float rad = radius - 6.f - (float) k * 3.1f;
            if (rad < 5.f)
                break;

            const float sweep = ring.amount * ModMatrix::shapeCurve(ring.now, ring.curve);
            totalOff += sweep;

            const float a0 = ang(v01);
            const float a1 = ang(juce::limitRange(v01 + sweep, 0.f, 1.f));
            if (std::abs(a1 - a0) > 0.0005f)
            {
                juce::Path p;
                p.addCentredArc(centre.x, centre.y, rad, rad, 0.f,
                                juce::jmin(a0, a1), juce::jmax(a0, a1), false);
                g.setColour(slotColour(ring.slot).withAlpha(0.9f));
                g.strokePath(p, juce::PathStrokeType(2.2f));
            }
            ++k;
        }

        if (! rings.empty())
        {
            const float ae = ang(juce::limitRange(v01 + totalOff, 0.f, 1.f));
            const float pr = radius - 6.f;
            g.setColour(txt());
            g.fillEllipse(centre.x + std::cos(ae) * pr - 2.f,
                          centre.y + std::sin(ae) * pr - 2.f, 4.f, 4.f);
        }
    }

    // pointer
    if (! chip)
    {
        const float pa = ang(v01);
        g.setColour(txt());
        g.drawLine(centre.x + std::cos(pa) * radius * 0.30f,
                   centre.y + std::sin(pa) * radius * 0.30f,
                   centre.x + std::cos(pa) * radius * 0.68f,
                   centre.y + std::sin(pa) * radius * 0.68f, 2.f);
    }

    // captions
    g.setFont(uiFont(chip ? 8.5f : 10.f));
    if (chip)
    {
        g.setColour(txt());
        g.drawText(label, kr, juce::Justification::centred);
    }
    else
    {
        g.setColour(dim());
        g.drawText(label, rc.removeFromBottom(14.f), juce::Justification::centred);
    }
}

void ModRingKnob::mouseDown(const juce::MouseEvent& e)
{
    if (chip)
    {
        if (e.mods.isLeftButtonDown())
            showConnMenu();
        return;
    }
    Slider::mouseDown(e);
}

void ModRingKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (chip)
        return;
    Slider::mouseDrag(e);
}

void ModRingKnob::mouseUp(const juce::MouseEvent& e)
{
    if (chip)
        return;
    if (e.mods.isPopupMenu())
    {
        showConnMenu();
        return;
    }
    Slider::mouseUp(e);
}

void ModRingKnob::showConnMenu()
{
    auto* m = services.matrix();
    if (m == nullptr)
        return;

    juce::PopupMenu menu;
    const auto rings = m->ringsFor(dest);
    if (rings.empty())
    {
        menu.addItem(1, "No modulations - drag a source badge here", false, false);
    }
    else
    {
        int itemId = 100;
        for (const auto& r : rings)
            menu.addItem(itemId++, "Remove: " + slotName(r.slot)
                + "  (x" + juce::String(r.amount, 2) + ")", true, false);
    }

    juce::Component::SafePointer<ModRingKnob> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe, m](int result)
    {
        if (safe == nullptr)
            return;
        if (result >= 100)
        {
            const int idx = result - 100;
            const auto rr = m->ringsFor(safe->dest);
            if (idx < (int) rr.size())
                m->removeConnection(rr[(size_t) idx].id);
        }
    });
}

bool ModRingKnob::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith("f64mod:");
}

void ModRingKnob::itemDropped(const SourceDetails& details)
{
    const int slot = details.description.toString()
                         .fromFirstOccurrenceOf("f64mod:", false, false)
                         .getIntValue();
    services.connectFromDrag(slot, dest);
}

} // namespace f64
