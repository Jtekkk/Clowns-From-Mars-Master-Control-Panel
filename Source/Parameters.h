#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Central registry of every automatable parameter in Master Control Panel.

    Keeping the IDs, ranges and layout in one place means the processor, the
    editor and the preset system can never disagree about what a control is.
    All ranges are chosen to be *musical* — the useful part of each control's
    travel maps to the sweet spot of the underlying DSP, not to raw math units.
*/
namespace cfm::params
{
    // Bump this whenever the parameter layout changes in a way that would
    // invalidate saved automation, so hosts migrate cleanly.
    inline constexpr int layoutVersion = 1;

    namespace id
    {
        // ---- Global / IO -----------------------------------------------------
        inline constexpr auto inputTrim   = "inputTrim";
        inline constexpr auto outputTrim  = "outputTrim";
        inline constexpr auto headroom    = "headroom";
        inline constexpr auto mix         = "mix";
        inline constexpr auto bypass      = "bypass";
        inline constexpr auto delta       = "delta";
        inline constexpr auto autoGain    = "autoGain";
        inline constexpr auto oversample  = "oversample";
        inline constexpr auto drift       = "drift";
        inline constexpr auto circuitBend = "circuitBend";

        // ---- Tube / MOJO drive stage ----------------------------------------
        inline constexpr auto tubeOn      = "tubeOn";
        inline constexpr auto tubeDrive   = "tubeDrive";
        inline constexpr auto tubeBias    = "tubeBias";
        inline constexpr auto tubeModel   = "tubeModel";   // choice
        inline constexpr auto tubeTone    = "tubeTone";     // spectral tilt

        // ---- Equaliser -------------------------------------------------------
        inline constexpr auto eqOn        = "eqOn";
        inline constexpr auto hpOn        = "hpOn";
        inline constexpr auto hpFreq      = "hpFreq";
        inline constexpr auto lpOn        = "lpOn";
        inline constexpr auto lpFreq      = "lpFreq";
        inline constexpr auto propQ       = "propQ";        // proportional-Q mode
        inline constexpr auto eqLinear    = "eqLinear";     // linear-phase EQ mode
        inline constexpr auto air         = "air";          // AIR shelf
        inline constexpr auto tight       = "tight";        // TIGHT low clean-up

        // Five bands: 0 low-shelf, 1..3 peak, 4 high-shelf.
        inline constexpr const char* bandFreq[5] = { "b0Freq", "b1Freq", "b2Freq", "b3Freq", "b4Freq" };
        inline constexpr const char* bandGain[5] = { "b0Gain", "b1Gain", "b2Gain", "b3Gain", "b4Gain" };
        inline constexpr const char* bandQ   [5] = { "b0Q",    "b1Q",    "b2Q",    "b3Q",    "b4Q"    };
        inline constexpr const char* bandOn  [5] = { "b0On",   "b1On",   "b2On",   "b3On",   "b4On"   };

        // ---- Compressor ------------------------------------------------------
        inline constexpr auto compOn       = "compOn";
        inline constexpr auto compThresh   = "compThresh";
        inline constexpr auto compRatio    = "compRatio";
        inline constexpr auto compAttack   = "compAttack";
        inline constexpr auto compRelease  = "compRelease";
        inline constexpr auto compKnee     = "compKnee";
        inline constexpr auto compMakeup   = "compMakeup";
        inline constexpr auto compAutoMk   = "compAutoMk";
        inline constexpr auto compMode     = "compMode";     // choice
        inline constexpr auto compOptBlend = "compOptBlend"; // opto<->discrete
        inline constexpr auto compMix      = "compMix";      // parallel
        inline constexpr auto scHpf        = "scHpf";        // detector HP
        inline constexpr auto transformer  = "transformer";  // choice

        // ---- Tape ------------------------------------------------------------
        inline constexpr auto tapeOn     = "tapeOn";
        inline constexpr auto tapeDrive  = "tapeDrive";
        inline constexpr auto tapeSpeed  = "tapeSpeed";  // choice
        inline constexpr auto tapeLF     = "tapeLF";
        inline constexpr auto tapeHF     = "tapeHF";

        // ---- Stereo ----------------------------------------------------------
        inline constexpr auto width    = "width";
        inline constexpr auto monoFreq = "monoFreq";
    }

    // Choice orderings — referenced by both DSP and GUI.
    enum class TubeModel   { triode = 0, pentode, starved };
    enum class CompMode    { stereo = 0, dualMono, midSide };
    enum class Transformer { nickel = 0, iron, steel };
    enum class TapeSpeed   { ips15 = 0, ips30 };
    enum class Oversample  { off = 0, x2, x4, x8 };

    inline juce::StringArray tubeModelChoices()   { return { "Triode", "Pentode", "Starved" }; }
    inline juce::StringArray compModeChoices()    { return { "Stereo", "Dual-Mono", "Mid/Side" }; }
    inline juce::StringArray transformerChoices() { return { "Nickel", "Iron", "Steel" }; }
    inline juce::StringArray tapeSpeedChoices()   { return { "15 ips", "30 ips" }; }
    inline juce::StringArray oversampleChoices()  { return { "Off", "2x", "4x", "8x" }; }

    // Convenience skew for frequency sliders (perceptually log).
    inline juce::NormalisableRange<float> freqRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
