#pragma once

#include "DspCommon.h"
#include <array>

namespace cfm::dsp
{
    /**
        Mid/Side stereo finishing: Mono Maker + Width.

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

        void setParams (float widthPct, float monoFreqHz) noexcept
        {
            width    = juce::jlimit (0.0f, 2.0f, widthPct * 0.01f);
            monoFreq = monoFreqHz;
            monoOn   = monoFreqHz >= 1.0f;
            if (monoOn && monoFreq != lastMono) { lastMono = monoFreq; updateFilter(); }
        }

        void process (float* const* data, int numChannels, int numSamples) noexcept
        {
            if (numChannels < 2) return; // stereo-only effect
            float* L = data[0];
            float* R = data[1];

            const bool doWidth = std::abs (width - 1.0f) > 1.0e-4f;
            if (! doWidth && ! monoOn) return;

            for (int n = 0; n < numSamples; ++n)
            {
                float mid  = 0.5f * (L[n] + R[n]);
                float side = 0.5f * (L[n] - R[n]);

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
            const float f = juce::jlimit (20.0f, 400.0f, monoFreq);
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, 0.707f);
            for (auto& flt : sideHp) flt.coefficients = c;
        }

        double sampleRate = 44100.0;
        float  width = 1.0f, monoFreq = 0.0f, lastMono = -1.0f;
        bool   monoOn = false;
        std::array<juce::dsp::IIR::Filter<float>, 1> sideHp;
    };
}
