#pragma once

#include "DspCommon.h"
#include <atomic>
#include <array>

namespace cfm::dsp
{
    /**
        ITU-R BS.1770 loudness meter (LUFS), 64-bit.

        Signal is K-weighted (a high-frequency shelving "head" filter followed by
        an RLB high-pass), then measured as:
          * Momentary  — 400 ms sliding window,
          * Short-term — 3 s sliding window,
          * Integrated — gated (absolute -70 LUFS + relative -10 LU), computed
                         from a loudness histogram so it needs no growing storage.

        The K-weighting uses RBJ high-shelf + high-pass matched to the BS.1770
        corner frequencies, recomputed per sample rate, so it tracks the standard
        curve closely at every rate. Metering only — never in the audio path.
    */
    class LoudnessMeter
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            blockLen   = juce::jmax (1, (int) std::round (0.1 * sr)); // 100 ms gating step

            using C = juce::dsp::IIR::Coefficients<double>;
            auto shelf = C::makeHighShelf (sr, 1681.97, 0.7071752, dbToGain (3.99984));
            auto hp    = C::makeHighPass  (sr, 38.13547, 0.5003271);
            for (int ch = 0; ch < 2; ++ch) { pre[ch].coefficients = shelf; rlb[ch].coefficients = hp; }
            reset();
        }

        void reset() noexcept
        {
            for (int ch = 0; ch < 2; ++ch) { pre[ch].reset(); rlb[ch].reset(); acc[ch] = 0.0; ring[ch].fill (0.0); }
            count = 0; writeIdx = 0; filled = 0;
            hist.fill (Bin{});
            momentary.store (-100.f); shortTerm.store (-100.f); integrated.store (-100.f);
        }

        void resetIntegrated() noexcept
        {
            hist.fill (Bin{});
            integrated.store (-100.f);
        }

        void process (const double* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (2, numChannels);
            for (int n = 0; n < numSamples; ++n)
            {
                for (int ch = 0; ch < nCh; ++ch)
                {
                    double s = pre[ch].processSample (data[ch][n]);
                    s = rlb[ch].processSample (s);
                    acc[ch] += s * s;
                }
                if (++count >= blockLen)
                {
                    finishBlock (nCh);
                    count = 0;
                }
            }
            for (int ch = 0; ch < nCh; ++ch) { pre[ch].snapToZero(); rlb[ch].snapToZero(); }
        }

        float getMomentaryLufs()  const noexcept { return momentary.load (std::memory_order_relaxed); }
        float getShortTermLufs()  const noexcept { return shortTerm.load (std::memory_order_relaxed); }
        float getIntegratedLufs() const noexcept { return integrated.load (std::memory_order_relaxed); }

    private:
        static constexpr double absGate = -70.0;
        static constexpr int    nBins   = 800;   // -70.0 .. +10.0 LUFS, 0.1 steps
        static constexpr double binLo   = -70.0, binStep = 0.1;

        struct Bin { long long n = 0; double z = 0.0; };

        void finishBlock (int nCh) noexcept
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                ring[ch][(size_t) writeIdx] = (ch < nCh) ? acc[ch] / (double) blockLen : 0.0;
                acc[ch] = 0.0;
            }
            writeIdx = (writeIdx + 1) % ringLen;
            filled = juce::jmin (filled + 1, ringLen);

            // z over a window of the last `blocks` 100 ms slots, summed over channels.
            auto zOver = [this, nCh] (int blocks) -> double
            {
                const int b = juce::jmin (blocks, filled);
                if (b <= 0) return 0.0;
                double z = 0.0;
                for (int ch = 0; ch < nCh; ++ch)
                {
                    double s = 0.0;
                    for (int i = 0; i < b; ++i)
                        s += ring[ch][(size_t) ((writeIdx - 1 - i + ringLen) % ringLen)];
                    z += s / (double) b;
                }
                return z;
            };

            const double z400 = zOver (4);   // 400 ms momentary / gating block
            const double z3s  = zOver (30);  // 3 s short-term

            momentary.store ((float) loudness (z400), std::memory_order_relaxed);
            shortTerm.store ((float) loudness (z3s),  std::memory_order_relaxed);

            // Feed the integrated histogram (one gating block every 100 ms).
            const double lm = loudness (z400);
            if (z400 > 0.0 && lm >= absGate)
            {
                int idx = (int) ((lm - binLo) / binStep);
                idx = juce::jlimit (0, nBins - 1, idx);
                hist[(size_t) idx].n += 1;
                hist[(size_t) idx].z += z400;
            }
            updateIntegrated();
        }

        void updateIntegrated() noexcept
        {
            // Absolute-gated mean.
            long long nAbs = 0; double zAbs = 0.0;
            for (int i = 0; i < nBins; ++i) { nAbs += hist[(size_t) i].n; zAbs += hist[(size_t) i].z; }
            if (nAbs == 0) { integrated.store (-100.f, std::memory_order_relaxed); return; }

            const double relGate = loudness (zAbs / (double) nAbs) - 10.0;
            const int relStart = juce::jlimit (0, nBins - 1, (int) ((relGate - binLo) / binStep));

            long long nRel = 0; double zRel = 0.0;
            for (int i = relStart; i < nBins; ++i) { nRel += hist[(size_t) i].n; zRel += hist[(size_t) i].z; }
            if (nRel == 0) { integrated.store (-100.f, std::memory_order_relaxed); return; }

            integrated.store ((float) loudness (zRel / (double) nRel), std::memory_order_relaxed);
        }

        static double loudness (double z) noexcept
        {
            return (z > 1.0e-12) ? -0.691 + 10.0 * std::log10 (z) : -100.0;
        }

        static constexpr int ringLen = 30; // 3 s of 100 ms slots

        double sampleRate = 44100.0;
        int    blockLen = 4410, count = 0, writeIdx = 0, filled = 0;
        std::array<juce::dsp::IIR::Filter<double>, 2> pre, rlb;
        std::array<double, 2> acc { {} };
        std::array<std::array<double, ringLen>, 2> ring {};
        std::array<Bin, nBins> hist {};

        std::atomic<float> momentary { -100.f }, shortTerm { -100.f }, integrated { -100.f };
    };
}
