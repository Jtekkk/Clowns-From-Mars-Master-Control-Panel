#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace cfm::dsp
{
    /**
        Inter-sample (true) peak estimator.

        A 4x polyphase FIR (windowed-sinc, 8 taps/phase) reconstructs the values
        between samples, so the meter catches inter-sample overs that a plain
        sample-peak meter misses — the honest reading a mastering engineer needs
        before a limiter/converter. Metering-only; never in the audio path.
    */
    class TruePeakInterp
    {
    public:
        static constexpr int L = 4;   // oversampling factor
        static constexpr int N = 8;   // taps per phase

        void prepare()
        {
            const int M = N * L;
            const double centre = (M - 1) * 0.5;
            std::array<double, N * L> proto {};
            for (int i = 0; i < M; ++i)
            {
                const double t = (i - centre) / (double) L;
                const double s = (std::abs (t) < 1.0e-9) ? 1.0
                                : std::sin (juce::MathConstants<double>::pi * t)
                                  / (juce::MathConstants<double>::pi * t);
                const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (M - 1));
                proto[(size_t) i] = s * w;
            }
            for (int p = 0; p < L; ++p)
            {
                double sum = 0.0;
                for (int k = 0; k < N; ++k) sum += proto[(size_t) (p + k * L)];
                const double norm = (std::abs (sum) > 1.0e-12) ? 1.0 / sum : 1.0;
                for (int k = 0; k < N; ++k) phase[p][k] = proto[(size_t) (p + k * L)] * norm;
            }
            reset();
        }

        void reset() { for (auto& c : ring) for (auto& s : c) s = 0.0; }

        // Returns the inter-sample peak magnitude around this input sample.
        double process (int ch, double x) noexcept
        {
            auto& r = ring[(size_t) (ch & 1)];
            for (int k = N - 1; k > 0; --k) r[(size_t) k] = r[(size_t) (k - 1)];
            r[0] = x;

            double pk = std::abs (x);
            for (int p = 0; p < L; ++p)
            {
                double acc = 0.0;
                for (int k = 0; k < N; ++k) acc += r[(size_t) k] * phase[p][k];
                pk = juce::jmax (pk, std::abs (acc));
            }
            return pk;
        }

    private:
        std::array<std::array<double, N>, L> phase {};
        std::array<std::array<double, N>, 2> ring {};
    };

    /**
        Output metering with honest reference levels.

        Reports true (inter-sample) peak and RMS per channel. All values are held
        in atomics written on the audio thread and read on the UI thread — no
        locks, no allocation. Peaks decay smoothly so the meter reads like
        hardware rather than flickering.
    */
    class MeterBallistics
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            peakDecay = std::exp (-1.0 / (0.6 * sr));
            rmsCoeff  = std::exp (-1.0 / (0.3 * sr));
            tp[0].prepare(); tp[1].prepare();
            reset();
        }

        void reset() noexcept
        {
            for (auto& v : peak) v = 0.0;
            for (auto& v : ms)   v = 0.0;
            tp[0].reset(); tp[1].reset();
            for (int c = 0; c < 2; ++c) { peakAtomic[c].store (0.f); rmsAtomic[c].store (0.f); }
        }

        void process (const double* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (2, numChannels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                double p = peak[ch], m = ms[ch];
                const double* x = data[ch];
                for (int n = 0; n < numSamples; ++n)
                {
                    const double a = tp[ch].process (ch, x[n]);   // inter-sample peak
                    p = (a > p) ? a : p * peakDecay;
                    m = rmsCoeff * m + (1.0 - rmsCoeff) * (x[n] * x[n]);
                }
                peak[ch] = p; ms[ch] = m;
                peakAtomic[ch].store ((float) p, std::memory_order_relaxed);
                rmsAtomic[ch].store ((float) std::sqrt (m), std::memory_order_relaxed);
            }
        }

        float getPeak (int ch) const noexcept { return peakAtomic[juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed); }
        float getRms  (int ch) const noexcept { return rmsAtomic [juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed); }

    private:
        double sampleRate = 44100.0;
        double peakDecay = 0.0, rmsCoeff = 0.0;
        std::array<double, 2> peak { {} }, ms { {} };
        std::array<TruePeakInterp, 2> tp {};
        std::array<std::atomic<float>, 2> peakAtomic, rmsAtomic;
    };

    /**
        Lock-free mono feed for the FFT spectrum analyzer. The audio thread
        pushes a mono sum (down-converted to float for the display FFT); the UI
        thread drains the newest block and windows/transforms it on its own time.
    */
    class AnalyzerFifo
    {
    public:
        static constexpr int fftOrder = 11;             // 2048-point
        static constexpr int fftSize  = 1 << fftOrder;

        void push (const double* const* data, int numChannels, int numSamples) noexcept
        {
            for (int n = 0; n < numSamples; ++n)
            {
                double s = 0.0;
                for (int ch = 0; ch < numChannels; ++ch) s += data[ch][n];
                s /= (double) juce::jmax (1, numChannels);
                buffer[writeIndex] = (float) s;
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
