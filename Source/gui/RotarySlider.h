#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace cfm::gui
{
    /**
        A labelled rotary control bound to an APVTS parameter, with the caption
        above and the live value read-out in the knob. Accent colour drives the
        value arc so related controls can be grouped by hue.
    */
    class RotarySlider : public juce::Component
    {
    public:
        RotarySlider (juce::AudioProcessorValueTreeState& state,
                      const juce::String& paramID,
                      const juce::String& displayName,
                      juce::Colour accent);

        void resized() override;
        void paint (juce::Graphics&) override;

        juce::Slider slider;

    private:
        juce::Label caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotarySlider)
    };
}
