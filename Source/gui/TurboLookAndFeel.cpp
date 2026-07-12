#include "TurboLookAndFeel.h"

using namespace tt;

namespace tt::gui
{
    TurboLookAndFeel::TurboLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       theme::textHi);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, theme::bg0.withAlpha (0.6f));
        setColour (juce::Label::textColourId,               theme::textMid);
        setColour (juce::ComboBox::backgroundColourId,      theme::bg0);
        setColour (juce::ComboBox::textColourId,            theme::textHi);
        setColour (juce::ComboBox::outlineColourId,         theme::brassDark);
        setColour (juce::ComboBox::arrowColourId,           theme::brass);
        setColour (juce::PopupMenu::backgroundColourId,     theme::bg1);
        setColour (juce::PopupMenu::textColourId,           theme::textHi);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::rust);
        setColour (juce::PopupMenu::highlightedTextColourId,       theme::textHi);
        setColour (juce::TextButton::buttonColourId,        theme::bg2);
        setColour (juce::TextButton::textColourOffId,       theme::textMid);
        setColour (juce::TextButton::textColourOnId,        theme::textHi);
    }

    void TurboLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider& s)
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = juce::jmin (area.getWidth(), area.getHeight());   // keep it circular
        const auto bounds = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
        const auto centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);

        const bool bipolar = (s.getMinimum() < 0.0 && s.getMaximum() > 0.0);
        const auto accent  = s.findColour (juce::Slider::thumbColourId, true);
        const auto arcCol  = accent.isTransparent() ? theme::tubeGlow : accent;

        // Outer machined ring.
        {
            juce::ColourGradient grad (theme::brass.brighter (0.2f), centre.x, bounds.getY(),
                                       theme::brassDark.darker (0.4f), centre.x, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillEllipse (bounds);
            g.setColour (theme::bg0);
            g.fillEllipse (bounds.reduced (radius * 0.14f));
        }

        // Knob body with a soft top-light.
        const auto body = bounds.reduced (radius * 0.20f);
        {
            juce::ColourGradient grad (theme::bg2.brighter (0.25f), centre.x, body.getY(),
                                       theme::bg0, centre.x, body.getBottom(), false);
            g.setGradientFill (grad);
            g.fillEllipse (body);
            g.setColour (theme::bg0.darker (0.4f));
            g.drawEllipse (body, 1.2f);
        }

        // Value arc.
        const float arcR = radius * 0.90f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (theme::bg0.brighter (0.08f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path arc;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, from, angle, true);
        g.setColour (arcCol);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer.
        juce::Path pointer;
        const float pl = radius * 0.62f, pw = juce::jmax (2.0f, radius * 0.09f);
        pointer.addRoundedRectangle (-pw * 0.5f, -pl, pw, pl, pw * 0.5f);
        g.setColour (theme::textHi);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));

        // Centre cap with a faint tube glow.
        const auto cap = body.reduced (radius * 0.55f);
        g.setColour (arcCol.withAlpha (0.85f));
        g.fillEllipse (cap);
        g.setColour (theme::bg0);
        g.drawEllipse (cap, 1.0f);
    }

    void TurboLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                             bool highlighted, bool /*down*/)
    {
        auto bounds = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        // A small "panel switch": rounded slot + travelling cap + label.
        const float switchW = juce::jmin (34.0f, bounds.getWidth() * 0.5f);
        auto sw = bounds.removeFromLeft (switchW).withSizeKeepingCentre (switchW, 18.0f);
        const auto accent = b.findColour (juce::TextButton::buttonOnColourId, true);
        const auto onCol  = accent.isTransparent() ? theme::rustBright : accent;

        g.setColour (theme::bg0);
        g.fillRoundedRectangle (sw, 10.0f);
        g.setColour ((on ? onCol : theme::brassDark).withAlpha (highlighted ? 1.0f : 0.85f));
        g.drawRoundedRectangle (sw.reduced (0.5f), 10.0f, 1.4f);

        auto cap = sw.reduced (2.5f).withWidth (sw.getHeight() - 5.0f);
        if (on) cap.setX (sw.getRight() - cap.getWidth() - 2.5f);
        g.setColour (on ? onCol : theme::steel.darker (0.2f));
        g.fillEllipse (cap);
        if (on) { g.setColour (onCol.withAlpha (0.35f)); g.fillEllipse (cap.expanded (2.0f)); }

        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? theme::textHi : theme::textMid);
            g.setFont (theme::font (13.0f, on));
            g.drawText (b.getButtonText(), bounds.withTrimmedLeft (6.0f),
                        juce::Justification::centredLeft, true);
        }
    }

    void TurboLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                 const juce::Colour& /*bg*/, bool highlighted, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        auto base = on ? theme::rust : theme::bg2;
        if (down) base = base.darker (0.2f);
        else if (highlighted) base = base.brighter (0.12f);

        juce::ColourGradient grad (base.brighter (0.18f), r.getX(), r.getY(),
                                   base.darker (0.25f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour ((on ? theme::brass : theme::brassDark).withAlpha (0.8f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void TurboLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId
                                                      : juce::TextButton::textColourOffId));
        g.setFont (theme::font (14.0f, b.getToggleState()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }

    void TurboLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
        g.setColour (theme::bg0);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 3.0f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (theme::brass);
        g.strokePath (arrow, juce::PathStrokeType (1.6f));
    }

    void TurboLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void TurboLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        g.setColour (theme::bg1);
        g.fillRect (0, 0, width, height);
        g.setColour (theme::brassDark.withAlpha (0.6f));
        g.drawRect (0, 0, width, height, 1);
    }
}
