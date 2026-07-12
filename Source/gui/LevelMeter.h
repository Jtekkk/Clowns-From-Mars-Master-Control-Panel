#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Metering.h"
#include "Theme.h"

namespace cfm::gui
{
    /**
        Vertical stereo output meter: filled RMS bar with a floating peak-hold
        cap, calibrated in dBFS from -60 to +6. Reference marks at -18 dBFS
        (a common mastering alignment point) and 0 dBFS keep the read honest.
    */
    class LevelMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit LevelMeter (cfm::dsp::MeterBallistics& src) : meter (src)
        {
            startTimerHz (30);
        }
        ~LevelMeter() override { stopTimer(); }

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        float dbToY (float db, juce::Rectangle<float> area) const;

        cfm::dsp::MeterBallistics& meter;
        float peakHold[2] { -100.f, -100.f };
        int   holdCount[2] { 0, 0 };
        float rmsDb[2] { -100.f, -100.f };
        float peakDb[2] { -100.f, -100.f };

        static constexpr float minDb = -60.0f, maxDb = 6.0f;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
    };
}
