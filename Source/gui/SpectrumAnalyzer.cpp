#include "SpectrumAnalyzer.h"

using namespace cfm;

namespace cfm::gui
{
    SpectrumAnalyzer::SpectrumAnalyzer (cfm::dsp::AnalyzerFifo& fifoIn,
                                        std::function<double (double)> eqMagDbAt)
        : fifo (fifoIn), eqMagAt (std::move (eqMagDbAt))
    {
        display.fill (minDb);
        startTimerHz (30);
    }

    float SpectrumAnalyzer::freqToX (float hz, juce::Rectangle<float> a) const
    {
        const float t = std::log (hz / minHz) / std::log (maxHz / minHz);
        return a.getX() + juce::jlimit (0.0f, 1.0f, t) * a.getWidth();
    }

    float SpectrumAnalyzer::dbToY (float db, juce::Rectangle<float> a) const
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return a.getBottom() - t * a.getHeight();
    }

    void SpectrumAnalyzer::timerCallback()
    {
        if (fifo.read (fftData.data()))
        {
            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            const int numBins = fftSize / 2;
            const float norm  = 2.0f / (float) fftSize;

            for (size_t i = 0; i < display.size(); ++i)
            {
                const float t  = (float) i / (float) (display.size() - 1);
                const float hz = minHz * std::pow (maxHz / minHz, t);
                const int   bin = juce::jlimit (1, numBins - 1,
                                    (int) std::round (hz * (float) fftSize / (float) sampleRate));
                const float mag = fftData[(size_t) bin] * norm;
                const float db  = juce::Decibels::gainToDecibels (mag, minDb);

                // Rise fast, fall slow for a readable analyser.
                display[i] = (db > display[i]) ? db : display[i] + (db - display[i]) * 0.35f;
            }
        }
        repaint();
    }

    void SpectrumAnalyzer::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (theme::bg0);
        g.fillRoundedRectangle (area, 4.0f);

        auto plot = area.reduced (6.0f);

        // --- grid --------------------------------------------------------------
        g.setFont (theme::font (9.5f));
        for (float hz : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            const float x = freqToX (hz, plot);
            g.setColour (theme::bg2.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (theme::textLo);
            g.drawText (hz >= 1000.f ? juce::String (hz / 1000.f, 0) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 14.0f, plot.getBottom() - 12.0f, 28.0f, 12.0f),
                        juce::Justification::centred);
        }
        for (float db : { 0.f, -12.f, -24.f, -36.f, -48.f, -60.f, -72.f })
        {
            const float y = dbToY (db, plot);
            g.setColour (theme::bg2.withAlpha (0.6f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }

        // --- spectrum fill -----------------------------------------------------
        juce::Path spec;
        spec.startNewSubPath (plot.getX(), plot.getBottom());
        for (size_t i = 0; i < display.size(); ++i)
        {
            const float t  = (float) i / (float) (display.size() - 1);
            const float hz = minHz * std::pow (maxHz / minHz, t);
            spec.lineTo (freqToX (hz, plot), dbToY (display[i], plot));
        }
        spec.lineTo (plot.getRight(), plot.getBottom());
        spec.closeSubPath();

        juce::ColourGradient grad (theme::rust.withAlpha (0.55f), 0, plot.getY(),
                                   theme::rust.withAlpha (0.05f), 0, plot.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (spec);

        juce::Path line;
        for (size_t i = 0; i < display.size(); ++i)
        {
            const float t  = (float) i / (float) (display.size() - 1);
            const float hz = minHz * std::pow (maxHz / minHz, t);
            const auto  p  = juce::Point<float> (freqToX (hz, plot), dbToY (display[i], plot));
            if (i == 0) line.startNewSubPath (p); else line.lineTo (p);
        }
        g.setColour (theme::rustBright);
        g.strokePath (line, juce::PathStrokeType (1.3f));

        // --- EQ response overlay ----------------------------------------------
        if (eqMagAt)
        {
            juce::Path eqCurve;
            const float centreY = plot.getCentreY();          // 0 dB sits mid-height
            const float dbPerPx = plot.getHeight() / 80.0f;    // ±18 dB fills a comfy band
            bool started = false;
            for (float px = plot.getX(); px <= plot.getRight(); px += 2.0f)
            {
                const float tx = (px - plot.getX()) / plot.getWidth();
                const float hz = minHz * std::pow (maxHz / minHz, tx);
                const float db = (float) eqMagAt ((double) hz);
                const float y = centreY - juce::jlimit (-18.0f, 18.0f, db) * dbPerPx;
                if (! started) { eqCurve.startNewSubPath (px, y); started = true; }
                else            eqCurve.lineTo (px, y);
            }
            g.setColour (theme::clownTeal);
            g.strokePath (eqCurve, juce::PathStrokeType (1.8f));
            g.setColour (theme::textLo.withAlpha (0.5f));
            g.drawHorizontalLine ((int) centreY, plot.getX(), plot.getRight());
        }

        g.setColour (theme::brassDark.withAlpha (0.5f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
    }
}
