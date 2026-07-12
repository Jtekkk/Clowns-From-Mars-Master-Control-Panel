#pragma once

#include "DspCommon.h"
#include "DriftModel.h"
#include <array>

namespace tt::dsp
{
    /**
        Two-stage, transformer-balanced mastering compressor.

        Topology (decoupled, "smooth-the-gain"):
          detector (RMS, sidechain-HP'd)  ->  soft-knee gain computer
             ->  TWO parallel envelopes:
                   * discrete : your Attack/Release, precise
                   * optical  : program-dependent release, gluey
             ->  blended, applied, makeup, then transformer colour.

        Modes: Stereo (linked), Dual-Mono (independent), Mid/Side.
        Character: Nickel (clean/airy), Iron (mid focus), Steel (harmonic).
        Program-dependent optical release models a real photocell's memory,
        which is why it behaves differently from a static ratio curve.
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
            for (auto& v : envD) v = 0.0f;
            for (auto& v : envO) v = 0.0f;
            for (auto& v : rms)  v = 0.0f;
            for (auto& f : scHp) f.reset();
            for (auto& z : colZ) z = 0.0f;
            grReadout = 0.0f;
        }

        void setDrift (const DriftModel* d) noexcept { drift = d; }

        struct Settings
        {
            bool  on = false;
            float threshold = -12.f, ratio = 2.f, attackMs = 20.f, releaseMs = 200.f, knee = 6.f;
            float makeup = 0.f;  bool autoMakeup = true;
            int   mode = 0;       // 0 stereo, 1 dual-mono, 2 mid/side
            float optBlend = 0.5f; // 0 discrete .. 1 optical
            float mix = 1.0f;      // parallel
            float scHpFreq = 20.f;
            int   transformer = 0; // 0 nickel, 1 iron, 2 steel
        };

        void setSettings (const Settings& s) noexcept
        {
            cfg = s;
            // Ballistic coefficients.
            atkD = std::exp (-1.0f / (juce::jmax (0.05f, cfg.attackMs)  * 0.001f * (float) sampleRate));
            relD = std::exp (-1.0f / (juce::jmax (1.0f,  cfg.releaseMs) * 0.001f * (float) sampleRate));
            atkO = std::exp (-1.0f / (juce::jmax (5.0f,  cfg.attackMs * 1.6f) * 0.001f * (float) sampleRate));
            rmsA = std::exp (-1.0f / (0.003f * (float) sampleRate)); // ~3 ms RMS window

            // Static auto-makeup: restore roughly half of the curve's pull on a 0 dB peak.
            const float grAt0 = computeGR (0.0f);
            autoMk = dbToGain (-0.5f * grAt0);

            if (cfg.scHpFreq != lastScFreq) { lastScFreq = cfg.scHpFreq; updateDetectorFilter(); }
        }

        // scData may be nullptr (no external sidechain).
        void process (float* const* data, int numChannels, int numSamples,
                      const float* const* scData, int scChannels) noexcept
        {
            if (! cfg.on) { grReadout = 0.0f; return; }

            const int nCh = juce::jmin (numChannels, channels);
            const bool useExt = (scData != nullptr && scChannels > 0);
            float dryMix, wetMix; equalPower (cfg.mix, dryMix, wetMix);

            float blockGrDb = 0.0f;

            for (int n = 0; n < numSamples; ++n)
            {
                // --- Build voice signals & detector keys per mode -----------
                float vSig[2] { 0.f, 0.f }, vKey[2] { 0.f, 0.f };
                int   voices  = 1;

                float inL = data[0][n];
                float inR = (nCh > 1 ? data[1][n] : inL);

                float scL = useExt ? scData[0][n] : inL;
                float scR = useExt ? (scChannels > 1 ? scData[1][n] : scL) : inR;
                scL = scHp[0].processSample (scL);
                scR = scHp[1].processSample (scR);

                switch (cfg.mode)
                {
                    case 2: // Mid/Side
                        voices  = 2;
                        vSig[0] = 0.5f * (inL + inR); vSig[1] = 0.5f * (inL - inR);
                        vKey[0] = 0.5f * (scL + scR); vKey[1] = 0.5f * (scL - scR);
                        break;
                    case 1: // Dual-Mono
                        voices  = 2;
                        vSig[0] = inL; vSig[1] = inR;
                        vKey[0] = scL; vKey[1] = scR;
                        break;
                    default: // Stereo (linked)
                        voices  = 1;
                        vSig[0] = 0.f; // unused; we apply the shared gain to L/R directly
                        vKey[0] = juce::jmax (std::abs (scL), std::abs (scR));
                        break;
                }

                float gain[2] { 1.f, 1.f };
                for (int v = 0; v < voices; ++v)
                {
                    // RMS-ish detector.
                    const float key = (cfg.mode == 0) ? vKey[0] : vKey[v];
                    rms[v] = rmsA * rms[v] + (1.0f - rmsA) * (key * key) + antiDenormal;
                    const float levelDb = 10.0f * std::log10 (juce::jmax (1.0e-9f, rms[v]));

                    const float targetGr = computeGR (levelDb); // <= 0 dB

                    // Discrete envelope (user ballistics).
                    if (targetGr < envD[v]) envD[v] = atkD * envD[v] + (1.0f - atkD) * targetGr;
                    else                    envD[v] = relD * envD[v] + (1.0f - relD) * targetGr;

                    // Optical envelope: program-dependent release.
                    const float depth   = juce::jlimit (0.0f, 1.0f, -targetGr / 24.0f);
                    const float relMsO  = juce::jmap (depth, 60.0f, 900.0f); // longer tail when hit harder
                    const float relO    = std::exp (-1.0f / (relMsO * 0.001f * (float) sampleRate));
                    if (targetGr < envO[v]) envO[v] = atkO * envO[v] + (1.0f - atkO) * targetGr;
                    else                    envO[v] = relO * envO[v] + (1.0f - relO) * targetGr;

                    const float grDb = envD[v] * (1.0f - cfg.optBlend) + envO[v] * cfg.optBlend;
                    gain[v] = dbToGain (grDb);
                    blockGrDb = juce::jmin (blockGrDb, grDb);
                }

                const float makeup = cfg.autoMakeup ? autoMk : dbToGain (cfg.makeup);

                // --- Apply, colour, recombine, parallel-mix -----------------
                float outL, outR;
                if (cfg.mode == 2) // M/S
                {
                    const float m = colour (vSig[0] * gain[0] * makeup, 0);
                    const float s = colour (vSig[1] * gain[1] * makeup, 1);
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
        float getGainReductionDb() const noexcept { return grReadout; }

    private:
        static constexpr int kMax = 2;

        float computeGR (float levelDb) const noexcept
        {
            const float over = levelDb - cfg.threshold;
            const float W    = juce::jmax (0.0001f, cfg.knee);
            const float slope = (1.0f / cfg.ratio) - 1.0f; // <= 0
            if (over <= -W * 0.5f) return 0.0f;
            if (over >=  W * 0.5f) return slope * over;
            const float x = over + W * 0.5f;
            return slope * (x * x) / (2.0f * W);
        }

        // Transformer voicing — subtle, program-following colour on the output.
        float colour (float x, int ch) noexcept
        {
            const float d = drift ? drift->factor (ch, 5, 0.04f) : 1.0f;
            float drive, tiltHz, tiltGain;
            switch (cfg.transformer)
            {
                case 1: drive = 1.6f; tiltHz = 900.f;  tiltGain = 1.10f; break; // Iron: mids
                case 2: drive = 2.4f; tiltHz = 1500.f; tiltGain = 1.00f; break; // Steel: harmonics
                default: drive = 1.1f; tiltHz = 6000.f; tiltGain = 1.08f; break; // Nickel: air
            }
            drive *= d;
            const float sat = fastTanh (x * drive) / drive;

            // one-pole low/high split for a gentle voicing tilt
            const float a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * tiltHz / (float) sampleRate);
            colZ[ch] = colZ[ch] + a * (sat - colZ[ch]);
            const float low = colZ[ch], high = sat - low;
            return low + high * tiltGain;
        }

        void updateDetectorFilter() noexcept
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                        sampleRate, juce::jlimit (20.0f, (float) (sampleRate * 0.45), cfg.scHpFreq), 0.707f);
            for (auto& f : scHp) f.coefficients = c;
        }

        double sampleRate = 44100.0;
        int    channels   = 2;

        Settings cfg;
        float atkD = 0.f, relD = 0.f, atkO = 0.f, rmsA = 0.f, autoMk = 1.0f, lastScFreq = -1.f;

        std::array<float, kMax> envD { {} }, envO { {} }, rms { {} }, colZ { {} };
        std::array<juce::dsp::IIR::Filter<float>, kMax> scHp;
        const DriftModel* drift = nullptr;
        float grReadout = 0.0f;
    };
}
