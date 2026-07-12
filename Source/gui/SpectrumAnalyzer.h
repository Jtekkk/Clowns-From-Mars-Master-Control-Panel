#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/Metering.h"
#include "Theme.h"
#include <functional>

namespace tt::gui
{
    /**
        Real-time FFT spectrum with the live EQ response curve drawn on top.

        The audio thread feeds a lock-free FIFO; here we drain the newest block,
        Hann-window and transform it, average into a log-frequency display, and
        overlay the EQ magnitude so you can see filters land where you place
        them. Purely visual — it never touches the audio path.
    */
    class SpectrumAnalyzer : public juce::Component, private juce::Timer
    {
    public:
        SpectrumAnalyzer (tt::dsp::AnalyzerFifo& fifoIn,
                          std::function<double (double)> eqMagDbAt);
        ~SpectrumAnalyzer() override { stopTimer(); }

        void paint (juce::Graphics&) override;
        void setSampleRate (double sr) noexcept { sampleRate = sr; }

    private:
        void timerCallback() override;
        float freqToX (float hz, juce::Rectangle<float>) const;
        float dbToY  (float db, juce::Rectangle<float>) const;

        tt::dsp::AnalyzerFifo& fifo;
        std::function<double (double)> eqMagAt;

        juce::dsp::FFT fft { tt::dsp::AnalyzerFifo::fftOrder };
        juce::dsp::WindowingFunction<float> window { (size_t) tt::dsp::AnalyzerFifo::fftSize,
                                                     juce::dsp::WindowingFunction<float>::hann };

        static constexpr int fftSize = tt::dsp::AnalyzerFifo::fftSize;
        std::array<float, (size_t) fftSize * 2> fftData { {} };
        std::array<float, 256> display { {} };   // smoothed magnitude per display bin

        static constexpr float minDb = -90.0f, maxDb = 6.0f;
        static constexpr float minHz = 20.0f,  maxHz = 20000.0f;
        double sampleRate = 48000.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
    };
}
