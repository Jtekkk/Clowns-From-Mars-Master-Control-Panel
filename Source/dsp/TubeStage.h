#pragma once

#include "DspCommon.h"
#include "DriftModel.h"

namespace cfm::dsp
{
    /**
        Class-A tube / MOJO drive stage.

        Three amp voicings, each a smooth, bounded, band-limited nonlinearity.
        The stage is designed to run *inside* an oversampled block so the
        harmonics it generates fold back cleanly (see PluginProcessor). It adds
        harmonic colour rather than volume: a partial gain-normalisation keeps
        low-level signal near unity while density builds as you drive it.

        Character comes from three ingredients that a static "curve" plug-in
        misses:
          * asymmetry (even harmonics) that you can steer with Bias,
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
            for (auto& s : dcX)   s = 0.0f;
            for (auto& s : dcY)   s = 0.0f;
            for (auto& s : tiltZ) s = 0.0f;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        // model: 0 triode, 1 pentode, 2 starved
        void setParams (int modelIndex, float drivePct, float biasPct, float tonePct) noexcept
        {
            model = modelIndex;

            // Drive: 0..100% -> ~1x..~10x, exponential so the knob "feels" even.
            const float d = juce::jlimit (0.0f, 1.0f, drivePct * 0.01f);
            driveGain = std::pow (10.0f, d * 1.0f);          // 1 .. 10
            // Partial output compensation: unity for tiny signals, density on top.
            outNorm   = 1.0f / std::pow (driveGain, 0.82f);

            biasDC    = juce::jlimit (-1.0f, 1.0f, biasPct * 0.01f) * 0.35f;

            const float t = juce::jlimit (-1.0f, 1.0f, tonePct * 0.01f);
            tiltLowGain  = dbToGain (-t * 3.5f);             // tone < 0 -> darker
            tiltHighGain = dbToGain ( t * 3.5f);
        }

        void process (float* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (numChannels, channels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* x = data[ch];

                // Per-channel drift: slightly different drive, bias and tilt.
                const float chDrive = driveGain * (drift ? drift->factor (ch, 0, 0.06f) : 1.0f);
                const float chBias  = biasDC    + (drift ? drift->offset (ch, 1, 0.05f) : 0.0f);
                const float chNorm  = outNorm   * (drift ? drift->factor (ch, 2, 0.03f) : 1.0f);
                const float shaped0 = shape (chBias);        // operating-point DC

                for (int n = 0; n < numSamples; ++n)
                {
                    float in  = x[n];
                    float pre = in * chDrive + chBias;
                    float y   = (shape (pre) - shaped0) * chNorm;

                    y = dcBlock (y, ch);
                    y = tilt (y, ch);

                    x[n] = flushDenorm (y);
                }
            }
        }

    private:
        // ---- transfer functions -------------------------------------------
        inline float shape (float v) const noexcept
        {
            switch (model)
            {
                case 1: // Pentode: symmetric, firmer knee, odd-harmonic forward.
                {
                    const float t = fastTanh (v * 1.25f);
                    return 1.08f * t - 0.08f * t * t * t;      // gentle 3rd emphasis, bounded
                }
                case 2: // Starved plate: strongly asymmetric, gritty tops.
                {
                    const float base = (v >= 0.0f) ? fastTanh (v)
                                                   : fastTanh (v * 0.45f);
                    const float hard = juce::jlimit (-1.0f, 1.0f, v * 0.9f);
                    return 0.72f * base + 0.28f * hard;
                }
                default: // Triode: soft, asymmetric, even-harmonic rich.
                {
                    return (v >= 0.0f) ? fastTanh (v)
                                       : fastTanh (v * 0.75f);
                }
            }
        }

        inline float dcBlock (float x, int ch) noexcept
        {
            // First-order DC servo (~10 Hz high-pass) removes the offset that
            // asymmetric saturation introduces, keeping the operating point
            // centred exactly as a cathode-bias cap would.
            const float y = x - dcX[ch] + dcR * dcY[ch];
            dcX[ch] = x;
            dcY[ch] = flushDenorm (y);
            return y;
        }

        inline float tilt (float x, int ch) noexcept
        {
            // One-pole low/high split, recombined with complementary gains.
            tiltZ[ch] = tiltZ[ch] + tiltA * (x - tiltZ[ch]);
            const float low  = tiltZ[ch];
            const float high = x - low;
            return low * tiltLowGain + high * tiltHighGain;
        }

        void updateInternalCoeffs() noexcept
        {
            const float dcCut = 10.0f;
            dcR   = std::exp (-2.0f * juce::MathConstants<float>::pi * dcCut / (float) sampleRate);
            const float tiltCut = 720.0f;
            tiltA = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * tiltCut / (float) sampleRate);
        }

        static constexpr int kMax = 2;
        double sampleRate = 44100.0;
        int    channels   = 2;
        int    model      = 0;

        float driveGain = 1.0f, outNorm = 1.0f, biasDC = 0.0f;
        float tiltLowGain = 1.0f, tiltHighGain = 1.0f;
        float dcR = 0.999f, tiltA = 0.1f;

        std::array<float, kMax> dcX { {} }, dcY { {} }, tiltZ { {} };
        const DriftModel* drift = nullptr;
    };
}
