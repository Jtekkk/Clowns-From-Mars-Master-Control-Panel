#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/DspCommon.h"
#include "dsp/DriftModel.h"
#include "dsp/TubeStage.h"
#include "dsp/EqualizerModule.h"
#include "dsp/CompressorModule.h"
#include "dsp/TapeModule.h"
#include "dsp/StereoModule.h"
#include "dsp/Metering.h"

/**
    MASTER CONTROL PANEL — intelligent analog-modelled mastering processor.

    The entire internal signal path runs in 64-bit double precision. When the
    host offers double-precision buffers we process them natively; when it hands
    us 32-bit buffers we widen to double, process, and narrow back — so the DSP
    always works at reference fidelity regardless of host precision.

    Signal flow (per block):

        Input Trim (smoothed)
          -> EQ (5-band + shelves + HP/LP + AIR/TIGHT)
          -> Compressor (2-stage, M/S aware, sidechain)
          -> [ +Headroom -> Oversample( Tube -> Tape ) -> -Headroom ]
          -> Stereo (Mono Maker + Width)
          -> Circuit Bend
          -> Output Trim (smoothed)
          -> Auto-Gain match  ->  Dry/Wet mix  (or Delta)

    Engineering: denormals flushed, no audio-thread allocation, selectable
    double-precision oversampling with reported latency, latency-compensated dry
    path, smoothed gain staging, true-peak metering.
*/
class ControlPanelAudioProcessor : public juce::AudioProcessor
{
public:
    ControlPanelAudioProcessor();
    ~ControlPanelAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool supportsDoublePrecisionProcessing() const override { return true; }
    void processBlock (juce::AudioBuffer<float>&,  juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory presets exposed as host "programs".
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor ---------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    cfm::dsp::MeterBallistics& getOutputMeter() noexcept { return outMeter; }
    cfm::dsp::MeterBallistics& getInputMeter()  noexcept { return inMeter; }
    cfm::dsp::AnalyzerFifo&    getAnalyzer()     noexcept { return analyzer; }

    float getGainReductionDb() const noexcept { return grDbAtomic.load (std::memory_order_relaxed); }
    float getAutoGainDb()      const noexcept { return autoGainDbAtomic.load (std::memory_order_relaxed); }
    juce::int64 getSerial()    const noexcept { return serial; }

    // Queries the EQ magnitude response (dB) for the UI curve.
    double getEqMagnitudeDb (double freqHz) noexcept
    {
        return cfm::dsp::gainToDb (eq.magnitudeAt (freqHz));
    }

    // A/B/C/D snapshot slots.
    void   storeSnapshot (int slot);
    void   recallSnapshot (int slot);
    int    getActiveSnapshot() const noexcept { return activeSnapshot; }
    bool   hasSnapshot (int slot) const noexcept
    {
        return juce::isPositiveAndBelow (slot, numSnapshots) && snapshots[(size_t) slot].isValid();
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        return cfm::params::createLayout();
    }

    void applyProgram (int index);
    void ensureSerial();
    void prepareBuffers (int block);
    void selectOversampling (int choice);

    // The single double-precision core. Operates in place on `main` (up to 2
    // channels); `sc` is an optional external sidechain (may be nullptr).
    void processDouble (double* const* main, int mainCh, int numSamples,
                        const double* const* sc, int scCh);

    // Cached raw parameter pointers (atomic, RT-safe reads).
    struct Raw
    {
        std::atomic<float>* inputTrim; std::atomic<float>* outputTrim; std::atomic<float>* headroom;
        std::atomic<float>* mix; std::atomic<float>* bypass; std::atomic<float>* delta;
        std::atomic<float>* autoGain; std::atomic<float>* oversample; std::atomic<float>* drift;
        std::atomic<float>* circuitBend;
        std::atomic<float>* tubeOn; std::atomic<float>* tubeDrive; std::atomic<float>* tubeBias;
        std::atomic<float>* tubeModel; std::atomic<float>* tubeTone;
        std::atomic<float>* eqOn; std::atomic<float>* hpOn; std::atomic<float>* hpFreq;
        std::atomic<float>* lpOn; std::atomic<float>* lpFreq; std::atomic<float>* propQ;
        std::atomic<float>* air; std::atomic<float>* tight;
        std::atomic<float>* bFreq[5]; std::atomic<float>* bGain[5]; std::atomic<float>* bQ[5]; std::atomic<float>* bOn[5];
        std::atomic<float>* compOn; std::atomic<float>* compThresh; std::atomic<float>* compRatio;
        std::atomic<float>* compAttack; std::atomic<float>* compRelease; std::atomic<float>* compKnee;
        std::atomic<float>* compMakeup; std::atomic<float>* compAutoMk; std::atomic<float>* compMode;
        std::atomic<float>* compOptBlend; std::atomic<float>* compMix; std::atomic<float>* scHpf;
        std::atomic<float>* transformer;
        std::atomic<float>* tapeOn; std::atomic<float>* tapeDrive; std::atomic<float>* tapeSpeed;
        std::atomic<float>* tapeLF; std::atomic<float>* tapeHF;
        std::atomic<float>* width; std::atomic<float>* monoFreq;
    } raw;

    void cacheParams();

    // Modules (all 64-bit).
    cfm::dsp::DriftModel       driftModel;
    cfm::dsp::EqualizerModule  eq;
    cfm::dsp::CompressorModule comp;
    cfm::dsp::TubeStage        tube;
    cfm::dsp::TapeModule       tape;
    cfm::dsp::StereoModule     stereo;
    cfm::dsp::MeterBallistics  inMeter, outMeter;
    cfm::dsp::AnalyzerFifo     analyzer;

    // Double-precision oversampling: index 0->2x, 1->4x, 2->8x. "Off" separate.
    std::array<std::unique_ptr<juce::dsp::Oversampling<double>>, 3> oversamplers;
    int    currentOsChoice = -1;
    double baseSampleRate  = 44100.0;
    int    maxBlockSize    = 512;

    // Latency-compensated dry path.
    juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };
    juce::AudioBuffer<double> dryBuffer;      // pre-processing capture
    juce::AudioBuffer<double> alignedDry;     // latency-aligned dry
    juce::AudioBuffer<double> workBuffer;     // widened main (float path)
    juce::AudioBuffer<double> scWorkBuffer;   // widened sidechain (float path)
    int reportedLatency = 0;

    // Smoothed gain staging (per-sample, anti-zipper).
    juce::LinearSmoothedValue<double> inGainSm, outGainSm, mixSm;

    // Auto-gain matching.
    double agInSq = 0.0, agOutSq = 0.0, agCoeff = 0.0, agGainSmoothed = 1.0;

    // Bit-crush state for Circuit Bend.
    std::array<double, 2> cbHold { {} };
    std::array<int, 2>    cbCounter { {} };

    std::atomic<float> grDbAtomic { 0.0f };
    std::atomic<float> autoGainDbAtomic { 0.0f };

    juce::int64 serial = 0;
    int currentProgram = 0;

    static constexpr int numSnapshots = 4;
    std::array<juce::ValueTree, numSnapshots> snapshots;
    int activeSnapshot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanelAudioProcessor)
};
