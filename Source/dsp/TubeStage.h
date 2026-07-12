#pragma once

#include "DspCommon.h"
#include "DriftModel.h"

namespace cfm::dsp
{
    /**
        Class-A tube / MOJO drive stage (64-bit).

        Three amp voicings, each a smooth, bounded, band-limited nonlinearity
        built from the exact tanh transfer so the harmonic series is correct.
        The stage runs inside an oversampled block so its harmonics fold back
        cleanly. It adds harmonic colour rather than volume: a partial gain
        normalisation keeps low-level signal near unity while density builds as
        you drive it.

        Character comes from three ingredients a static "curve" plug-in misses:
          * asymmetry (even harmonics) you steer with Bias,
          * per-channel component drift (analog width), and
          * a program-following operating point via the DC-servo blocker.
    */
    class TubeStage
    {
    public:
        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);
            reset();
            updateInternalCoeffs();
        }

        void reset() noexcept
        {
            for (auto& s : dcX)   s = 0.0;
            for (auto& s : dcY)   s = 0.0;
            for (auto& s : tiltZ) s = 0.0;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        // model: 0 triode, 1 pentode, 2 starved
        void setParams (int modelIndex, double drivePct, double biasPct, double tonePct) noexcept
        {
            model = modelIndex;

            // Drive: 0..100% -> ~1x..~10x, exponential so the knob "feels" even.
            const double d = juce::jlimit (0.0, 1.0, drivePct * 0.01);
            driveGain = std::pow (10.0, d);                  // 1 .. 10
            // Partial output compensation: unity for tiny signals, density on top.
            outNorm   = 1.0 / std::pow (driveGain, 0.82);

            biasDC    = juce::jlimit (-1.0, 1.0, biasPct * 0.01) * 0.35;

            const double t = juce::jlimit (-1.0, 1.0, tonePct * 0.01);
            tiltLowGain  = dbToGain (-t * 3.5);              // tone < 0 -> darker
            tiltHighGain = dbToGain ( t * 3.5);
        }

        void process (double* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (numChannels, channels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                double* x = data[ch];

                // Per-channel drift: slightly different drive, bias and tilt.
                const double chDrive = driveGain * (drift ? drift->factor (ch, 0, 0.06) : 1.0);
                const double chBias  = biasDC    + (drift ? drift->offset (ch, 1, 0.05) : 0.0);
                const double chNorm  = outNorm   * (drift ? drift->factor (ch, 2, 0.03) : 1.0);
                const double shaped0 = shape (chBias);       // operating-point DC

                for (int n = 0; n < numSamples; ++n)
                {
                    const double in  = x[n];
                    const double pre = in * chDrive + chBias;
                    double y = (shape (pre) - shaped0) * chNorm;

                    y = dcBlock (y, ch);
                    y = tilt (y, ch);

                    x[n] = flushDenorm (y);
                }
            }
        }

    private:
        // ---- transfer functions -------------------------------------------
        inline double shape (double v) const noexcept
        {
            switch (model)
            {
                case 1: // Pentode: symmetric, firmer knee, odd-harmonic forward.
                {
                    const double t = saturate (v * 1.25);
                    return 1.08 * t - 0.08 * t * t * t;       // gentle 3rd emphasis, bounded
                }
                case 2: // Starved plate: strongly asymmetric, gritty tops.
                {
                    const double base = (v >= 0.0) ? saturate (v)
                                                   : saturate (v * 0.45);
                    const double hard = juce::jlimit (-1.0, 1.0, v * 0.9);
                    return 0.72 * base + 0.28 * hard;
                }
                default: // Triode: soft, asymmetric, even-harmonic rich.
                {
                    return (v >= 0.0) ? saturate (v)
                                      : saturate (v * 0.75);
                }
            }
        }

        inline double dcBlock (double x, int ch) noexcept
        {
            // First-order DC servo (~10 Hz high-pass) removes the offset that
            // asymmetric saturation introduces, keeping the operating point
            // centred exactly as a cathode-bias cap would.
            const double y = x - dcX[ch] + dcR * dcY[ch];
            dcX[ch] = x;
            dcY[ch] = flushDenorm (y);
            return y;
        }

        inline double tilt (double x, int ch) noexcept
        {
            // One-pole low/high split, recombined with complementary gains.
            tiltZ[ch] = tiltZ[ch] + tiltA * (x - tiltZ[ch]);
            const double low  = tiltZ[ch];
            const double high = x - low;
            return low * tiltLowGain + high * tiltHighGain;
        }

        void updateInternalCoeffs() noexcept
        {
            const double dcCut = 10.0;
            dcR   = std::exp (-2.0 * juce::MathConstants<double>::pi * dcCut / sampleRate);
            const double tiltCut = 720.0;
            tiltA = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * tiltCut / sampleRate);
        }

        static constexpr int kMax = 2;
        double sampleRate = 44100.0;
        int    channels   = 2;
        int    model      = 0;

        double driveGain = 1.0, outNorm = 1.0, biasDC = 0.0;
        double tiltLowGain = 1.0, tiltHighGain = 1.0;
        double dcR = 0.999, tiltA = 0.1;

        std::array<double, kMax> dcX { {} }, dcY { {} }, tiltZ { {} };
        const DriftModel* drift = nullptr;
    };
}
