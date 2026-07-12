#pragma once

#include "DspCommon.h"
#include <array>

namespace cfm::dsp
{
    /**
        Mastering-grade five-band parametric EQ (64-bit biquads).

          band 0 : low shelf     band 1..3 : peak     band 4 : high shelf
          + variable High-Pass and Low-Pass (12 dB/oct, Butterworth)
          + AIR   : gentle high shelf around 15 kHz for open top end
          + TIGHT : low-shelf clean-up that de-muds the 250 Hz region

        Curves use the RBJ analog-matched biquad design computed and run in
        double precision, so boosts stay clean and phase-accurate right down to
        the lowest bands where 32-bit biquads lose resolution. Q can follow the
        classic Proportional-Q law (broad for small moves, tighter for big ones)
        or stay Constant-Q, switchable globally.
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
            bool   eqOn = true;
            bool   hpOn = false;  double hpFreq = 30.0;
            bool   lpOn = false;  double lpFreq = 20000.0;
            bool   propQ = true;
            double air = 0.0, tight = 0.0;
            struct Band { bool on; double freq, gain, q; };
            std::array<Band, numBands> band {};
        };

        void setSettings (const Settings& s) noexcept
        {
            if (! settingsEqual (s, pending)) { pending = s; dirty = true; }
        }

        static bool settingsEqual (const Settings& a, const Settings& b) noexcept
        {
            if (a.eqOn != b.eqOn || a.hpOn != b.hpOn || a.hpFreq != b.hpFreq
                || a.lpOn != b.lpOn || a.lpFreq != b.lpFreq || a.propQ != b.propQ
                || a.air != b.air || a.tight != b.tight) return false;
            for (int i = 0; i < numBands; ++i)
            {
                const auto& x = a.band[i]; const auto& y = b.band[i];
                if (x.on != y.on || x.freq != y.freq || x.gain != y.gain || x.q != y.q) return false;
            }
            return true;
        }

        // Returns true if the coefficients were actually (re)computed this call.
        bool updateCoefficients() noexcept
        {
            if (! dirty) return false;
            dirty = false;
            active = pending;

            const double nyq = sampleRate * 0.5;
            auto clampF = [nyq] (double f) { return juce::jlimit (10.0, nyq * 0.98, f); };

            using Coefs = juce::dsp::IIR::Coefficients<double>;

            if (active.hpOn)
                assignAll (hp, Coefs::makeHighPass (sampleRate, clampF (active.hpFreq), 0.707));
            if (active.lpOn)
                assignAll (lp, Coefs::makeLowPass  (sampleRate, clampF (active.lpFreq), 0.707));

            for (int b = 0; b < numBands; ++b)
            {
                const auto& bs = active.band[b];
                const double f = clampF (bs.freq);
                const double g = dbToGain (bs.gain);
                const bool  isShelf = (b == 0 || b == numBands - 1);
                // Proportional-Q is a bell concept: on a shelf, any Q above
                // Butterworth (~0.707) produces a resonant overshoot at the
                // corner. Keep shelves at their (Butterworth-ish) base Q and
                // apply the proportional law only to the three peak bands.
                const double q = isShelf ? bs.q : effectiveQ (bs.q, bs.gain);

                Coefs::Ptr c;
                if (b == 0)                 c = Coefs::makeLowShelf  (sampleRate, f, q, g);
                else if (b == numBands - 1) c = Coefs::makeHighShelf (sampleRate, f, q, g);
                else                        c = Coefs::makePeakFilter (sampleRate, f, q, g);
                assignAll (bands[b], c);
            }

            // AIR — fixed 15 kHz high shelf, 0..100% maps to 0..+6 dB.
            assignAll (airF, Coefs::makeHighShelf (sampleRate, clampF (15000.0), 0.6,
                                                   dbToGain (active.air * 0.06)));
            // TIGHT — 250 Hz low shelf cut, 0..100% maps to 0..-6 dB, cleans low mud.
            assignAll (tightF, Coefs::makeLowShelf (sampleRate, 250.0, 0.7,
                                                    dbToGain (-active.tight * 0.06)));
            return true;
        }

        void process (double* const* data, int numChannels, int numSamples) noexcept
        {
            if (! active.eqOn) return;
            const int nCh = juce::jmin (numChannels, channels);

            for (int ch = 0; ch < nCh; ++ch)
            {
                double* x = data[ch];
                for (int n = 0; n < numSamples; ++n)
                {
                    double s = x[n];
                    if (active.hpOn) s = hp[ch].processSample (s);
                    if (active.lpOn) s = lp[ch].processSample (s);
                    for (int b = 0; b < numBands; ++b)
                        if (active.band[b].on)
                            s = bands[b][ch].processSample (s);
                    if (active.air   > 0.001) s = airF[ch].processSample (s);
                    if (active.tight > 0.001) s = tightF[ch].processSample (s);
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
            auto mul = [&] (const juce::dsp::IIR::Coefficients<double>::Ptr& c)
            {
                if (c != nullptr)
                    m *= c->getMagnitudeForFrequency (freq, sampleRate);
            };
            if (active.hpOn) mul (hp[0].coefficients);
            if (active.lpOn) mul (lp[0].coefficients);
            for (int b = 0; b < numBands; ++b)
                if (active.band[b].on) mul (bands[b][0].coefficients);
            if (active.air   > 0.001) mul (airF[0].coefficients);
            if (active.tight > 0.001) mul (tightF[0].coefficients);
            return m;
        }

    private:
        static constexpr int kMax = 2;
        using Filter = juce::dsp::IIR::Filter<double>;

        double effectiveQ (double baseQ, double gainDb) const noexcept
        {
            if (! active.propQ) return baseQ;
            // Proportional-Q: broaden small moves, tighten large ones.
            const double norm = juce::jlimit (0.0, 1.0, std::abs (gainDb) / 18.0);
            return baseQ * (0.5 + 1.5 * norm);
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
