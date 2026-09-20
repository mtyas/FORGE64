#include "ModRingKnob.h"
#include "../Engine/MidiLearn.h"

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

    if (chip)
    {
        const float knobSize = juce::jmin(rc.getWidth(), rc.getHeight()) - 2.f;
        auto kr = rc.withSizeKeepingCentre(knobSize, knobSize);
        g.setGradientFill(juce::ColourGradient(panelHi().brighter(0.15f), kr.getX(), kr.getY(),
                                               juce::Colour(0xFF16100E), kr.getX(), kr.getBottom(), false));
        g.fillEllipse(kr.reduced(2.f));
        g.setColour(isDragOver ? accent() : line().brighter(0.2f));
        g.drawEllipse(kr.reduced(2.f), isDragOver ? 2.5f : 1.f);
        g.setFont(uiFont(8.5f, true));
        g.setColour(isDragOver ? accentHot() : txt());
        g.drawText(label, kr, juce::Justification::centred);
        return;
    }

    // Top area for rotary dial, bottom for label & numeric value
    const float labelH = (rc.getHeight() < 58.f) ? juce::jmin(18.f, rc.getHeight() * 0.36f) : 26.f;
    auto knobArea = rc.withTrimmedBottom(labelH);
    const float pad = (rc.getHeight() < 58.f) ? 3.f : 4.f;
    const float knobSize = juce::jmax(4.f, juce::jmin(knobArea.getWidth() - pad * 2.f, knobArea.getHeight() - pad * 2.f));
    auto kr = knobArea.withSizeKeepingCentre(knobSize, knobSize);

    const float radius = kr.getWidth() * 0.5f;
    const auto centre = kr.getCentre();

    // Standard rotary sweep: 7 o'clock (-135 deg) to 5 o'clock (+135 deg)
    const float startAng = -juce::MathConstants<float>::pi * 0.75f;
    const float endAng   =  juce::MathConstants<float>::pi * 0.75f;
    auto ang = [&](float norm)
    {
        return startAng + juce::jlimit(0.f, 1.f, norm) * (endAng - startAng);
    };

    const float v01 = (float) valueToProportionOfLength(getValue());

    // 1. Dark outer track groove (forged metal channel)
    {
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.f, startAng, endAng, true);
        g.setColour(juce::Colour(0xFF0C0908));
        g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 2. Active illuminated value arc (molten fire arc with heat bloom)
    const bool isBipolar = (getMinimum() < -0.01 && getMaximum() > 0.01) ||
                           dest.contains("pan") || dest.contains("tune") ||
                           dest.contains("lg") || dest.contains("mg") || dest.contains("hg");

    const float zeroNorm = isBipolar ? (float) valueToProportionOfLength(0.0) : 0.f;
    const float aFrom = ang(zeroNorm);
    const float aTo   = ang(v01);

    if (std::abs(aTo - aFrom) > 0.008f)
    {
        juce::Path valArc;
        valArc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.f,
                             juce::jmin(aFrom, aTo), juce::jmax(aFrom, aTo), true);

        // Radiant heat bloom
        g.setColour(accentGlow().withAlpha(0.38f));
        g.strokePath(valArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Molten wire core
        g.setColour(accent());
        g.strokePath(valArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Modulation rings
    float totalOff = 0.f;
    if (auto* m = services.matrix())
    {
        const auto rings = m->ringsFor(dest);
        int k = 0;
        for (const auto& ring : rings)
        {
            if (std::abs(ring.amount) < 0.005f)
                continue;

            const float rad = radius - 5.5f - (float) k * 2.8f;
            if (rad < 6.f)
                break;

            const float sweep = ring.amount * ModMatrix::shapeCurve(ring.now, ring.curve);
            totalOff += sweep;

            const float a0 = ang(v01);
            const float a1 = ang(clampRange(v01 + sweep, 0.f, 1.f));
            if (std::abs(a1 - a0) > 0.008f)
            {
                juce::Path p;
                p.addCentredArc(centre.x, centre.y, rad, rad, 0.f,
                                juce::jmin(a0, a1), juce::jmax(a0, a1), true);
                g.setColour(slotColour(ring.slot).withAlpha(0.92f));
                g.strokePath(p, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            ++k;
        }

        if (! rings.empty())
        {
            const float ae = ang(clampRange(v01 + totalOff, 0.f, 1.f));
            const float pr = radius - 5.5f;
            const float dotX = centre.x + std::sin(ae) * pr;
            const float dotY = centre.y - std::cos(ae) * pr;
            g.setColour(accentHot());
            g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);
        }
    }

    // 4. Inner knob cap: Machined forged steel dial with metallic bevel
    const float innerRad = radius - 5.5f;
    auto capBounds = kr.reduced(5.5f);

    // Bevel rim
    g.setColour(steel().darker(0.2f));
    g.fillEllipse(capBounds);
    g.setColour(juce::Colour(0xFF3F322D));
    g.drawEllipse(capBounds, 1.2f);

    // Inner dial face
    auto faceBounds = capBounds.reduced(2.0f);
    g.setGradientFill(juce::ColourGradient(panelHi().brighter(0.18f), centre.x, faceBounds.getY(),
                                           juce::Colour(0xFF140E0D), centre.x, faceBounds.getBottom(), false));
    g.fillEllipse(faceBounds);
    g.setColour(isDragOver ? accent() : line().brighter(0.15f));
    g.drawEllipse(faceBounds, isDragOver ? 2.0f : 1.0f);

    // 5. Hot metal pointer needle with glowing molten tip
    const float pa = ang(v01);
    const float pStartR = innerRad * 0.22f;
    const float pEndR   = innerRad * 0.82f;
    const float tipX = centre.x + std::sin(pa) * pEndR;
    const float tipY = centre.y - std::cos(pa) * pEndR;

    // Glowing tip bloom
    g.setColour(accentGlow().withAlpha(0.6f));
    g.fillEllipse(tipX - 3.f, tipY - 3.f, 6.f, 6.f);

    // Needle body
    g.setColour(accentHot());
    g.drawLine(centre.x + std::sin(pa) * pStartR,
               centre.y - std::cos(pa) * pStartR,
               tipX, tipY, 2.2f);

    // Center forged rivet
    g.setColour(panelHi().darker(0.35f));
    g.fillEllipse(centre.x - 3.f, centre.y - 3.f, 6.f, 6.f);
    g.setColour(line());
    g.drawEllipse(centre.x - 3.f, centre.y - 3.f, 6.f, 6.f, 1.f);

    // 6. Label & Value text
    auto labelRc = rc.removeFromBottom(labelH);
    if (labelH >= 20.f)
    {
        auto titleRc = labelRc.removeFromTop(labelH * 0.52f);
        g.setFont(uiFont(9.5f, true));
        g.setColour(dim().brighter(0.35f));
        g.drawText(label, titleRc, juce::Justification::centred);

        auto valRc = labelRc;
        g.setFont(uiFont(8.5f));
        g.setColour(isMouseOverOrDragging() ? accentHot() : dim());
        g.drawText(getTextFromValue(getValue()), valRc, juce::Justification::centred);
    }
    else
    {
        g.setFont(uiFont(8.5f, true));
        g.setColour(dim().brighter(0.35f));
        g.drawText(label, labelRc, juce::Justification::centred);
    }

    // 7. MIDI Learn pulsating aura or mapped CC dot
    if (services.midiLearn() != nullptr)
    {
        if (services.midiLearn()->isParamLearning(dest))
        {
            const float pulse = 0.5f + 0.5f * std::sin((float) juce::Time::getMillisecondCounter() * 0.009f);
            g.setColour(accentHot().withAlpha(0.35f + 0.55f * pulse));
            g.drawEllipse(kr.expanded(2.5f), 2.5f);
        }
        else if (services.midiLearn()->getBoundCC(dest) >= 0)
        {
            g.setColour(accentHot());
            g.fillEllipse(kr.getRight() - 5.f, kr.getY() + 2.f, 3.5f, 3.5f);
        }
    }

    if (! isEnabled())
    {
        g.setColour(ui::bg().withAlpha(0.62f));
        g.fillRoundedRectangle(rc, 4.f);
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

void ModRingKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    if (chip)
        return;
    const bool isBip = (getMinimum() < -0.01 && getMaximum() > 0.01) ||
                       dest.contains("pan") || dest.contains("tune") ||
                       dest.contains("lg") || dest.contains("mg") || dest.contains("hg");
    setValue(isBip ? 0.0 : getMinimum(), juce::sendNotificationAsync);
}

void ModRingKnob::showConnMenu()
{
    juce::PopupMenu menu;

    auto* ml = services.midiLearn();
    if (ml != nullptr)
    {
        const int boundCC = ml->getBoundCC(dest);
        if (boundCC >= 0)
        {
            menu.addItem(10, "MIDI Unlearn (Currently CC " + juce::String(boundCC) + ")");
            menu.addItem(11, "Re-learn MIDI CC...");
        }
        else
        {
            menu.addItem(11, "MIDI Learn (Move hardware knob/fader)");
        }
        menu.addSeparator();
    }

    menu.addItem(12, "Reset to Default Value");

    auto* m = services.matrix();
    if (m != nullptr)
    {
        const auto rings = m->ringsFor(dest);
        if (! rings.empty())
        {
            menu.addSeparator();
            juce::PopupMenu modMenu;
            int itemId = 100;
            for (const auto& r : rings)
            {
                modMenu.addItem(itemId++, "Remove: " + slotName(r.slot)
                    + " (" + juce::String((int) std::round(r.amount * 100.f)) + "%)", true, false);
            }
            menu.addSubMenu("Modulations (" + juce::String((int) rings.size()) + ")", modMenu);
        }
    }

    juce::Component::SafePointer<ModRingKnob> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe, m, ml](int result)
    {
        if (safe == nullptr)
            return;
        if (result == 10 && ml != nullptr)
        {
            ml->unbind(safe->dest);
            safe->repaint();
        }
        else if (result == 11 && ml != nullptr)
        {
            ml->startLearning(safe->dest);
            safe->repaint();
        }
        else if (result == 12)
        {
            const bool isBip = (safe->getMinimum() < -0.01 && safe->getMaximum() > 0.01) ||
                               safe->dest.contains("pan") || safe->dest.contains("tune") ||
                               safe->dest.contains("lg") || safe->dest.contains("mg") || safe->dest.contains("hg");
            safe->setValue(isBip ? 0.0 : safe->getDefaultValue(), juce::sendNotificationAsync);
        }
        else if (result >= 100 && m != nullptr)
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
    isDragOver = false;
    repaint();
    const int slot = details.description.toString()
                         .fromFirstOccurrenceOf("f64mod:", false, false)
                         .getIntValue();
    services.connectFromDrag(slot, dest);
}

} // namespace f64
