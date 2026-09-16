#pragma once
#include <JuceHeader.h>

namespace f64::ui {

inline juce::Colour bg()      { return juce::Colour(0xFF15171C); }
inline juce::Colour panel()   { return juce::Colour(0xFF1E222A); }
inline juce::Colour panelHi() { return juce::Colour(0xFF272C37); }
inline juce::Colour accent()  { return juce::Colour(0xFF35C4F0); }
inline juce::Colour txt()     { return juce::Colour(0xFFE9ECF2); }
inline juce::Colour dim()     { return juce::Colour(0xFF8A93A5); }
inline juce::Colour line()    { return juce::Colour(0xFF333A47); }

inline void styleButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, panelHi());
    b.setColour(juce::TextButton::buttonOnColourId, accent().darker(0.45f));
    b.setColour(juce::TextButton::textColourOffId, txt());
    b.setColour(juce::TextButton::textColourOnId, accent());
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
    c.setColour(juce::ComboBox::arrowColourId, dim());
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

} // namespace f64::ui
