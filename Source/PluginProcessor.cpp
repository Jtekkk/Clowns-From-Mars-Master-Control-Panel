#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace cfm;

//==============================================================================
ControlPanelAudioProcessor::ControlPanelAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
    cacheParams();
    ensureSerial();
    driftModel.setSerial (serial);
}

void ControlPanelAudioProcessor::cacheParams()
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    using namespace cfm::params::id;

    raw.inputTrim = g (inputTrim); raw.outputTrim = g (outputTrim); raw.headroom = g (headroom);
    raw.mix = g (mix); raw.bypass = g (bypass); raw.delta = g (delta);
    raw.autoGain = g (autoGain); raw.oversample = g (oversample); raw.drift = g (drift);
    raw.circuitBend = g (circuitBend);

    raw.tubeOn = g (tubeOn); raw.tubeDrive = g (tubeDrive); raw.tubeBias = g (tubeBias);
    raw.tubeModel = g (tubeModel); raw.tubeTone = g (tubeTone);

    raw.eqOn = g (eqOn); raw.hpOn = g (hpOn); raw.hpFreq = g (hpFreq);
    raw.lpOn = g (lpOn); raw.lpFreq = g (lpFreq); raw.propQ = g (propQ);
    raw.eqLinear = g (eqLinear); raw.air = g (air); raw.tight = g (tight);
    for (int b = 0; b < 5; ++b)
    {
        raw.bFreq[b] = g (bandFreq[b]); raw.bGain[b] = g (bandGain[b]);
        raw.bQ[b]    = g (bandQ[b]);    raw.bOn[b]   = g (bandOn[b]);
    }

    raw.compOn = g (compOn); raw.compThresh = g (compThresh); raw.compRatio = g (compRatio);
    raw.compAttack = g (compAttack); raw.compRelease = g (compRelease); raw.compKnee = g (compKnee);
    raw.compMakeup = g (compMakeup); raw.compAutoMk = g (compAutoMk); raw.compMode = g (compMode);
    raw.compOptBlend = g (compOptBlend); raw.compMix = g (compMix); raw.scHpf = g (scHpf);
    raw.transformer = g (transformer);

    raw.tapeOn = g (tapeOn); raw.tapeDrive = g (tapeDrive); raw.tapeSpeed = g (tapeSpeed);
    raw.tapeLF = g (tapeLF); raw.tapeHF = g (tapeHF);

    raw.width = g (width); raw.monoFreq = g (monoFreq);
}

void ControlPanelAudioProcessor::ensureSerial()
{
    if (serial == 0)
    {
        auto& tree = apvts.state;
        if (tree.hasProperty ("serial"))
            serial = (juce::int64) tree.getProperty ("serial");
        else
        {
            juce::Random r (juce::Time::getHighResolutionTicks());
            serial = (r.nextInt64() & 0x7fffffffffffLL) | 0x100000LL;
            tree.setProperty ("serial", serial, nullptr);
        }
    }
}

//==============================================================================
bool ControlPanelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

//==============================================================================
void ControlPanelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;
    maxBlockSize   = samplesPerBlock;

    using OS = juce::dsp::Oversampling<double>;
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[i] = std::make_unique<OS> (2, (size_t) (i + 1),
                                                OS::filterHalfBandFIREquiripple, true, true);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }
    currentOsChoice = -1; // force reconfigure on first block

    eq.prepare     (baseSampleRate, 2);
    linEq.prepare  (baseSampleRate, 2);
    comp.prepare   (baseSampleRate, 2);
    stereo.prepare (baseSampleRate, 2);
    loudness.prepare (baseSampleRate);
    inMeter.prepare  (baseSampleRate);
    outMeter.prepare (baseSampleRate);
    eqLinearActive = raw.eqLinear->load() > 0.5f;
    linKernelValid = false;
    tube.setDrift (&driftModel);
    tape.setDrift (&driftModel);
    comp.setDrift (&driftModel);

    juce::dsp::ProcessSpec spec { baseSampleRate, (juce::uint32) samplesPerBlock, 2 };
    dryDelay.prepare (spec);
    dryDelay.reset();

    prepareBuffers (samplesPerBlock);

    const double smoothT = 0.02; // 20 ms anti-zipper smoothing
    inGainSm.reset  (baseSampleRate, smoothT);
    outGainSm.reset (baseSampleRate, smoothT);
    mixSm.reset     (baseSampleRate, smoothT);
    inGainSm.setCurrentAndTargetValue  (dsp::dbToGain ((double) raw.inputTrim->load()));
    outGainSm.setCurrentAndTargetValue (dsp::dbToGain ((double) raw.outputTrim->load()));
    mixSm.setCurrentAndTargetValue     ((double) raw.mix->load() * 0.01);

    agCoeff = std::exp (-1.0 / (0.3 * baseSampleRate));
    agInSq = agOutSq = 1.0e-6; agGainSmoothed = 1.0;
    cbHold = { 0.0, 0.0 }; cbCounter = { 0, 0 };
}

void ControlPanelAudioProcessor::prepareBuffers (int block)
{
    dryBuffer.setSize    (2, block);
    alignedDry.setSize   (2, block);
    workBuffer.setSize   (2, block);
    scWorkBuffer.setSize (2, block);
}

void ControlPanelAudioProcessor::selectOversampling (int choice)
{
    if (choice == currentOsChoice) return;
    currentOsChoice = choice;

    double effSr = baseSampleRate;
    if (choice > 0)
    {
        const int factor = 1 << choice; // 2,4,8
        effSr = baseSampleRate * factor;
        osLatency = (int) std::round (oversamplers[choice - 1]->getLatencyInSamples());
    }
    else
    {
        osLatency = 0;
    }

    // (Re)prepare the nonlinear stages at the effective (oversampled) rate.
    tube.prepare (effSr, 2);
    tape.prepare (effSr, 2);

    updateLatency();
}

void ControlPanelAudioProcessor::updateLatency()
{
    reportedLatency = osLatency + (eqLinearActive ? cfm::dsp::LinearPhaseEq::latency : 0);
    dryDelay.setDelay ((double) reportedLatency);
    setLatencySamples (reportedLatency);
}

//==============================================================================
// The single 64-bit processing core. Operates in place on `main`.
void ControlPanelAudioProcessor::processDouble (double* const* main, int mainCh, int numSamples,
                                                const double* const* sc, int scCh)
{
    const int osChoice = (int) raw.oversample->load();
    selectOversampling (osChoice);

    // ---- capture true-input dry & feed input meter -------------------------
    for (int c = 0; c < mainCh; ++c)
        juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), main[c], numSamples);
    {
        const double* dp[2] = { dryBuffer.getReadPointer (0), mainCh > 1 ? dryBuffer.getReadPointer (1) : dryBuffer.getReadPointer (0) };
        inMeter.process (dp, mainCh, numSamples);
    }

    // ---- latency-align the dry path ----------------------------------------
    for (int c = 0; c < mainCh; ++c)
    {
        const double* d = dryBuffer.getReadPointer (c);
        double* ad = alignedDry.getWritePointer (c);
        for (int n = 0; n < numSamples; ++n)
        {
            dryDelay.pushSample (c, d[n]);
            ad[n] = dryDelay.popSample (c);
        }
    }

    const bool bypassed = raw.bypass->load() > 0.5f;
    if (bypassed)
    {
        for (int c = 0; c < mainCh; ++c)
            juce::FloatVectorOperations::copy (main[c], alignedDry.getReadPointer (c), numSamples);
        const double* op[2] = { main[0], mainCh > 1 ? main[1] : main[0] };
        outMeter.process (op, mainCh, numSamples);
        analyzer.push (op, mainCh, numSamples);
        loudness.process (op, mainCh, numSamples);
        return;
    }

    driftModel.setAmount ((double) raw.drift->load());

    // 1) Input trim (per-sample smoothed) ------------------------------------
    inGainSm.setTargetValue (dsp::dbToGain ((double) raw.inputTrim->load()));
    for (int n = 0; n < numSamples; ++n)
    {
        const double gn = inGainSm.getNextValue();
        for (int c = 0; c < mainCh; ++c) main[c][n] *= gn;
    }

    // Linear-phase toggle changes latency; handle it before processing.
    {
        const bool lin = raw.eqLinear->load() > 0.5f;
        if (lin != eqLinearActive)
        {
            eqLinearActive = lin;
            linEq.reset();
            linKernelValid = false;
            updateLatency();
        }
    }

    // 2) EQ (minimum-phase biquads, or linear-phase FIR) ---------------------
    {
        const bool eqEnabled = raw.eqOn->load() > 0.5f;
        dsp::EqualizerModule::Settings es;
        es.eqOn = eqEnabled;
        es.hpOn = raw.hpOn->load() > 0.5f; es.hpFreq = raw.hpFreq->load();
        es.lpOn = raw.lpOn->load() > 0.5f; es.lpFreq = raw.lpFreq->load();
        es.propQ = raw.propQ->load() > 0.5f;
        es.air = raw.air->load(); es.tight = raw.tight->load();
        for (int b = 0; b < 5; ++b)
            es.band[b] = { raw.bOn[b]->load() > 0.5f, raw.bFreq[b]->load(), raw.bGain[b]->load(), raw.bQ[b]->load() };
        eq.setSettings (es);
        const bool coeffsChanged = eq.updateCoefficients();

        if (eqEnabled)
        {
            if (eqLinearActive)
            {
                if (coeffsChanged || ! linKernelValid)
                {
                    linEq.rebuild ([this] (double f) { return eq.magnitudeAt (f); });
                    linKernelValid = true;
                }
                linEq.process (main, mainCh, numSamples);
            }
            else
            {
                eq.process (main, mainCh, numSamples);
            }
        }
    }

    // 3) Compressor ----------------------------------------------------------
    {
        dsp::CompressorModule::Settings cs;
        cs.on = raw.compOn->load() > 0.5f;
        cs.threshold = raw.compThresh->load(); cs.ratio = raw.compRatio->load();
        cs.attackMs = raw.compAttack->load(); cs.releaseMs = raw.compRelease->load();
        cs.knee = raw.compKnee->load();
        cs.makeup = raw.compMakeup->load(); cs.autoMakeup = raw.compAutoMk->load() > 0.5f;
        cs.mode = (int) raw.compMode->load();
        cs.optBlend = raw.compOptBlend->load() * 0.01;
        cs.mix = raw.compMix->load() * 0.01;
        cs.scHpFreq = raw.scHpf->load();
        cs.transformer = (int) raw.transformer->load();
        comp.setSettings (cs);
        comp.process (main, mainCh, numSamples, scCh > 0 ? sc : nullptr, scCh);
        grDbAtomic.store ((float) comp.getGainReductionDb(), std::memory_order_relaxed);
    }

    // 4) Headroom -> Oversampled nonlinear ( Tube -> Tape ) -> -Headroom -----
    {
        const double hr = raw.headroom->load();
        const double preG  = dsp::dbToGain (hr);
        const double postG = dsp::dbToGain (-hr);
        const bool tubeEnabled = raw.tubeOn->load() > 0.5f;
        const bool tapeEnabled = raw.tapeOn->load() > 0.5f;

        if (tubeEnabled)
            tube.setParams ((int) raw.tubeModel->load(), raw.tubeDrive->load(),
                            raw.tubeBias->load(), raw.tubeTone->load());
        if (tapeEnabled)
            tape.setParams (raw.tapeDrive->load(), (int) raw.tapeSpeed->load(),
                            raw.tapeLF->load(), raw.tapeHF->load());

        if (preG != 1.0)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], preG, numSamples);

        if (osChoice > 0)
        {
            juce::dsp::AudioBlock<double> mainBlock (main, (size_t) mainCh, (size_t) numSamples);
            auto& os = *oversamplers[osChoice - 1];
            auto up = os.processSamplesUp (juce::dsp::AudioBlock<const double> (mainBlock));

            double* upPtr[2]; const int upN = (int) up.getNumSamples();
            for (int c = 0; c < mainCh; ++c) upPtr[c] = up.getChannelPointer ((size_t) c);
            if (tubeEnabled) tube.process (upPtr, mainCh, upN);
            if (tapeEnabled) tape.process (upPtr, mainCh, upN);

            os.processSamplesDown (mainBlock);
        }
        else
        {
            if (tubeEnabled) tube.process (main, mainCh, numSamples);
            if (tapeEnabled) tape.process (main, mainCh, numSamples);
        }

        if (postG != 1.0)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], postG, numSamples);
    }

    // 5) Stereo (Mono Maker + Width) ----------------------------------------
    stereo.setParams (raw.width->load(), raw.monoFreq->load());
    stereo.process (main, mainCh, numSamples);

    // 6) Circuit Bend --------------------------------------------------------
    {
        const double cb = raw.circuitBend->load() * 0.01;
        if (cb > 0.001)
        {
            const double bits  = juce::jmap (cb, 16.0, 4.0);
            const double steps = std::pow (2.0, bits - 1.0);
            const int    hold  = 1 + (int) std::round (juce::jmap (cb, 0.0, 8.0));
            const double drive = 1.0 + cb * 3.0;
            for (int c = 0; c < mainCh; ++c)
            {
                double* x = main[c];
                for (int n = 0; n < numSamples; ++n)
                {
                    if (cbCounter[c] <= 0) { cbHold[c] = x[n]; cbCounter[c] = hold; }
                    --cbCounter[c];
                    double q = std::round (cbHold[c] * steps) / steps;
                    q = dsp::saturate (q * drive);
                    x[n] = x[n] * (1.0 - cb) + q * cb;
                }
            }
        }
    }

    // 7) Output trim (per-sample smoothed) -----------------------------------
    outGainSm.setTargetValue (dsp::dbToGain ((double) raw.outputTrim->load()));
    for (int n = 0; n < numSamples; ++n)
    {
        const double gn = outGainSm.getNextValue();
        for (int c = 0; c < mainCh; ++c) main[c][n] *= gn;
    }

    // 8) Auto-gain match -----------------------------------------------------
    const bool delta    = raw.delta->load() > 0.5f;
    const bool autoGain = raw.autoGain->load() > 0.5f;
    {
        double inSum = 0.0, outSum = 0.0;
        for (int c = 0; c < mainCh; ++c)
        {
            const double* dp = alignedDry.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n) { inSum += dp[n] * dp[n]; outSum += main[c][n] * main[c][n]; }
        }
        const double invN = 1.0 / (double) (numSamples * mainCh);
        agInSq  = agCoeff * agInSq  + (1.0 - agCoeff) * (inSum  * invN) + dsp::antiDenormal;
        agOutSq = agCoeff * agOutSq + (1.0 - agCoeff) * (outSum * invN) + dsp::antiDenormal;

        double target = 1.0;
        if (autoGain)
            target = juce::jlimit (0.25, 4.0, std::sqrt (agInSq / juce::jmax (1.0e-12, agOutSq)));
        agGainSmoothed += 0.05 * (target - agGainSmoothed);

        if (autoGain && ! delta)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], agGainSmoothed, numSamples);

        autoGainDbAtomic.store ((float) dsp::gainToDb (autoGain ? agGainSmoothed : 1.0), std::memory_order_relaxed);
    }

    // 9) Delta / Dry-Wet -----------------------------------------------------
    if (delta)
    {
        for (int c = 0; c < mainCh; ++c)
        {
            const double* dp = alignedDry.getReadPointer (c);
            double* x = main[c];
            for (int n = 0; n < numSamples; ++n) x[n] = (x[n] - dp[n]) * 2.0;
        }
    }
    else
    {
        mixSm.setTargetValue ((double) raw.mix->load() * 0.01);
        for (int n = 0; n < numSamples; ++n)
        {
            const double m = mixSm.getNextValue();
            double dg, wg; dsp::equalPower (m, dg, wg);
            for (int c = 0; c < mainCh; ++c)
                main[c][n] = dg * alignedDry.getReadPointer (c)[n] + wg * main[c][n];
        }
    }

    // ---- output meter + analyzer + loudness -------------------------------
    const double* op[2] = { main[0], mainCh > 1 ? main[1] : main[0] };
    outMeter.process (op, mainCh, numSamples);
    analyzer.push (op, mainCh, numSamples);
    loudness.process (op, mainCh, numSamples);
}

//==============================================================================
void ControlPanelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIO = getBusBuffer (buffer, true, 0);
    const int mainCh     = juce::jmin (2, mainIO.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (mainCh == 0 || numSamples == 0) return;

    // Widen main input to the double work buffer.
    double* wMain[2] = { nullptr, nullptr };
    for (int c = 0; c < mainCh; ++c)
    {
        wMain[c] = workBuffer.getWritePointer (c);
        const float* src = mainIO.getReadPointer (c);
        for (int n = 0; n < numSamples; ++n) wMain[c][n] = (double) src[n];
    }

    // Widen sidechain (optional).
    const double* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    if (auto* scBusPtr = getBus (true, 1); scBusPtr != nullptr && scBusPtr->isEnabled())
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (2, scBuf.getNumChannels());
        for (int c = 0; c < scCh; ++c)
        {
            double* d = scWorkBuffer.getWritePointer (c);
            const float* s = scBuf.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n) d[n] = (double) s[n];
            scPtrs[c] = d;
        }
    }

    processDouble (wMain, mainCh, numSamples, scCh > 0 ? scPtrs : nullptr, scCh);

    // Narrow back to 32-bit.
    for (int c = 0; c < mainCh; ++c)
    {
        float* dst = mainIO.getWritePointer (c);
        for (int n = 0; n < numSamples; ++n) dst[n] = (float) wMain[c][n];
    }
    for (int c = mainCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

void ControlPanelAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIO = getBusBuffer (buffer, true, 0);
    const int mainCh     = juce::jmin (2, mainIO.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (mainCh == 0 || numSamples == 0) return;

    double* main[2] = { nullptr, nullptr };
    for (int c = 0; c < mainCh; ++c) main[c] = mainIO.getWritePointer (c);

    const double* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    if (auto* scBusPtr = getBus (true, 1); scBusPtr != nullptr && scBusPtr->isEnabled())
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (2, scBuf.getNumChannels());
        for (int c = 0; c < scCh; ++c) scPtrs[c] = scBuf.getReadPointer (c);
    }

    processDouble (main, mainCh, numSamples, scCh > 0 ? scPtrs : nullptr, scCh);

    for (int c = mainCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* ControlPanelAudioProcessor::createEditor()
{
    return new ControlPanelAudioProcessorEditor (*this);
}

//==============================================================================
int ControlPanelAudioProcessor::getNumPrograms()          { return (int) cfm::presets::factory().size(); }
const juce::String ControlPanelAudioProcessor::getProgramName (int index)
{
    auto& f = cfm::presets::factory();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? f[(size_t) index].name : juce::String();
}

void ControlPanelAudioProcessor::setCurrentProgram (int index)
{
    auto& f = cfm::presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    currentProgram = index;
    applyProgram (index);
}

void ControlPanelAudioProcessor::applyProgram (int index)
{
    auto& f = cfm::presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;

    // Presets are self-contained: reset every parameter to its default first,
    // so a preset sounds identical no matter what was loaded before it. Only
    // then apply the values this preset explicitly names. Without this reset,
    // controls omitted by a preset would inherit whatever the previous preset
    // left behind, making recalls order-dependent and non-reproducible.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& kv : f[(size_t) index].values)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (kv.first)))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

//==============================================================================
void ControlPanelAudioProcessor::storeSnapshot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSnapshots)) return;
    snapshots[(size_t) slot] = apvts.copyState();
    activeSnapshot = slot;
}

void ControlPanelAudioProcessor::recallSnapshot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSnapshots)) return;
    if (snapshots[(size_t) slot].isValid())
    {
        apvts.replaceState (snapshots[(size_t) slot].createCopy());
        activeSnapshot = slot;
    }
}

//==============================================================================
void ControlPanelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("serial", serial, nullptr);
    state.setProperty ("activeSnapshot", activeSnapshot, nullptr);

    for (int i = 0; i < numSnapshots; ++i)
        if (snapshots[(size_t) i].isValid())
        {
            juce::ValueTree wrap ("SNAP" + juce::String (i));
            wrap.appendChild (snapshots[(size_t) i].createCopy(), nullptr);
            state.appendChild (wrap, nullptr);
        }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ControlPanelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;

    if (tree.hasProperty ("serial"))
    {
        serial = (juce::int64) tree.getProperty ("serial");
        driftModel.setSerial (serial);
    }
    if (tree.hasProperty ("activeSnapshot"))
        activeSnapshot = (int) tree.getProperty ("activeSnapshot");

    for (int i = 0; i < numSnapshots; ++i)
    {
        auto snapWrap = tree.getChildWithName ("SNAP" + juce::String (i));
        if (snapWrap.isValid())
        {
            if (snapWrap.getNumChildren() > 0)
                snapshots[(size_t) i] = snapWrap.getChild (0).createCopy();
            tree.removeChild (snapWrap, nullptr);
        }
    }

    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ControlPanelAudioProcessor();
}
