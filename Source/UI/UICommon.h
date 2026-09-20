#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"

namespace f64::ui {

// ---------------------------------------------------------------------------
// FORGE64 Visual Theme: Cast Iron, Fire, Smoke, and Molten Metal
// ---------------------------------------------------------------------------
inline juce::Colour bg()          { return juce::Colour(0xFF110D0C); } // Deep soot cast-iron anvil
inline juce::Colour panel()       { return juce::Colour(0xFF191312); } // Smoked forged iron plate
inline juce::Colour panelHi()     { return juce::Colour(0xFF261D1A); } // Heavy heated iron slab
inline juce::Colour panelHover()  { return juce::Colour(0xFF352622); } // Scorched bronze / furnace metal
inline juce::Colour accent()      { return juce::Colour(0xFFFF7300); } // Molten hot metal fire orange
inline juce::Colour accentGlow()  { return juce::Colour(0xFFFF3300); } // Incandescent red-orange heat bloom
inline juce::Colour accentHot()   { return juce::Colour(0xFFFFD166); } // White-hot furnace crucible core
inline juce::Colour ember()       { return juce::Colour(0xFFD62828); } // Smoldering crimson coal ember
inline juce::Colour smoke()       { return juce::Colour(0xFF6B5C56); } // Atmospheric ash smoke gray
inline juce::Colour steel()       { return juce::Colour(0xFF483C37); } // Weathered anvil steel
inline juce::Colour txt()         { return juce::Colour(0xFFF7ECE4); } // Hot ash silver-white
inline juce::Colour dim()         { return juce::Colour(0xFF9E8A80); } // Smoked slag steel gray
inline juce::Colour line()        { return juce::Colour(0xFF352723); } // Charcoal iron seam

inline void styleButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, panelHi());
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF702808));
    b.setColour(juce::TextButton::textColourOffId, txt());
    b.setColour(juce::TextButton::textColourOnId, accentHot());
}

inline void styleToggle(juce::ToggleButton& t)
{
    t.setColour(juce::ToggleButton::tickColourId, accent());
    t.setColour(juce::ToggleButton::textColourId, txt());
}

inline void styleCombo(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId, panelHi());
    c.setColour(juce::ComboBox::textColourId, txt());
    c.setColour(juce::ComboBox::arrowColourId, accent());
    c.setColour(juce::ComboBox::outlineColourId, line());
}

inline void styleListBox(juce::ListBox& l)
{
    l.setColour(juce::ListBox::backgroundColourId, panel());
    l.setColour(juce::ListBox::outlineColourId, line());
    l.setColour(juce::ListBox::textColourId, txt());
}

inline std::unique_ptr<juce::Label> makeLabel(const juce::String& text, float size = 13.f,
                                              juce::Colour c = {})
{
    auto l = std::make_unique<juce::Label>();
    l->setText(text, juce::dontSendNotification);
    l->setFont(uiFont(size));
    l->setColour(juce::Label::textColourId, c == juce::Colour() ? txt() : c);
    l->setJustificationType(juce::Justification::centredLeft);
    return l;
}

// Draws a dark forged iron plate with warm ambient furnace gradient and subtle hot seam
inline void drawForgedPlate(juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.f, bool glowing = false)
{
    // Smoked iron bevel gradient
    g.setGradientFill(juce::ColourGradient(panelHi().brighter(0.08f), r.getX(), r.getY(),
                                           panel().darker(0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);

    if (glowing)
    {
        // Molten ember rim glow
        g.setColour(accent().withAlpha(0.75f));
        g.drawRoundedRectangle(r, corner, 1.8f);
        g.setColour(accentGlow().withAlpha(0.25f));
        g.drawRoundedRectangle(r.expanded(1.5f), corner + 1.f, 2.0f);
    }
    else
    {
        // Forged seam line with warm ember tint on top edge
        g.setColour(line().withAlpha(0.55f));
        g.drawRoundedRectangle(r, corner, 1.0f);
        g.setColour(juce::Colour(0xFFFF6600).withAlpha(0.08f));
        g.drawHorizontalLine((int) r.getY(), r.getX() + corner, r.getRight() - corner);
    }
}

} // namespace f64::ui
