#pragma once

#include "DspCommon.h"
#include <array>

namespace tt::dsp
{
    /**
        Mastering-grade five-band parametric EQ.

          band 0 : low shelf     band 1..3 : peak     band 4 : high shelf
          + variable High-Pass and Low-Pass (12 dB/oct, Butterworth)
          + AIR   : gentle high shelf around 15 kHz for open top end
          + TIGHT : low-shelf clean-up that de-muds the 250 Hz region

        Curves use the RBJ analog-matched biquad design and are clamped away
        from Nyquist so boosts stay clean at every sample rate. Q can follow
        the classic Proportional-Q law (broad for small moves, tighter for big
        ones) or stay Constant-Q, switchable globally.
    */
    class EqualizerModule
    {
    public:
        static constexpr int numBands = 5;

        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, kMax, numChannels);
            reset();
            forceUpdate();
        }

        void reset() noexcept
        {
            for (auto& f : hp)     f.reset();
            for (auto& f : lp)     f.reset();
            for (auto& band : bands) for (auto& f : band) f.reset();
            for (auto& f : airF)   f.reset();
            for (auto& f : tightF) f.reset();
        }

        struct Settings
        {
            bool  eqOn = true;
            bool  hpOn = false;  float hpFreq = 30.f;
            bool  lpOn = false;  float lpFreq = 20000.f;
            bool  propQ = true;
            float air = 0.f, tight = 0.f;
            struct Band { bool on; float freq, gain, q; };
            std::array<Band, numBands> band {};
        };

        void setSettings (const Settings& s) noexcept { pending = s; dirty = true; }

        void updateCoefficients() noexcept
        {
            if (! dirty) return;
            dirty = false;
            active = pending;

            const double nyq = sampleRate * 0.5;
            auto clampF = [nyq] (float f) { return (float) juce::jlimit (10.0, nyq * 0.98, (double) f); };

            using Coefs = juce::dsp::IIR::Coefficients<float>;

            if (active.hpOn)
                assignAll (hp, Coefs::makeHighPass (sampleRate, clampF (active.hpFreq), 0.707f));
            if (active.lpOn)
                assignAll (lp, Coefs::makeLowPass  (sampleRate, clampF (active.lpFreq), 0.707f));

            for (int b = 0; b < numBands; ++b)
            {
                const auto& bs = active.band[b];
                const float f  = clampF (bs.freq);
                const float g  = dbToGain (bs.gain);
                const float q  = effectiveQ (bs.q, bs.gain);

                Coefs::Ptr c;
                if (b == 0)                 c = Coefs::makeLowShelf  (sampleRate, f, q, g);
                else if (b == numBands - 1) c = Coefs::makeHighShelf (sampleRate, f, q, g);
                else                        c = Coefs::makePeakFilter (sampleRate, f, q, g);
                assignAll (bands[b], c);
            }

            // AIR — fixed 15 kHz high shelf, up to +6 dB.
            assignAll (airF, Coefs::makeHighShelf (sampleRate, clampF (15000.f), 0.6f,
                                                   dbToGain (active.air * 0.06f * 6.f)));
            // TIGHT — 250 Hz low shelf cut, up to -6 dB, cleans low mud.
            assignAll (tightF, Coefs::makeLowShelf (sampleRate, 250.f, 0.7f,
                                                    dbToGain (-active.tight * 0.06f * 6.f)));
        }

        void process (float* const* data, int numChannels, int numSamples) noexcept
        {
            if (! active.eqOn) return;
            const int nCh = juce::jmin (numChannels, channels);

            for (int ch = 0; ch < nCh; ++ch)
            {
                float* x = data[ch];
                for (int n = 0; n < numSamples; ++n)
                {
                    float s = x[n];
                    if (active.hpOn) s = hp[ch].processSample (s);
                    if (active.lpOn) s = lp[ch].processSample (s);
                    for (int b = 0; b < numBands; ++b)
                        if (active.band[b].on)
                            s = bands[b][ch].processSample (s);
                    if (active.air   > 0.001f) s = airF[ch].processSample (s);
                    if (active.tight > 0.001f) s = tightF[ch].processSample (s);
                    x[n] = flushDenorm (s);
                }
                for (int b = 0; b < numBands; ++b) bands[b][ch].snapToZero();
                hp[ch].snapToZero(); lp[ch].snapToZero();
                airF[ch].snapToZero();  tightF[ch].snapToZero();
            }
        }

        // Magnitude of the whole curve at a frequency, for the UI response plot.
        double magnitudeAt (double freq) const noexcept
        {
            double m = 1.0;
            auto mul = [&] (const juce::dsp::IIR::Coefficients<float>::Ptr& c)
            {
                if (c != nullptr)
                    m *= c->getMagnitudeForFrequency (freq, sampleRate);
            };
            if (active.hpOn) mul (hp[0].coefficients);
            if (active.lpOn) mul (lp[0].coefficients);
            for (int b = 0; b < numBands; ++b)
                if (active.band[b].on) mul (bands[b][0].coefficients);
            if (active.air   > 0.001f) mul (airF[0].coefficients);
            if (active.tight > 0.001f) mul (tightF[0].coefficients);
            return m;
        }

    private:
        static constexpr int kMax = 2;
        using Filter = juce::dsp::IIR::Filter<float>;

        float effectiveQ (float baseQ, float gainDb) const noexcept
        {
            if (! active.propQ) return baseQ;
            // Proportional-Q: broaden small moves, tighten large ones.
            const float norm = juce::jlimit (0.f, 1.f, std::abs (gainDb) / 18.f);
            return baseQ * (0.5f + 1.5f * norm);
        }

        template <typename Arr, typename Ptr>
        void assignAll (Arr& arr, Ptr c) noexcept
        {
            for (auto& f : arr) f.coefficients = c;
        }

        void forceUpdate() noexcept { dirty = true; updateCoefficients(); }

        double sampleRate = 44100.0;
        int    channels   = 2;
        bool   dirty      = true;

        Settings pending, active;

        std::array<Filter, kMax> hp, lp;   // one biquad per channel
        std::array<std::array<Filter, kMax>, numBands> bands; // [band][ch]
        std::array<Filter, kMax> airF, tightF;
    };
}
