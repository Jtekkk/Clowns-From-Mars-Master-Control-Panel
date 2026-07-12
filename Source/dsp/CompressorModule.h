#pragma once

#include "DspCommon.h"
#include "DriftModel.h"
#include <array>

namespace cfm::dsp
{
    /**
        Two-stage, transformer-balanced mastering compressor (64-bit).

        Topology (decoupled, "smooth-the-gain"):
          detector (RMS, sidechain-HP'd)  ->  soft-knee gain computer
             ->  TWO parallel envelopes:
                   * discrete : your Attack/Release, precise
                   * optical  : program-dependent release, gluey
             ->  blended, applied, makeup, then transformer colour.

        Everything — detector, envelopes, gain computer — runs in double so the
        gain signal is smooth to the bit and never quantises the low-level tail.
        Modes: Stereo (linked), Dual-Mono, Mid/Side. Character: Nickel (air),
        Iron (mids), Steel (harmonics).
    */
    class CompressorModule
    {
    public:
        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, kMax, numChannels);
            reset();
            updateDetectorFilter();
        }

        void reset() noexcept
        {
            for (auto& v : envD) v = 0.0;
            for (auto& v : envO) v = 0.0;
            for (auto& v : rms)  v = 0.0;
            for (auto& f : scHp) f.reset();
            for (auto& z : colZ) z = 0.0;
            grReadout = 0.0;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        struct Settings
        {
            bool   on = false;
            double threshold = -12.0, ratio = 2.0, attackMs = 20.0, releaseMs = 200.0, knee = 6.0;
            double makeup = 0.0;  bool autoMakeup = true;
            int    mode = 0;       // 0 stereo, 1 dual-mono, 2 mid/side
            double optBlend = 0.5; // 0 discrete .. 1 optical
            double mix = 1.0;      // parallel
            double scHpFreq = 20.0;
            int    transformer = 0; // 0 nickel, 1 iron, 2 steel
        };

        void setSettings (const Settings& s) noexcept
        {
            cfg = s;
            // Ballistic coefficients.
            atkD = std::exp (-1.0 / (juce::jmax (0.05, cfg.attackMs)  * 0.001 * sampleRate));
            relD = std::exp (-1.0 / (juce::jmax (1.0,  cfg.releaseMs) * 0.001 * sampleRate));
            atkO = std::exp (-1.0 / (juce::jmax (5.0,  cfg.attackMs * 1.6) * 0.001 * sampleRate));
            rmsA = std::exp (-1.0 / (0.003 * sampleRate)); // ~3 ms RMS window

            // Static auto-makeup: restore roughly half of the curve's pull on a 0 dB peak.
            const double grAt0 = computeGR (0.0);
            autoMk = dbToGain (-0.5 * grAt0);

            if (cfg.scHpFreq != lastScFreq) { lastScFreq = cfg.scHpFreq; updateDetectorFilter(); }
        }

        // scData may be nullptr (no external sidechain).
        void process (double* const* data, int numChannels, int numSamples,
                      const double* const* scData, int scChannels) noexcept
        {
            if (! cfg.on) { grReadout = 0.0; return; }

            const int nCh = juce::jmin (numChannels, channels);
            const bool useExt = (scData != nullptr && scChannels > 0);
            double dryMix, wetMix; equalPower (cfg.mix, dryMix, wetMix);

            double blockGrDb = 0.0;

            for (int n = 0; n < numSamples; ++n)
            {
                double vSig[2] { 0.0, 0.0 }, vKey[2] { 0.0, 0.0 };
                int    voices  = 1;

                const double inL = data[0][n];
                const double inR = (nCh > 1 ? data[1][n] : inL);

                double scL = useExt ? scData[0][n] : inL;
                double scR = useExt ? (scChannels > 1 ? scData[1][n] : scL) : inR;
                scL = scHp[0].processSample (scL);
                scR = scHp[1].processSample (scR);

                switch (cfg.mode)
                {
                    case 2: // Mid/Side
                        voices  = 2;
                        vSig[0] = 0.5 * (inL + inR); vSig[1] = 0.5 * (inL - inR);
                        vKey[0] = 0.5 * (scL + scR); vKey[1] = 0.5 * (scL - scR);
                        break;
                    case 1: // Dual-Mono
                        voices  = 2;
                        vSig[0] = inL; vSig[1] = inR;
                        vKey[0] = scL; vKey[1] = scR;
                        break;
                    default: // Stereo (linked)
                        voices  = 1;
                        vKey[0] = juce::jmax (std::abs (scL), std::abs (scR));
                        break;
                }

                double gain[2] { 1.0, 1.0 };
                for (int v = 0; v < voices; ++v)
                {
                    const double key = (cfg.mode == 0) ? vKey[0] : vKey[v];
                    rms[v] = rmsA * rms[v] + (1.0 - rmsA) * (key * key) + antiDenormal;
                    const double levelDb = 10.0 * std::log10 (juce::jmax (1.0e-12, rms[v]));

                    const double targetGr = computeGR (levelDb); // <= 0 dB

                    if (targetGr < envD[v]) envD[v] = atkD * envD[v] + (1.0 - atkD) * targetGr;
                    else                    envD[v] = relD * envD[v] + (1.0 - relD) * targetGr;

                    const double depth  = juce::jlimit (0.0, 1.0, -targetGr / 24.0);
                    const double relMsO = juce::jmap (depth, 60.0, 900.0);
                    const double relO   = std::exp (-1.0 / (relMsO * 0.001 * sampleRate));
                    if (targetGr < envO[v]) envO[v] = atkO * envO[v] + (1.0 - atkO) * targetGr;
                    else                    envO[v] = relO * envO[v] + (1.0 - relO) * targetGr;

                    const double grDb = envD[v] * (1.0 - cfg.optBlend) + envO[v] * cfg.optBlend;
                    gain[v] = dbToGain (grDb);
                    blockGrDb = juce::jmin (blockGrDb, grDb);
                }

                const double makeup = cfg.autoMakeup ? autoMk : dbToGain (cfg.makeup);

                double outL, outR;
                if (cfg.mode == 2) // M/S
                {
                    const double m = colour (vSig[0] * gain[0] * makeup, 0);
                    const double s = colour (vSig[1] * gain[1] * makeup, 1);
                    outL = m + s; outR = m - s;
                }
                else if (cfg.mode == 1) // dual-mono
                {
                    outL = colour (vSig[0] * gain[0] * makeup, 0);
                    outR = colour (vSig[1] * gain[1] * makeup, 1);
                }
                else // stereo linked
                {
                    outL = colour (inL * gain[0] * makeup, 0);
                    outR = colour (inR * gain[0] * makeup, 1);
                }

                data[0][n] = flushDenorm (dryMix * inL + wetMix * outL);
                if (nCh > 1) data[1][n] = flushDenorm (dryMix * inR + wetMix * outR);
            }

            for (auto& f : scHp) f.snapToZero();
            grReadout = blockGrDb; // most reduction this block (<= 0 dB)
        }

        // For the gain-reduction meter (dB, <= 0).
        double getGainReductionDb() const noexcept { return grReadout; }

    private:
        static constexpr int kMax = 2;

        double computeGR (double levelDb) const noexcept
        {
            const double over  = levelDb - cfg.threshold;
            const double W     = juce::jmax (0.0001, cfg.knee);
            const double slope = (1.0 / cfg.ratio) - 1.0; // <= 0
            if (over <= -W * 0.5) return 0.0;
            if (over >=  W * 0.5) return slope * over;
            const double x = over + W * 0.5;
            return slope * (x * x) / (2.0 * W);
        }

        // Transformer voicing — subtle, program-following colour on the output.
        double colour (double x, int ch) noexcept
        {
            const double d = drift ? drift->factor (ch, 5, 0.04) : 1.0;
            double drive, tiltHz, tiltGain;
            switch (cfg.transformer)
            {
                case 1: drive = 1.6; tiltHz = 900.0;  tiltGain = 1.10; break; // Iron: mids
                case 2: drive = 2.4; tiltHz = 1500.0; tiltGain = 1.00; break; // Steel: harmonics
                default: drive = 1.1; tiltHz = 6000.0; tiltGain = 1.08; break; // Nickel: air
            }
            drive *= d;
            const double sat = saturate (x * drive) / drive;

            const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * tiltHz / sampleRate);
            colZ[ch] = colZ[ch] + a * (sat - colZ[ch]);
            const double low = colZ[ch], high = sat - low;
            return low + high * tiltGain;
        }

        void updateDetectorFilter() noexcept
        {
            auto c = juce::dsp::IIR::Coefficients<double>::makeHighPass (
                        sampleRate, juce::jlimit (20.0, sampleRate * 0.45, cfg.scHpFreq), 0.707);
            for (auto& f : scHp) f.coefficients = c;
        }

        double sampleRate = 44100.0;
        int    channels   = 2;

        Settings cfg;
        double atkD = 0.0, relD = 0.0, atkO = 0.0, rmsA = 0.0, autoMk = 1.0, lastScFreq = -1.0;

        std::array<double, kMax> envD { {} }, envO { {} }, rms { {} }, colZ { {} };
        std::array<juce::dsp::IIR::Filter<double>, kMax> scHp;
        const DriftModel* drift = nullptr;
        double grReadout = 0.0;
    };
}
