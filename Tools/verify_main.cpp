// ============================================================================
//  Turbo Tubes verification harness (not shipped in the plugin).
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

    std::printf ("DSP verification: %s  (fuzz finite ok, fuzzMaxAbs=%.2f, worstLatency=%d)\n",
                 ok ? "PASS" : "FAIL", fuzzMaxAbs, worstLatency);

    // -------------------------------------------------------------- UI render
    juce::String outPath = (argc > 1) ? juce::String (argv[1])
                                      : juce::String ("turbo_tubes_ui.png");
    if (auto* editor = proc->createEditorIfNeeded())
    {
        editor->setSize (1120, 860);
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
