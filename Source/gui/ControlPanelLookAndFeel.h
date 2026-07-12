#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace cfm::gui
{
    /**
        Rusted-brass control styling: chunky knobs with a machined pointer and
        an amber value arc, toggle "switches" that read as panel hardware, and
        legible combo boxes. Everything is drawn, so there are no image assets
        to ship and it scales cleanly at any UI size.
    */
    class ControlPanelLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ControlPanelLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (14.0f); }
        juce::Font getLabelFont (juce::Label&) override { return theme::font (13.0f); }
        juce::Font getPopupMenuFont() override { return theme::font (14.0f); }

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    };
}
