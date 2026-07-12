#pragma once

#include "DspCommon.h"
#include <array>

namespace cfm::dsp
{
    /**
        Mid/Side stereo finishing: Mono Maker + Width (64-bit).

        Mono Maker high-passes the Side channel so everything below the chosen
        frequency collapses to mono — tight, centred, phase-solid low end.
        Width then scales the remaining Side energy (0% mono … 200% wide).
        Order matters: we mono the lows *before* widening so we never widen
        bass we intend to keep centred.
    */
    class StereoModule
    {
    public:
        void prepare (double sr, int /*numChannels*/)
        {
            sampleRate = sr;
            reset();
            updateFilter();
        }

        void reset() noexcept { for (auto& f : sideHp) f.reset(); }

        void setParams (double widthPct, double monoFreqHz) noexcept
        {
            width    = juce::jlimit (0.0, 2.0, widthPct * 0.01);
            monoFreq = monoFreqHz;
            monoOn   = monoFreqHz >= 1.0;
            if (monoOn && monoFreq != lastMono) { lastMono = monoFreq; updateFilter(); }
        }

        void process (double* const* data, int numChannels, int numSamples) noexcept
        {
            if (numChannels < 2) return; // stereo-only effect
            double* L = data[0];
            double* R = data[1];

            const bool doWidth = std::abs (width - 1.0) > 1.0e-6;
            if (! doWidth && ! monoOn) return;

            for (int n = 0; n < numSamples; ++n)
            {
                double mid  = 0.5 * (L[n] + R[n]);
                double side = 0.5 * (L[n] - R[n]);

                if (monoOn) side = sideHp[0].processSample (side); // remove lows from Side
                side *= width;

                L[n] = flushDenorm (mid + side);
                R[n] = flushDenorm (mid - side);
            }
            sideHp[0].snapToZero();
        }

    private:
        void updateFilter() noexcept
        {
            const double f = juce::jlimit (20.0, 400.0, monoFreq);
            auto c = juce::dsp::IIR::Coefficients<double>::makeHighPass (sampleRate, f, 0.707);
            for (auto& flt : sideHp) flt.coefficients = c;
        }

        double sampleRate = 44100.0;
        double width = 1.0, monoFreq = 0.0, lastMono = -1.0;
        bool   monoOn = false;
        std::array<juce::dsp::IIR::Filter<double>, 1> sideHp;
    };
}
