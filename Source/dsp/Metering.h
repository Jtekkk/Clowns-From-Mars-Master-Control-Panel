#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

namespace tt::dsp
{
    /**
        Output metering with honest reference levels.

        We report both true peak-ish sample peak and RMS per channel. All
        values are held in atomics written on the audio thread and read on the
        UI thread — no locks, no allocation. Peaks decay smoothly so the meter
        reads like hardware rather than flickering.
    */
    class MeterBallistics
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            // ~11.8 dB/s peak decay, 300 ms RMS window.
            peakDecay = std::exp (-1.0f / (float) (0.6 * sr));
            rmsCoeff  = std::exp (-1.0f / (float) (0.3 * sr));
            reset();
        }

        void reset() noexcept
        {
            for (auto& v : peak) v = 0.0f;
            for (auto& v : ms)   v = 0.0f;
            for (int c = 0; c < 2; ++c) { peakAtomic[c].store (0.f); rmsAtomic[c].store (0.f); }
        }

        void process (const float* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (2, numChannels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                float p = peak[ch], m = ms[ch];
                const float* x = data[ch];
                for (int n = 0; n < numSamples; ++n)
                {
                    const float a = std::abs (x[n]);
                    p = (a > p) ? a : p * peakDecay;
                    m = rmsCoeff * m + (1.0f - rmsCoeff) * (x[n] * x[n]);
                }
                peak[ch] = p; ms[ch] = m;
                peakAtomic[ch].store (p, std::memory_order_relaxed);
                rmsAtomic[ch].store (std::sqrt (m), std::memory_order_relaxed);
            }
        }

        float getPeak (int ch) const noexcept { return peakAtomic[juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed); }
        float getRms  (int ch) const noexcept { return rmsAtomic [juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed); }

    private:
        double sampleRate = 44100.0;
        float  peakDecay = 0.f, rmsCoeff = 0.f;
        std::array<float, 2> peak { {} }, ms { {} };
        std::array<std::atomic<float>, 2> peakAtomic, rmsAtomic;
    };

    /**
        Lock-free mono feed for the FFT spectrum analyzer. The audio thread
        pushes a mono sum into a ring buffer; the UI thread drains the newest
        block and windows/transforms it on its own time.
    */
    class AnalyzerFifo
    {
    public:
        static constexpr int fftOrder = 11;             // 2048-point
        static constexpr int fftSize  = 1 << fftOrder;

        void push (const float* const* data, int numChannels, int numSamples) noexcept
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float s = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch) s += data[ch][n];
                s /= (float) juce::jmax (1, numChannels);
                buffer[writeIndex] = s;
                writeIndex = (writeIndex + 1) & (fftSize - 1);
                ready.store (true, std::memory_order_relaxed);
            }
        }

        // Copies the most recent fftSize samples into dst (size >= fftSize).
        // Returns false if nothing new since the last read.
        bool read (float* dst) noexcept
        {
            if (! ready.exchange (false, std::memory_order_relaxed)) return false;
            const int start = writeIndex;
            for (int i = 0; i < fftSize; ++i)
                dst[i] = buffer[(start + i) & (fftSize - 1)];
            return true;
        }

    private:
        std::array<float, fftSize> buffer { {} };
        int writeIndex = 0;
        std::atomic<bool> ready { false };
    };
}
