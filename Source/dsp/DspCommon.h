#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
    Small shared helpers used across the Turbo Tubes DSP modules.

    Everything here is real-time safe: no allocation, no locking, no logging.
*/
namespace tt::dsp
{
    // Fast, branch-free denormal guard. We also flush denormals globally via
    // juce::ScopedNoDenormals in the processor, but individual feedback paths
    // (envelopes, filter states) benefit from an explicit tiny-DC offset.
    inline constexpr float antiDenormal = 1.0e-20f;

    template <typename T>
    inline T flushDenorm (T x) noexcept
    {
        return x + (T) antiDenormal - (T) antiDenormal;
    }

    // Cheap tanh approximation — accurate to ~1e-3 over the audio range and
    // considerably faster than std::tanh in a per-sample loop. Odd-symmetric,
    // monotonic, and bounded, so it is well behaved as a saturating shaper.
    inline float fastTanh (float x) noexcept
    {
        const float x2 = x * x;
        const float a  = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const float b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        return juce::jlimit (-1.0f, 1.0f, a / b);
    }

    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  noexcept { return 20.0f * std::log10 (juce::jmax (1.0e-9f, g)); }

    // One-pole smoothing coefficient for a given time constant (seconds).
    inline float onePoleCoeff (float timeSeconds, double sampleRate) noexcept
    {
        if (timeSeconds <= 0.0f) return 0.0f;
        return std::exp (-1.0f / (timeSeconds * (float) sampleRate));
    }

    // Equal-power-ish crossfade used for dry/wet and parallel blends.
    inline void equalPower (float mix, float& dryGain, float& wetGain) noexcept
    {
        dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
        wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
    }
}
