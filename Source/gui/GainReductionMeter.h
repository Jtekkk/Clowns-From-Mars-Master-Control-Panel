#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace cfm::gui
{
    class ControlPanelAudioProcessorAccess;

    /**
        Horizontal gain-reduction meter for the compressor. Reads the most
        recent block's peak reduction (a negative dB value) via a supplied
        callback, so it stays decoupled from the processor type.
    */
    class GainReductionMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit GainReductionMeter (std::function<float()> grSource) : getGr (std::move (grSource))
        {
            startTimerHz (30);
        }
        ~GainReductionMeter() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (theme::bg0);
            g.fillRoundedRectangle (r, 3.0f);

            auto inner = r.reduced (2.0f);
            const float maxGr = 24.0f;               // full scale
            const float t = juce::jlimit (0.0f, 1.0f, -smoothed / maxGr);
            auto fill = inner.withWidth (inner.getWidth() * t);

            juce::ColourGradient grad (theme::tubeGlow, inner.getX(), 0,
                                       theme::clownRed, inner.getRight(), 0, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 2.0f);

            g.setColour (theme::brassDark.withAlpha (0.6f));
            g.drawRoundedRectangle (inner, 2.0f, 1.0f);

            g.setFont (theme::font (10.0f, true));
            g.setColour (theme::textMid);
            g.drawText (juce::String (smoothed, 1) + " dB GR", inner, juce::Justification::centredRight);
        }

    private:
        void timerCallback() override
        {
            const float target = getGr ? getGr() : 0.0f;
            // Fast attack toward more reduction, slow release back to 0.
            if (target < smoothed) smoothed = target;
            else                   smoothed += 0.25f * (target - smoothed);
            repaint();
        }

        std::function<float()> getGr;
        float smoothed = 0.0f;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
    };
}
