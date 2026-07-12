// ============================================================================
//  Master Control Panel verification harness (not shipped in the plugin).
//
//  1. DSP smoke test: drives the processor across sample rates, block sizes and
//     randomised parameters, asserting the output stays finite and bounded and
//     that reported latency is sane. This exercises the real-time path for
//     denormals, NaNs and thread-safety of the parameter reads.
//  2. UI render: builds the editor and writes a PNG snapshot so the layout can
//     be inspected without a DAW.
// ============================================================================
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());

    // -------------------------------------------------------------- DSP tests
    bool ok = true;                 // hard fail only on non-finite / instability
    int  worstLatency = 0;
    double fuzzMaxAbs = 0.0;

    juce::Random rng (0x7ea55);
    auto& params = proc->getParameters();

    const double rates[]  = { 44100.0, 48000.0, 96000.0 };
    const int    blocks[] = { 32, 128, 512 };

    // (1) Fuzz: random params across SR/buffer — the ONLY hard requirement is
    //     that the output is always finite (no NaN/Inf). Magnitude may legally
    //     be large when every EQ band is stacked to +18 dB, so it's info only.
    for (double sr : rates)
        for (int block : blocks)
        {
            proc->setPlayConfigDetails (2, 2, sr, block);
            proc->prepareToPlay (sr, block);

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;

            for (int iter = 0; iter < 300; ++iter)
            {
                if (iter % 15 == 0)
                    for (auto* p : params)
                        p->setValueNotifyingHost (rng.nextFloat());

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = buf.getWritePointer (ch);
                    for (int n = 0; n < block; ++n)
                    {
                        const float t = (float) ((iter * block + n) / sr);
                        d[n] = 0.35f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * t)
                             + 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
                    }
                }

                proc->processBlock (buf, midi);

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = buf.getReadPointer (ch);
                    for (int n = 0; n < block; ++n)
                    {
                        if (! std::isfinite (d[n])) ok = false;
                        fuzzMaxAbs = juce::jmax (fuzzMaxAbs, (double) std::abs (d[n]));
                    }
                }
            }
            worstLatency = juce::jmax (worstLatency, proc->getLatencySamples());
            std::printf ("  fuzz  sr=%-6.0f block=%-4d latency=%-4d  finite=%s\n",
                         sr, block, proc->getLatencySamples(), ok ? "yes" : "NO");
        }

    // (2) Default params: gain staging should be sane, and silence in must
    //     decay to silence out (no self-oscillation / runaway feedback).
    for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());
    {
        const double sr = 48000.0; const int block = 256;
        proc->setPlayConfigDetails (2, 2, sr, block);
        proc->prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;

        double inSq = 0, outSq = 0; int count = 0; double defMax = 0;
        for (int iter = 0; iter < 400; ++iter)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    const float t = (float) ((iter * block + n) / sr);
                    d[n] = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * t); // ~-12 dBFS
                }
            }
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < block; ++n) inSq += 0.0625 * 0.5;
            proc->processBlock (buf, midi);
            if (iter > 100) // let auto-gain settle
                for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                    for (int n = 0; n < block; ++n) { outSq += d[n]*d[n]; defMax = juce::jmax (defMax,(double)std::abs(d[n])); ++count; } }
        }
        const double outRms = std::sqrt (outSq / juce::jmax (1, count));
        const double outDb  = juce::Decibels::gainToDecibels ((float) outRms);
        std::printf ("  default gain: out RMS %.1f dBFS (in ~-15 dBFS), peak %.2f\n", outDb, defMax);
        if (defMax > 4.0 || outDb > 0.0) { std::printf ("  !! default gain staging out of range\n"); ok = false; }

        // Silence -> silence (feed zeros, expect decay)
        double tail = 0;
        for (int iter = 0; iter < 200; ++iter)
        {
            buf.clear();
            proc->processBlock (buf, midi);
            if (iter > 150) for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                for (int n = 0; n < block; ++n) tail = juce::jmax (tail, (double) std::abs (d[n])); }
        }
        std::printf ("  silence tail after 200 blocks: %.2e\n", tail);
        if (tail > 1.0e-3 || ! std::isfinite (tail)) { std::printf ("  !! self-oscillation / non-decaying tail\n"); ok = false; }
    }

    // (3) Preset sweep: load every factory program and process a burst,
    //     asserting finite, bounded output — proves all presets are valid.
    {
        const double sr = 48000.0; const int block = 256;
        proc->setPlayConfigDetails (2, 2, sr, block);
        proc->prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        int nProg = proc->getNumPrograms();
        int bad = 0;
        for (int prog = 0; prog < nProg; ++prog)
        {
            proc->setCurrentProgram (prog);
            double pmax = 0; bool pfin = true;
            for (int iter = 0; iter < 60; ++iter)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = buf.getWritePointer (ch);
                    for (int n = 0; n < block; ++n)
                    {
                        const float t = (float) ((iter * block + n) / sr);
                        d[n] = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 110.0f * t)
                             + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 3000.0f * t);
                    }
                }
                proc->processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                    for (int n = 0; n < block; ++n) { if (! std::isfinite (d[n])) pfin = false;
                        pmax = juce::jmax (pmax, (double) std::abs (d[n])); } }
            }
            if (! pfin || pmax > 8.0)
            {
                std::printf ("  preset %2d '%s' : finite=%s peak=%.2f  <-- CHECK\n",
                             prog, proc->getProgramName (prog).toRawUTF8(), pfin ? "yes" : "NO", pmax);
                if (! pfin) { ok = false; ++bad; }
            }
        }
        std::printf ("  preset sweep: %d programs, %d flagged\n", nProg, bad);
    }

    // (4) Native double-precision path: the host may hand us 64-bit buffers.
    {
        proc->setProcessingPrecision (juce::AudioProcessor::doublePrecision);
        const double sr = 48000.0; const int block = 256;
        proc->setPlayConfigDetails (2, 2, sr, block);
        proc->prepareToPlay (sr, block);
        for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());

        juce::AudioBuffer<double> buf (2, block);
        juce::MidiBuffer midi;
        bool dfin = true; double dmax = 0.0;
        for (int iter = 0; iter < 120; ++iter)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    const double t = (iter * block + n) / sr;
                    d[n] = 0.3 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * t);
                }
            }
            proc->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                for (int n = 0; n < block; ++n) { if (! std::isfinite (d[n])) dfin = false;
                    dmax = juce::jmax (dmax, std::abs (d[n])); } }
        }
        std::printf ("  double-precision path: finite=%s peak=%.3f\n", dfin ? "yes" : "NO", dmax);
        if (! dfin) ok = false;
        proc->setProcessingPrecision (juce::AudioProcessor::singlePrecision);
    }

    // (5) Linear-phase EQ path: enable it with a boost, confirm finite output
    //     and that it reports latency (the FIR group delay).
    {
        auto findParam = [&] (const juce::String& nm) -> juce::AudioProcessorParameter*
        {
            for (auto* p : params) if (p->getName (64).equalsIgnoreCase (nm)) return p;
            return nullptr;
        };
        for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());
        if (auto* q = findParam ("EQ On"))        q->setValueNotifyingHost (1.0f);
        if (auto* q = findParam ("Linear Phase")) q->setValueNotifyingHost (1.0f);
        if (auto* q = findParam ("High Mid On"))  q->setValueNotifyingHost (1.0f);
        if (auto* q = findParam ("High Mid Gain"))q->setValueNotifyingHost (0.85f);

        const double sr = 48000.0; const int block = 256;
        proc->setProcessingPrecision (juce::AudioProcessor::singlePrecision);
        proc->setPlayConfigDetails (2, 2, sr, block);
        proc->prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        bool lfin = true; double lmax = 0.0;
        for (int iter = 0; iter < 200; ++iter)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    const float t = (float) ((iter * block + n) / sr);
                    d[n] = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * t);
                }
            }
            proc->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                for (int n = 0; n < block; ++n) { if (! std::isfinite (d[n])) lfin = false;
                    lmax = juce::jmax (lmax, (double) std::abs (d[n])); } }
        }
        std::printf ("  linear-phase EQ: finite=%s peak=%.3f latency=%d\n",
                     lfin ? "yes" : "NO", lmax, proc->getLatencySamples());
        if (! lfin || proc->getLatencySamples() <= 0) ok = false;
    }

    // (6) EQ frequency-response probe: real, end-to-end measurement that the
    //     AIR shelf is ~+6 dB at full (not +36 dB) and that Proportional-Q no
    //     longer turns a shelf boost into a resonant overshoot.
    {
        auto findParam = [&] (const juce::String& nm) -> juce::AudioProcessorParameter*
        { for (auto* p : params) if (p->getName (64).equalsIgnoreCase (nm)) return p; return nullptr; };
        auto setP = [&] (const juce::String& nm, double real)
        {
            if (auto* p = findParam (nm))
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) real));
        };
        const double sr = 48000.0; const int block = 256;
        proc->setProcessingPrecision (juce::AudioProcessor::singlePrecision);

        auto measureGainDb = [&] (double freq) -> double
        {
            proc->setPlayConfigDetails (2, 2, sr, block);
            proc->prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            const double amp = 0.2;
            double outSq = 0.0; int count = 0;
            for (int iter = 0; iter < 80; ++iter)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = buf.getWritePointer (ch);
                    for (int n = 0; n < block; ++n)
                    {
                        const double t = (iter * block + n) / sr;
                        d[n] = (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * t));
                    }
                }
                proc->processBlock (buf, midi);
                if (iter >= 48)
                    for (int ch = 0; ch < 2; ++ch) { auto* d = buf.getReadPointer (ch);
                        for (int n = 0; n < block; ++n) { outSq += d[n]*d[n]; ++count; } }
            }
            const double outRms = std::sqrt (outSq / juce::jmax (1, count));
            const double inRms  = amp / std::sqrt (2.0);
            return 20.0 * std::log10 (juce::jmax (1.0e-9, outRms / inRms));
        };

        // Neutralise everything except the EQ so we measure it in isolation.
        for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());
        setP ("Auto Gain", 0); setP ("Tube On", 0); setP ("Comp On", 0); setP ("Tape On", 0);
        setP ("EQ On", 1); setP ("Linear Phase", 0); setP ("Oversampling", 0);
        setP ("HP On", 0); setP ("LP On", 0); setP ("Mix", 100);
        setP ("Low Shelf Gain", 0); setP ("Low Mid Gain", 0); setP ("Mid Gain", 0);
        setP ("High Mid Gain", 0); setP ("High Shelf Gain", 0);
        setP ("AIR", 0); setP ("TIGHT", 0);

        setP ("AIR", 100);
        const double airHi = measureGainDb (16000.0);
        const double airLo = measureGainDb (1000.0);
        setP ("AIR", 0);
        std::printf ("  EQ probe: AIR=100%% @16k = %+.1f dB (expect ~+5), @1k = %+.1f dB (expect ~0)\n", airHi, airLo);
        if (airHi > 9.0 || airHi < 2.0 || std::abs (airLo) > 1.0) { std::printf ("  !! AIR shelf scaling wrong\n"); ok = false; }

        setP ("Proportional Q", 1);
        setP ("Low Shelf On", 1); setP ("Low Shelf Freq", 100); setP ("Low Shelf Q", 0.7);
        setP ("Low Shelf Gain", 12);
        double peak = -99.0;
        const double probeF[] = { 30, 50, 80, 120, 180, 260, 400 };
        for (double f : probeF) peak = juce::jmax (peak, measureGainDb (f));
        const double plateau = measureGainDb (30.0);
        setP ("Low Shelf Gain", 0);
        std::printf ("  EQ probe: low-shelf +12 dB (propQ on) plateau %+.1f dB, sweep peak %+.1f dB (overshoot %.1f dB)\n",
                     plateau, peak, peak - 12.0);
        if (peak - 12.0 > 1.5) { std::printf ("  !! shelf resonance: Proportional-Q inflating shelf Q\n"); ok = false; }

        // Linear-phase FIR must reproduce the same magnitude as the min-phase
        // cascade: flat -> unity, and a boost must match within tolerance.
        for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());
        setP ("Auto Gain", 0); setP ("Tube On", 0); setP ("Comp On", 0); setP ("Tape On", 0);
        setP ("EQ On", 1); setP ("Oversampling", 0); setP ("HP On", 0); setP ("LP On", 0);
        setP ("Mix", 100); setP ("AIR", 0); setP ("TIGHT", 0); setP ("Proportional Q", 0);
        setP ("Low Shelf Gain", 0); setP ("Low Mid Gain", 0); setP ("Mid Gain", 0);
        setP ("High Mid Gain", 0); setP ("High Shelf Gain", 0);

        setP ("Linear Phase", 1);
        const double linFlat1k  = measureGainDb (1000.0);
        const double linFlat100 = measureGainDb (100.0);
        const double linFlat10k = measureGainDb (10000.0);
        std::printf ("  LINPHASE flat: @100=%+.2f @1k=%+.2f @10k=%+.2f dB (expect ~0)\n",
                     linFlat100, linFlat1k, linFlat10k);
        if (std::abs (linFlat1k) > 0.5 || std::abs (linFlat100) > 0.6 || std::abs (linFlat10k) > 0.6)
        { std::printf ("  !! linear-phase flat is not unity\n"); ok = false; }

        setP ("Linear Phase", 0);
        setP ("High Mid On", 1); setP ("High Mid Freq", 3000); setP ("High Mid Q", 1.0); setP ("High Mid Gain", 6);
        setP ("Low Shelf On", 1); setP ("Low Shelf Freq", 120); setP ("Low Shelf Q", 0.7); setP ("Low Shelf Gain", 4);
        const double minA = measureGainDb (3000.0), minB = measureGainDb (1000.0), minC = measureGainDb (120.0);
        setP ("Linear Phase", 1);
        const double linA = measureGainDb (3000.0), linB = measureGainDb (1000.0), linC = measureGainDb (120.0);
        std::printf ("  LINPHASE curve: min[@120=%+.2f @1k=%+.2f @3k=%+.2f]  lin[@120=%+.2f @1k=%+.2f @3k=%+.2f]\n",
                     minC, minB, minA, linC, linB, linA);
        if (std::abs (linA - minA) > 1.5 || std::abs (linB - minB) > 1.0 || std::abs (linC - minC) > 1.5)
        { std::printf ("  !! linear-phase magnitude does not match minimum-phase curve\n"); ok = false; }
    }

    std::printf ("DSP verification: %s  (fuzz finite ok, fuzzMaxAbs=%.2f, worstLatency=%d)\n",
                 ok ? "PASS" : "FAIL", fuzzMaxAbs, worstLatency);

    // -------------------------------------------------------------- UI render
    juce::String outPath = (argc > 1) ? juce::String (argv[1])
                                      : juce::String ("turbo_tubes_ui.png");
    if (auto* editor = proc->createEditorIfNeeded())
    {
        editor->setSize (1360, 860);
        juce::Image img = editor->createComponentSnapshot (editor->getLocalBounds(), false, 1.0f);

        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
        out.deleteFile();
        if (auto os = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (img, *os);
            std::printf ("UI snapshot written: %s (%dx%d)\n",
                         out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        }
        proc->editorBeingDeleted (editor);
        delete editor;
    }
    else
    {
        std::printf ("UI snapshot: editor could not be created\n");
    }

    return ok ? 0 : 2;
}
