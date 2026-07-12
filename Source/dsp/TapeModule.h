#pragma once

#include "DspCommon.h"
#include "DriftModel.h"
#include <array>

namespace tt::dsp
{
    /**
        Analog tape saturation.

        Signal path per channel:  drive -> soft hysteresis saturation
          -> speed-dependent head bump (low shelf) -> gap loss (HF roll-off)
          -> user Low / High trim shelves.

        Selecting 15 ips gives a higher, fatter head bump and earlier HF loss;
        30 ips extends the top and tightens the lows — matching how a machine
        actually behaves at each speed. The saturation naturally provides tape
        compression (the transfer curve softens peaks) without added noise,
        crosstalk or pre/post-echo. Runs oversampled so it stays clean.
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
            for (auto& s : hyst)   s = 0.0f;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        void setParams (float drivePct, int speedIndex, float lfDb, float hfDb) noexcept
        {
            const float d = juce::jlimit (0.0f, 1.0f, drivePct * 0.01f);
            driveGain = std::pow (10.0f, d * 0.9f);      // 1 .. ~8
            outNorm   = 1.0f / std::pow (driveGain, 0.9f);
            speed30   = (speedIndex == 1);
            userLf = lfDb; userHf = hfDb;
            updateFilters();
        }

        void process (float* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (numChannels, channels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* x = data[ch];
                const float chDrive = driveGain * (drift ? drift->factor (ch, 8, 0.05f) : 1.0f);
                const float asym    = drift ? drift->offset (ch, 9, 0.04f) : 0.0f;

                for (int n = 0; n < numSamples; ++n)
                {
                    // Soft saturation with a hint of hysteresis (state term) and
                    // slight asymmetry — the compression is inherent in the curve.
                    const float in = x[n] * chDrive;
                    const float h  = fastTanh (in + asym + 0.12f * hyst[ch]);
                    hyst[ch] = flushDenorm (h);
                    float y = h * outNorm;

                    y = bump  [ch].processSample (y);
                    y = loss  [ch].processSample (y);
                    if (std::abs (userLf) > 0.01f) y = userLo[ch].processSample (y);
                    if (std::abs (userHf) > 0.01f) y = userHi[ch].processSample (y);
                    x[n] = flushDenorm (y);
                }
                bump[ch].snapToZero();  loss[ch].snapToZero();
                userLo[ch].snapToZero(); userHi[ch].snapToZero();
            }
        }

    private:
        static constexpr int kMax = 2;
        using Coefs = juce::dsp::IIR::Coefficients<float>;

        void updateFilters() noexcept
        {
            const float bumpF   = speed30 ? 55.0f  : 95.0f;
            const float bumpDb  = speed30 ? 1.2f   : 2.2f;
            const float lossF   = speed30 ? 12000.f : 8000.f;
            const float lossDb  = speed30 ? -1.2f  : -2.8f;
            const float nyq     = (float) (sampleRate * 0.49);

            auto bumpC = Coefs::makeLowShelf  (sampleRate, bumpF, 0.7f, dbToGain (bumpDb));
            auto lossC = Coefs::makeHighShelf (sampleRate, juce::jmin (lossF, nyq), 0.6f, dbToGain (lossDb));
            auto loC   = Coefs::makeLowShelf  (sampleRate, 110.0f, 0.7f, dbToGain (userLf));
            auto hiC   = Coefs::makeHighShelf (sampleRate, juce::jmin (9000.0f, nyq), 0.6f, dbToGain (userHf));

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
        float  driveGain = 1.0f, outNorm = 1.0f, userLf = 0.0f, userHf = 0.0f;

        std::array<juce::dsp::IIR::Filter<float>, kMax> bump, loss, userLo, userHi;
        std::array<float, kMax> hyst { {} };
        const DriftModel* drift = nullptr;
    };
}
