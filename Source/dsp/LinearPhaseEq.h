#pragma once

#include "DspCommon.h"
#include <functional>
#include <array>
#include <complex>

namespace cfm::dsp
{
    /**
        Linear-phase EQ via frequency-sampling FIR design + direct convolution.

        The minimum-phase biquad cascade (EqualizerModule) supplies the target
        magnitude response; we sample it across the spectrum, inverse-transform
        it to a zero-phase impulse, window it into a symmetric (hence
        linear-phase) FIR, and convolve. Result: the exact same magnitude curve
        with zero phase distortion — no group-delay smearing of transients, at
        the cost of a fixed latency of (FIR length − 1)/2 samples (reported to
        the host).

        The inverse transform uses a small self-contained double-precision
        radix-2 FFT rather than juce::dsp::FFT: the latter's scaling/engine is
        platform-dependent (and was observed to return incorrect results in some
        builds), which would corrupt the kernel. Owning the transform guarantees
        an exact, unity-at-flat kernel everywhere.

        The kernel is rebuilt only when the EQ settings actually change, using
        preallocated buffers, so steady-state playback does no FFT work.
    */
    class LinearPhaseEq
    {
    public:
        static constexpr int fftOrder = 12;          // 4096-point design FFT
        static constexpr int fftSize  = 1 << fftOrder;
        static constexpr int firLen   = fftSize / 2 + 1; // 2049 taps
        static constexpr int latency  = (firLen - 1) / 2; // 1024 samples

        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 2, numChannels);
            for (auto& d : line) d.fill (0.0);
            pos = { 0, 0 };
            haveKernel = false;
            // A flat (unity) kernel until the first rebuild: pure delay.
            kernel.fill (0.0);
            kernel[(size_t) latency] = 1.0;
        }

        int getLatencySamples() const noexcept { return latency; }

        // Rebuild the linear-phase kernel from a magnitude function (|H(f)| in
        // linear gain). RT-safe: no allocation, fixed-size FFT + windowing.
        void rebuild (const std::function<double (double)>& magAt) noexcept
        {
            const double nyq = sampleRate * 0.5;
            // Bound the sampled magnitude to a musical window so pathologically
            // stacked boosts can't build an absurd-gain kernel.
            const double mMax = dbToGain (36.0), mMin = dbToGain (-80.0);
            for (int k = 0; k < fftSize; ++k)
            {
                const int kk = (k <= fftSize / 2) ? k : fftSize - k;
                double f = (double) kk * sampleRate / (double) fftSize;
                f = juce::jmin (f, nyq);
                const double m = juce::jlimit (mMin, mMax, magAt (f));
                spec[(size_t) k] = { m, 0.0 };   // real, zero-phase spectrum
            }

            transform (spec.data(), /*inverse*/ true);  // unnormalised inverse DFT

            const int    centre = latency;
            const double norm   = 1.0 / (double) fftSize; // normalise the inverse
            for (int j = 0; j < firLen; ++j)
            {
                const int src = (j - centre + fftSize) % fftSize;
                const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                       * j / (firLen - 1));
                kernel[(size_t) j] = spec[(size_t) src].real() * norm * w;
            }
            haveKernel = true;
        }

        void process (double* const* data, int numChannels, int numSamples) noexcept
        {
            const int nCh = juce::jmin (numChannels, channels);
            for (int ch = 0; ch < nCh; ++ch)
            {
                double* x = data[ch];
                auto& d = line[(size_t) ch];
                int p = pos[(size_t) ch];
                for (int n = 0; n < numSamples; ++n)
                {
                    d[(size_t) p] = x[n];
                    double acc = 0.0;
                    int q = p;
                    for (int j = 0; j < firLen; ++j)
                    {
                        acc += kernel[(size_t) j] * d[(size_t) q];
                        if (--q < 0) q += firLen;
                    }
                    if (++p >= firLen) p = 0;
                    x[n] = flushDenorm (acc);
                }
                pos[(size_t) ch] = p;
            }
        }

        void reset() noexcept
        {
            for (auto& d : line) d.fill (0.0);
            pos = { 0, 0 };
        }

    private:
        // In-place iterative radix-2 Cooley–Tukey FFT. `inverse` uses +2π/len;
        // neither direction is scaled — rebuild() divides by fftSize. Exact and
        // deterministic on every platform.
        static void transform (std::complex<double>* a, bool inverse) noexcept
        {
            const int n = fftSize;
            for (int i = 1, j = 0; i < n; ++i)
            {
                int bit = n >> 1;
                for (; j & bit; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) std::swap (a[i], a[j]);
            }
            for (int len = 2; len <= n; len <<= 1)
            {
                const double ang = 2.0 * juce::MathConstants<double>::pi / (double) len
                                   * (inverse ? 1.0 : -1.0);
                const std::complex<double> wlen (std::cos (ang), std::sin (ang));
                for (int i = 0; i < n; i += len)
                {
                    std::complex<double> w (1.0, 0.0);
                    for (int k = 0; k < len / 2; ++k)
                    {
                        const std::complex<double> u = a[i + k];
                        const std::complex<double> v = a[i + k + len / 2] * w;
                        a[i + k]             = u + v;
                        a[i + k + len / 2]   = u - v;
                        w *= wlen;
                    }
                }
            }
        }

        double sampleRate = 44100.0;
        int    channels   = 2;
        bool   haveKernel = false;

        std::array<std::complex<double>, fftSize> spec {};
        std::array<double, firLen> kernel {};
        std::array<std::array<double, firLen>, 2> line {};
        std::array<int, 2> pos { 0, 0 };
    };
}
