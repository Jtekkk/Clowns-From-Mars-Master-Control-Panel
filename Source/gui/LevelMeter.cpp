#include "LevelMeter.h"

using namespace tt;

namespace tt::gui
{
    float LevelMeter::dbToY (float db, juce::Rectangle<float> area) const
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return area.getBottom() - t * area.getHeight();
    }

    void LevelMeter::timerCallback()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            rmsDb[ch]  = juce::Decibels::gainToDecibels (meter.getRms (ch),  -100.0f);
            peakDb[ch] = juce::Decibels::gainToDecibels (meter.getPeak (ch), -100.0f);

            if (peakDb[ch] >= peakHold[ch]) { peakHold[ch] = peakDb[ch]; holdCount[ch] = 45; }
            else if (--holdCount[ch] <= 0)  { peakHold[ch] -= 1.2f; }
        }
        repaint();
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();

        // Scale gutter on the right.
        auto scale = area.removeFromRight (26.0f);
        auto bars  = area.reduced (2.0f);

        g.setColour (theme::bg0);
        g.fillRoundedRectangle (bars, 3.0f);

        const float gap = 3.0f;
        const float barW = (bars.getWidth() - gap) * 0.5f;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto col = bars.withWidth (barW).withX (bars.getX() + ch * (barW + gap));
            auto inner = col.reduced (1.0f);

            // Zoned gradient background (unlit).
            const float y0  = dbToY (0.0f, inner);
            const float y18 = dbToY (-18.0f, inner);
            g.setColour (theme::meterGreen.withAlpha (0.10f)); g.fillRect (inner.withTop (y18));
            g.setColour (theme::meterAmber.withAlpha (0.10f)); g.fillRect (inner.withTop (y0).withBottom (y18));
            g.setColour (theme::meterRed.withAlpha (0.12f));   g.fillRect (inner.withBottom (y0));

            // Lit RMS fill.
            const float yr = dbToY (rmsDb[ch], inner);
            juce::ColourGradient grad (theme::meterGreen, inner.getX(), inner.getBottom(),
                                       theme::meterRed, inner.getX(), inner.getY(), false);
            grad.addColour (0.72, theme::meterAmber);
            g.setGradientFill (grad);
            g.fillRect (inner.withTop (yr));

            // Peak-hold cap.
            const float yp = dbToY (juce::jlimit (minDb, maxDb, peakHold[ch]), inner);
            g.setColour (peakHold[ch] > 0.0f ? theme::meterRed : theme::textHi);
            g.fillRect (juce::Rectangle<float> (inner.getX(), yp - 1.5f, inner.getWidth(), 2.0f));
        }

        // dB ticks.
        g.setFont (theme::font (9.5f));
        g.setColour (theme::textLo);
        for (int db : { 6, 0, -6, -12, -18, -24, -36, -48, -60 })
        {
            const float y = dbToY ((float) db, bars);
            g.drawText (juce::String (db), scale.withY (y - 6.0f).withHeight (12.0f).reduced (2, 0),
                        juce::Justification::centredLeft);
            g.setColour (theme::bg2);
            g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
            g.setColour (theme::textLo);
        }
    }
}
