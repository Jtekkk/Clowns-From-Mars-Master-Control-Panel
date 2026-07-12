#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/ControlPanelLookAndFeel.h"
#include "gui/RotarySlider.h"
#include "gui/LevelMeter.h"
#include "gui/GainReductionMeter.h"
#include "gui/SpectrumAnalyzer.h"

/**
    Master Control Panel editor.

    A fixed-layout "control panel" that scales cleanly: every control lives on a
    base-size canvas which is uniformly transformed to fit the (resizable,
    aspect-locked) window, giving a crisp Scalable UI without re-flowing.
*/
class ControlPanelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit ControlPanelAudioProcessorEditor (ControlPanelAudioProcessor&);
    ~ControlPanelAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    //==========================================================================
    // Canvas draws the background, panels and section titles behind the controls.
    class Canvas : public juce::Component
    {
    public:
        struct Panel { juce::String title; juce::Rectangle<int> area; juce::Colour accent; };
        std::vector<Panel> panels;
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override;

    cfm::gui::RotarySlider*  addKnob   (const char* id, const juce::String& name, juce::Colour accent);
    juce::ToggleButton* addToggle (const char* id, const juce::String& text, juce::Colour accent);
    juce::ComboBox*     addCombo  (const char* id, const juce::StringArray& items);

    void refreshSnapshotButtons();
    void selectProgram (int delta);

    ControlPanelAudioProcessor& processor;
    cfm::gui::ControlPanelLookAndFeel lnf;
    Canvas canvas;

    // control storage
    std::vector<std::unique_ptr<cfm::gui::RotarySlider>> knobStore;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggleStore;
    std::vector<std::unique_ptr<juce::ComboBox>> comboStore;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>   sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   buttonAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;

    // named controls we position explicitly
    cfm::gui::RotarySlider *kInput{}, *kOutput{}, *kHead{}, *kMix{}, *kDrift{}, *kBend{};
    cfm::gui::RotarySlider *kDrive{}, *kBias{}, *kTone{};
    cfm::gui::RotarySlider *kbFreq[5]{}, *kbGain[5]{}, *kbQ[5]{};
    cfm::gui::RotarySlider *kHP{}, *kLP{}, *kAir{}, *kTight{};
    cfm::gui::RotarySlider *kThr{}, *kRat{}, *kAtt{}, *kRel{}, *kKnee{}, *kMk{}, *kOpt{}, *kCMix{}, *kSc{};
    cfm::gui::RotarySlider *kTDrive{}, *kTLF{}, *kTHF{};
    cfm::gui::RotarySlider *kWidth{}, *kMono{};

    juce::ToggleButton *tBypass{}, *tDelta{}, *tAuto{}, *tTube{}, *tEq{}, *tHP{}, *tLP{}, *tPropQ{}, *tComp{}, *tAutoMk{}, *tTape{};
    juce::ComboBox *cOS{}, *cModel{}, *cMode{}, *cXfmr{}, *cSpeed{};

    // header widgets
    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::ComboBox   presetBox;
    juce::TextButton snapButtons[4];
    juce::Label      fingerprint;
    juce::Label      autoGainLabel;

    // meters / analyzer
    std::unique_ptr<cfm::gui::SpectrumAnalyzer>   analyzer;
    std::unique_ptr<cfm::gui::LevelMeter>         levelMeter;
    std::unique_ptr<cfm::gui::GainReductionMeter> grMeter;

    static constexpr int baseW = 1120, baseH = 860;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanelAudioProcessorEditor)
};
