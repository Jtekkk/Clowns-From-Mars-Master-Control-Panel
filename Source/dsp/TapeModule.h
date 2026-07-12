#pragma once

#include "DspCommon.h"
#include "DriftModel.h"
#include <array>

namespace cfm::dsp
{
    /**
        Analog tape saturation (64-bit).

        Signal path per channel:  drive -> soft hysteresis saturation
          -> speed-dependent head bump (low shelf) -> gap loss (HF roll-off)
          -> user Low / High trim shelves.

        15 ips gives a higher, fatter head bump and earlier HF loss; 30 ips
        extends the top and tightens the lows. The saturation naturally provides
        tape compression (the transfer curve softens peaks) with no added noise,
        crosstalk or pre/post-echo. Exact tanh + double biquads, run oversampled.
    */
    class TapeModule
    {
    public:
        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, kMax, numChannels);
            reset();
            updateFilters();
        }

        void reset() noexcept
        {
            for (auto& f : bump)   f.reset();
            for (auto& f : loss)   f.reset();
            for (auto& f : userLo) f.reset();
            for (auto& f : userHi) f.reset();
            for (auto& s : hyst)   s = 0.0;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        void setParams (double drivePct, int speedIndex, double lfDb, double hfDb) noexcept
        {
            const double d = juce::jlimit (0.0, 1.0, drivePct * 0.01);
            driveGain = std::pow (10.0, d * 0.9);      // 1 .. ~8
            outNorm   = 1.0 / std::pow (driveGain, 0.9);
            speed30   = (speedIndex == 1);
            userLf = lfDb; userHf = hfDb;
            updateFilters();
        }

        void process (double* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (numChannels, channels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                double* x = data[ch];
                const double chDrive = driveGain * (drift ? drift->factor (ch, 8, 0.05) : 1.0);
                const double asym    = drift ? drift->offset (ch, 9, 0.04) : 0.0;

                for (int n = 0; n < numSamples; ++n)
                {
                    // Soft saturation with a hint of hysteresis (state term) and
                    // slight asymmetry — the compression is inherent in the curve.
                    const double in = x[n] * chDrive;
                    const double h  = saturate (in + asym + 0.12 * hyst[ch]);
                    hyst[ch] = flushDenorm (h);
                    double y = h * outNorm;

                    y = bump  [ch].processSample (y);
                    y = loss  [ch].processSample (y);
                    if (std::abs (userLf) > 0.01) y = userLo[ch].processSample (y);
                    if (std::abs (userHf) > 0.01) y = userHi[ch].processSample (y);
                    x[n] = flushDenorm (y);
                }
                bump[ch].snapToZero();  loss[ch].snapToZero();
                userLo[ch].snapToZero(); userHi[ch].snapToZero();
            }
        }

    private:
        static constexpr int kMax = 2;
        using Coefs = juce::dsp::IIR::Coefficients<double>;

        void updateFilters() noexcept
        {
            const double bumpF   = speed30 ? 55.0   : 95.0;
            const double bumpDb  = speed30 ? 1.2    : 2.2;
            const double lossF   = speed30 ? 12000.0 : 8000.0;
            const double lossDb  = speed30 ? -1.2   : -2.8;
            const double nyq     = sampleRate * 0.49;

            auto bumpC = Coefs::makeLowShelf  (sampleRate, bumpF, 0.7, dbToGain (bumpDb));
            auto lossC = Coefs::makeHighShelf (sampleRate, juce::jmin (lossF, nyq), 0.6, dbToGain (lossDb));
            auto loC   = Coefs::makeLowShelf  (sampleRate, 110.0, 0.7, dbToGain (userLf));
            auto hiC   = Coefs::makeHighShelf (sampleRate, juce::jmin (9000.0, nyq), 0.6, dbToGain (userHf));

            for (int ch = 0; ch < kMax; ++ch)
            {
                bump[ch].coefficients   = bumpC;
                loss[ch].coefficients   = lossC;
                userLo[ch].coefficients = loC;
                userHi[ch].coefficients = hiC;
            }
        }

        double sampleRate = 44100.0;
        int    channels   = 2;
        bool   speed30    = true;
        double driveGain = 1.0, outNorm = 1.0, userLf = 0.0, userHf = 0.0;

        std::array<juce::dsp::IIR::Filter<double>, kMax> bump, loss, userLo, userHi;
        std::array<double, kMax> hyst { {} };
        const DriftModel* drift = nullptr;
    };
}
