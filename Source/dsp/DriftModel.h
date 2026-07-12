#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace cfm::dsp
{
    /**
        Component variation / analog drift.

        Real analog gear is built from components with tolerances, so no two
        units — and no two channels within a unit — measure identically. That
        tiny asymmetry is a big part of why hardware sounds "wide" and alive.

        Master Control Panel reproduces this deterministically: a serial number seeds a
        reproducible PRNG that offsets every drift-sensitive parameter per
        channel. Same serial → same unit, every session ("unique digital
        fingerprint"). The `amount` knob scales how far the components have
        drifted from nominal.
    */
    class DriftModel
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int slots       = 16; // number of independent drift taps

        void setSerial (juce::int64 serial) noexcept
        {
            juce::Random rng (serial ^ 0x5bd1e995LL);
            for (int ch = 0; ch < maxChannels; ++ch)
                for (int s = 0; s < slots; ++s)
                    unit[ch][s] = rng.nextFloat() * 2.0f - 1.0f; // [-1, 1]
        }

        // amountPercent: 0..100 from the Drift control.
        void setAmount (float amountPercent) noexcept
        {
            amount = juce::jlimit (0.0f, 1.0f, amountPercent * 0.01f);
        }

        // Returns a per-channel multiplicative deviation around 1.0.
        // maxDeviation is the full-scale spread (e.g. 0.05 == ±5%).
        float factor (int channel, int slot, float maxDeviation) const noexcept
        {
            const int ch = channel % maxChannels;
            const int s  = slot % slots;
            return 1.0f + unit[ch][s] * maxDeviation * amount;
        }

        // Additive deviation (e.g. for bias offsets), symmetric around 0.
        float offset (int channel, int slot, float maxOffset) const noexcept
        {
            const int ch = channel % maxChannels;
            const int s  = slot % slots;
            return unit[ch][s] * maxOffset * amount;
        }

    private:
        std::array<std::array<float, slots>, maxChannels> unit { {} };
        float amount = 0.25f;
    };
}
