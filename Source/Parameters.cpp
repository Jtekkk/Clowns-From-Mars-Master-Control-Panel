#include "Parameters.h"

namespace cfm::params
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using Attr  = juce::AudioParameterFloatAttributes;
    using PID   = juce::ParameterID;

    static PID pid (const char* s) { return PID { s, layoutVersion }; }

    static Attr db()      { return Attr().withLabel ("dB"); }
    static Attr pct()     { return Attr().withLabel ("%"); }
    static Attr hz()      { return Attr().withLabel ("Hz"); }
    static Attr ms()      { return Attr().withLabel ("ms"); }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        auto add = [&] (auto* p) { layout.add (std::unique_ptr<std::remove_pointer_t<decltype (p)>> (p)); };

        // ---------------------------------------------------------------- Global
        add (new APF (pid (id::inputTrim),  "Input Trim",  juce::NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f, db()));
        add (new APF (pid (id::outputTrim), "Output Trim", juce::NormalisableRange<float> (-10.f, 10.f, 0.01f), 0.f, db()));
        add (new APF (pid (id::headroom),   "Headroom",    juce::NormalisableRange<float> (-18.f, 18.f, 0.01f), 0.f, db()));
        add (new APF (pid (id::mix),        "Mix",         juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f, pct()));
        add (new APB (pid (id::bypass),     "Bypass",   false));
        add (new APB (pid (id::delta),      "Delta",    false));
        add (new APB (pid (id::autoGain),   "Auto Gain", true));
        add (new APC (pid (id::oversample), "Oversampling", oversampleChoices(), (int) Oversample::x2));
        add (new APF (pid (id::drift),      "Drift",       juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 25.f, pct()));
        add (new APF (pid (id::circuitBend),"Circuit Bend",juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f, pct()));

        // ---------------------------------------------------------------- Tube
        add (new APB (pid (id::tubeOn),    "Tube On",   true));
        add (new APF (pid (id::tubeDrive), "Tube Drive",juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 35.f, pct()));
        add (new APF (pid (id::tubeBias),  "Tube Bias", juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f, pct()));
        add (new APC (pid (id::tubeModel), "MOJO Amp",  tubeModelChoices(), (int) TubeModel::triode));
        add (new APF (pid (id::tubeTone),  "Tube Tone", juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f, pct()));

        // ---------------------------------------------------------------- EQ
        add (new APB (pid (id::eqOn),  "EQ On",  true));
        add (new APB (pid (id::hpOn),  "HP On",  false));
        add (new APF (pid (id::hpFreq),"High-Pass", freqRange (16.f, 400.f, 80.f), 30.f, hz()));
        add (new APB (pid (id::lpOn),  "LP On",  false));
        add (new APF (pid (id::lpFreq),"Low-Pass",  freqRange (2000.f, 22000.f, 12000.f), 20000.f, hz()));
        add (new APB (pid (id::propQ), "Proportional Q", true));
        add (new APF (pid (id::air),   "AIR",   juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f, pct()));
        add (new APF (pid (id::tight), "TIGHT", juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f, pct()));

        // Sensible default centre frequencies / Qs for a mastering EQ.
        const float  defFreq[5] = { 90.f, 300.f, 1200.f, 5000.f, 12000.f };
        const float  defQ  [5] = { 0.71f, 0.9f, 0.9f, 0.9f, 0.71f };
        const char*  bandNm[5] = { "Low Shelf", "Low Mid", "Mid", "High Mid", "High Shelf" };
        for (int b = 0; b < 5; ++b)
        {
            add (new APF (pid (id::bandFreq[b]), juce::String (bandNm[b]) + " Freq",
                          freqRange (20.f, 20000.f, defFreq[b]), defFreq[b], hz()));
            add (new APF (pid (id::bandGain[b]), juce::String (bandNm[b]) + " Gain",
                          juce::NormalisableRange<float> (-18.f, 18.f, 0.01f), 0.f, db()));
            add (new APF (pid (id::bandQ[b]),    juce::String (bandNm[b]) + " Q",
                          juce::NormalisableRange<float> (0.1f, 10.f, 0.001f, 0.35f), defQ[b]));
            add (new APB (pid (id::bandOn[b]),   juce::String (bandNm[b]) + " On", true));
        }

        // ---------------------------------------------------------------- Comp
        add (new APB (pid (id::compOn),      "Comp On",   false));
        add (new APF (pid (id::compThresh),  "Threshold", juce::NormalisableRange<float> (-40.f, 0.f, 0.01f), -12.f, db()));
        add (new APF (pid (id::compRatio),   "Ratio",     juce::NormalisableRange<float> (1.f, 20.f, 0.01f, 0.5f), 2.f, Attr().withLabel (":1")));
        add (new APF (pid (id::compAttack),  "Attack",    juce::NormalisableRange<float> (0.1f, 200.f, 0.01f, 0.35f), 20.f, ms()));
        add (new APF (pid (id::compRelease), "Release",   juce::NormalisableRange<float> (10.f, 2000.f, 0.1f, 0.35f), 200.f, ms()));
        add (new APF (pid (id::compKnee),    "Knee",      juce::NormalisableRange<float> (0.f, 18.f, 0.01f), 6.f, db()));
        add (new APF (pid (id::compMakeup),  "Makeup",    juce::NormalisableRange<float> (-12.f, 24.f, 0.01f), 0.f, db()));
        add (new APB (pid (id::compAutoMk),  "Auto Makeup", true));
        add (new APC (pid (id::compMode),    "Comp Mode", compModeChoices(), (int) CompMode::stereo));
        add (new APF (pid (id::compOptBlend),"Opto/Disc", juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f, pct()));
        add (new APF (pid (id::compMix),     "Comp Mix",  juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f, pct()));
        add (new APF (pid (id::scHpf),       "SC HP",     freqRange (20.f, 500.f, 100.f), 20.f, hz()));
        add (new APC (pid (id::transformer), "Transformer", transformerChoices(), (int) Transformer::nickel));

        // ---------------------------------------------------------------- Tape
        add (new APB (pid (id::tapeOn),    "Tape On",   false));
        add (new APF (pid (id::tapeDrive), "Tape Drive",juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 30.f, pct()));
        add (new APC (pid (id::tapeSpeed), "Tape Speed",tapeSpeedChoices(), (int) TapeSpeed::ips30));
        add (new APF (pid (id::tapeLF),    "Tape LF",   juce::NormalisableRange<float> (-6.f, 6.f, 0.01f), 0.f, db()));
        add (new APF (pid (id::tapeHF),    "Tape HF",   juce::NormalisableRange<float> (-6.f, 6.f, 0.01f), 0.f, db()));

        // ---------------------------------------------------------------- Stereo
        add (new APF (pid (id::width),    "Width",      juce::NormalisableRange<float> (0.f, 200.f, 0.1f), 100.f, pct()));
        add (new APF (pid (id::monoFreq), "Mono Maker", juce::NormalisableRange<float> (0.f, 400.f, 1.f), 0.f, hz()));

        return layout;
    }
}
