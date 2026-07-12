#include "RotarySlider.h"

using namespace tt;

namespace tt::gui
{
    RotarySlider::RotarySlider (juce::AudioProcessorValueTreeState& state,
                                const juce::String& paramID,
                                const juce::String& displayName,
                                juce::Colour accent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        slider.setColour (juce::Slider::thumbColourId, accent);
        slider.setColour (juce::Slider::textBoxTextColourId, theme::textHi);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);

        caption.setText (displayName, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setColour (juce::Label::textColourId, theme::textMid);
        caption.setFont (theme::font (12.5f, true));
        addAndMakeVisible (caption);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    }

    void RotarySlider::resized()
    {
        auto r = getLocalBounds();
        caption.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

    void RotarySlider::paint (juce::Graphics&) {}
}
