#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    "Clowns From Mars" visual language.

    Weathered rust and Martian iron-oxide reds, brass hardware, warm amber
    tube-glow, and sparing splashes of clown paint (teal, signal red). Dark,
    high-contrast and legible — a control panel bolted together on Mars.
*/
namespace cfm::theme
{
    // Backgrounds — deep Martian dusk to rusted-panel browns.
    inline const juce::Colour bg0        { 0xff140b07 }; // deepest
    inline const juce::Colour bg1        { 0xff23130c }; // panel base
    inline const juce::Colour bg2        { 0xff321d12 }; // raised panel
    inline const juce::Colour panelEdge  { 0xff5a3a26 };

    // Metals.
    inline const juce::Colour brass      { 0xffd9a441 };
    inline const juce::Colour brassDark  { 0xff8a6520 };
    inline const juce::Colour steel      { 0xff8f8378 };

    // Accents.
    inline const juce::Colour rust       { 0xffc0532a }; // primary accent
    inline const juce::Colour rustBright { 0xffe0722f };
    inline const juce::Colour tubeGlow   { 0xffff8a3c }; // amber tube heater
    inline const juce::Colour clownTeal  { 0xff33a89e };
    inline const juce::Colour clownRed   { 0xffd23a2e };

    // Text.
    inline const juce::Colour textHi     { 0xfff0e3d2 };
    inline const juce::Colour textMid    { 0xffbfa993 };
    inline const juce::Colour textLo     { 0xff7c6653 };

    // Meters.
    inline const juce::Colour meterGreen { 0xff6fbf4b };
    inline const juce::Colour meterAmber { 0xffe0a92f };
    inline const juce::Colour meterRed   { 0xffd23a2e };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height,
                    bold ? juce::Font::bold : juce::Font::plain));
    }
}
