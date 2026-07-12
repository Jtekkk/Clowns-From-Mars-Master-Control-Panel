#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
    Small shared helpers used across the Master Control Panel DSP modules.

    The whole internal signal path runs in double precision (`cfm::dsp::Sample`)
    for maximum headroom and numerical accuracy — filter states, envelopes and
    saturators never lose bits to 32-bit rounding, and coefficients are computed
    at 64-bit. Everything here is real-time safe: no allocation, no locking.
*/
namespace cfm::dsp
{
    // Internal processing precision. 64-bit throughout for reference fidelity.
    using Sample = double;

    // Tiny anti-denormal DC used on feedback paths (envelopes, filter states).
    // Global flush-to-zero is also enabled via juce::ScopedNoDenormals.
    inline constexpr double antiDenormal = 1.0e-30;

    template <typename T>
    inline T flushDenorm (T x) noexcept
    {
        return x + (T) antiDenormal - (T) antiDenormal;
    }

    // Accurate, bounded, odd-symmetric saturator. We deliberately use the exact
    // std::tanh (not a rational approximation) so the harmonic series is correct;
    // the nonlinear stages run oversampled, so accuracy — not raw speed — wins.
    inline double saturate (double x) noexcept { return std::tanh (x); }

    inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }
    inline double gainToDb (double g)  noexcept { return 20.0 * std::log10 (juce::jmax (1.0e-12, g)); }

    // One-pole smoothing coefficient for a given time constant (seconds).
    inline double onePoleCoeff (double timeSeconds, double sampleRate) noexcept
    {
        if (timeSeconds <= 0.0) return 0.0;
        return std::exp (-1.0 / (timeSeconds * sampleRate));
    }

    // Equal-power crossfade used for dry/wet and parallel blends.
    inline void equalPower (double mix, double& dryGain, double& wetGain) noexcept
    {
        dryGain = std::cos (mix * juce::MathConstants<double>::halfPi);
        wetGain = std::sin (mix * juce::MathConstants<double>::halfPi);
    }
}
