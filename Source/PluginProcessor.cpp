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
    raw.air = g (air); raw.tight = g (tight);
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
    // Persisted in state; if absent, mint a deterministic-but-unique fingerprint.
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

    // Sidechain may be disabled, mono, or stereo.
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

    using OS = juce::dsp::Oversampling<float>;
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[i] = std::make_unique<OS> (2, (size_t) (i + 1),
                                                OS::filterHalfBandFIREquiripple, true, true);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }
    currentOsChoice = -1; // force reconfigure on first block

    eq.prepare     (baseSampleRate, 2);
    comp.prepare   (baseSampleRate, 2);
    stereo.prepare (baseSampleRate, 2);
    inMeter.prepare  (baseSampleRate);
    outMeter.prepare (baseSampleRate);
    tube.setDrift (&driftModel);
    tape.setDrift (&driftModel);
    comp.setDrift (&driftModel);

    juce::dsp::ProcessSpec spec { baseSampleRate, (juce::uint32) samplesPerBlock, 2 };
    dryDelay.prepare (spec);
    dryDelay.reset();

    dryBuffer.setSize (2, samplesPerBlock);
    alignedDrySetup (samplesPerBlock);

    agCoeff = std::exp (-1.0f / (float) (0.3 * baseSampleRate));
    agInSq = agOutSq = 1.0e-6f; agGainSmoothed = 1.0f;
    cbHold = { 0.f, 0.f }; cbCounter = { 0, 0 };
}

//==============================================================================
void ControlPanelAudioProcessor::alignedDrySetup (int block)
{
    alignedDry.setSize (2, block);
}

void ControlPanelAudioProcessor::selectOversampling (int choice, int mainCh)
{
    if (choice == currentOsChoice) return;
    currentOsChoice = choice;

    double effSr = baseSampleRate;
    int    effBlk = maxBlockSize;
    if (choice > 0)
    {
        const int factor = 1 << choice; // 2,4,8
        effSr  = baseSampleRate * factor;
        effBlk = maxBlockSize * factor;
        reportedLatency = (int) std::round (oversamplers[choice - 1]->getLatencyInSamples());
    }
    else
    {
        reportedLatency = 0;
    }

    // (Re)prepare the nonlinear stages at the effective (oversampled) rate so
    // their internal filters land on the right cutoffs.
    tube.prepare (effSr, 2);
    tape.prepare (effSr, 2);
    juce::ignoreUnused (effBlk, mainCh);

    dryDelay.setDelay ((float) reportedLatency);
    setLatencySamples (reportedLatency);
}

//==============================================================================
void ControlPanelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIO = getBusBuffer (buffer, true, 0);
    const int mainCh    = juce::jmin (2, mainIO.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (mainCh == 0 || numSamples == 0) return;

    // ---- oversampling selection / latency ----------------------------------
    const int osChoice = (int) raw.oversample->load();
    selectOversampling (osChoice, mainCh);

    // ---- gather sidechain (optional) ---------------------------------------
    const float* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    if (auto* scBusPtr = getBus (true, 1); scBusPtr != nullptr && scBusPtr->isEnabled())
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (2, scBuf.getNumChannels());
        for (int c = 0; c < scCh; ++c) scPtrs[c] = scBuf.getReadPointer (c);
    }

    float* main[2] = { nullptr, nullptr };
    for (int c = 0; c < mainCh; ++c) main[c] = mainIO.getWritePointer (c);

    // ---- capture true-input dry & feed input meter -------------------------
    for (int c = 0; c < mainCh; ++c) dryBuffer.copyFrom (c, 0, main[c], numSamples);
    {
        const float* dp[2] = { dryBuffer.getReadPointer (0), mainCh > 1 ? dryBuffer.getReadPointer (1) : dryBuffer.getReadPointer (0) };
        inMeter.process (dp, mainCh, numSamples);
    }

    // ---- latency-align the dry path (for mix / delta / bypass) -------------
    for (int c = 0; c < mainCh; ++c)
    {
        const float* d = dryBuffer.getReadPointer (c);
        float* ad = alignedDry.getWritePointer (c);
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
        const float* op[2] = { main[0], mainCh > 1 ? main[1] : main[0] };
        outMeter.process (op, mainCh, numSamples);
        analyzer.push (op, mainCh, numSamples);
        return;
    }

    // ------------------------------------------------------------------ CHAIN
    driftModel.setAmount (raw.drift->load());

    // 1) Input trim ----------------------------------------------------------
    const float inGain = dsp::dbToGain (raw.inputTrim->load());
    for (int c = 0; c < mainCh; ++c)
        juce::FloatVectorOperations::multiply (main[c], inGain, numSamples);

    // 2) EQ ------------------------------------------------------------------
    if (raw.eqOn->load() > 0.5f)
    {
        dsp::EqualizerModule::Settings es;
        es.eqOn = true;
        es.hpOn = raw.hpOn->load() > 0.5f; es.hpFreq = raw.hpFreq->load();
        es.lpOn = raw.lpOn->load() > 0.5f; es.lpFreq = raw.lpFreq->load();
        es.propQ = raw.propQ->load() > 0.5f;
        es.air = raw.air->load(); es.tight = raw.tight->load();
        for (int b = 0; b < 5; ++b)
            es.band[b] = { raw.bOn[b]->load() > 0.5f, raw.bFreq[b]->load(), raw.bGain[b]->load(), raw.bQ[b]->load() };
        eq.setSettings (es);
        eq.updateCoefficients();
        eq.process (main, mainCh, numSamples);
    }
    else
    {
        // keep the response query in sync even when bypassed
        dsp::EqualizerModule::Settings es; es.eqOn = false;
        eq.setSettings (es); eq.updateCoefficients();
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
        cs.optBlend = raw.compOptBlend->load() * 0.01f;
        cs.mix = raw.compMix->load() * 0.01f;
        cs.scHpFreq = raw.scHpf->load();
        cs.transformer = (int) raw.transformer->load();
        comp.setSettings (cs);
        comp.process (main, mainCh, numSamples, scCh > 0 ? scPtrs : nullptr, scCh);
        grDbAtomic.store (comp.getGainReductionDb(), std::memory_order_relaxed);
    }

    // 4) Headroom -> Oversampled nonlinear ( Tube -> Tape ) -> -Headroom -----
    {
        const float hr = raw.headroom->load();
        const float preG  = dsp::dbToGain (hr);
        const float postG = dsp::dbToGain (-hr);
        const bool tubeEnabled = raw.tubeOn->load() > 0.5f;
        const bool tapeEnabled = raw.tapeOn->load() > 0.5f;

        if (tubeEnabled)
            tube.setParams ((int) raw.tubeModel->load(), raw.tubeDrive->load(),
                            raw.tubeBias->load(), raw.tubeTone->load());
        if (tapeEnabled)
            tape.setParams (raw.tapeDrive->load(), (int) raw.tapeSpeed->load(),
                            raw.tapeLF->load(), raw.tapeHF->load());

        if (preG != 1.0f)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], preG, numSamples);

        if (osChoice > 0)
        {
            juce::dsp::AudioBlock<float> mainBlock (main, (size_t) mainCh, (size_t) numSamples);
            auto& os = *oversamplers[osChoice - 1];
            auto up = os.processSamplesUp (juce::dsp::AudioBlock<const float> (mainBlock));

            float* upPtr[2]; const int upN = (int) up.getNumSamples();
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

        if (postG != 1.0f)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], postG, numSamples);
    }

    // 5) Stereo (Mono Maker + Width) ----------------------------------------
    stereo.setParams (raw.width->load(), raw.monoFreq->load());
    stereo.process (main, mainCh, numSamples);

    // 6) Circuit Bend --------------------------------------------------------
    {
        const float cb = raw.circuitBend->load() * 0.01f;
        if (cb > 0.001f)
        {
            const float bits = juce::jmap (cb, 16.0f, 4.0f);
            const float steps = std::pow (2.0f, bits - 1.0f);
            const int   hold  = 1 + (int) std::round (juce::jmap (cb, 0.0f, 8.0f));
            const float drive = 1.0f + cb * 3.0f;
            for (int c = 0; c < mainCh; ++c)
            {
                float* x = main[c];
                for (int n = 0; n < numSamples; ++n)
                {
                    if (cbCounter[c] <= 0) { cbHold[c] = x[n]; cbCounter[c] = hold; }
                    --cbCounter[c];
                    float q = std::round (cbHold[c] * steps) / steps;
                    q = dsp::fastTanh (q * drive);
                    x[n] = x[n] * (1.0f - cb) + q * cb;
                }
            }
        }
    }

    // 7) Output trim ---------------------------------------------------------
    const float outGain = dsp::dbToGain (raw.outputTrim->load());
    for (int c = 0; c < mainCh; ++c)
        juce::FloatVectorOperations::multiply (main[c], outGain, numSamples);

    // 8) Auto-gain match -----------------------------------------------------
    const bool delta    = raw.delta->load() > 0.5f;
    const bool autoGain = raw.autoGain->load() > 0.5f;
    {
        double inSum = 0.0, outSum = 0.0;
        for (int c = 0; c < mainCh; ++c)
        {
            const float* dp = alignedDry.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n) { inSum += dp[n] * dp[n]; outSum += main[c][n] * main[c][n]; }
        }
        const float invN = 1.0f / (float) (numSamples * mainCh);
        agInSq  = agCoeff * agInSq  + (1.0f - agCoeff) * (float) (inSum  * invN) + dsp::antiDenormal;
        agOutSq = agCoeff * agOutSq + (1.0f - agCoeff) * (float) (outSum * invN) + dsp::antiDenormal;

        float target = 1.0f;
        if (autoGain)
            target = juce::jlimit (0.25f, 4.0f, std::sqrt (agInSq / juce::jmax (1.0e-9f, agOutSq)));
        agGainSmoothed += 0.05f * (target - agGainSmoothed);

        if (autoGain && ! delta)
            for (int c = 0; c < mainCh; ++c) juce::FloatVectorOperations::multiply (main[c], agGainSmoothed, numSamples);

        autoGainDbAtomic.store (dsp::gainToDb (autoGain ? agGainSmoothed : 1.0f), std::memory_order_relaxed);
    }

    // 9) Delta / Dry-Wet -----------------------------------------------------
    if (delta)
    {
        // Hear exactly what the plugin added or removed (processed - dry).
        for (int c = 0; c < mainCh; ++c)
        {
            const float* dp = alignedDry.getReadPointer (c);
            float* x = main[c];
            for (int n = 0; n < numSamples; ++n) x[n] = (x[n] - dp[n]) * 2.0f;
        }
    }
    else
    {
        float dg, wg; dsp::equalPower (raw.mix->load() * 0.01f, dg, wg);
        for (int c = 0; c < mainCh; ++c)
        {
            const float* dp = alignedDry.getReadPointer (c);
            float* x = main[c];
            for (int n = 0; n < numSamples; ++n) x[n] = dg * dp[n] + wg * x[n];
        }
    }

    // Mono → duplicate handled by host; ensure any unused channel is cleared.
    for (int c = mainCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // ---- output meter + analyzer ------------------------------------------
    const float* op[2] = { main[0], mainCh > 1 ? main[1] : main[0] };
    outMeter.process (op, mainCh, numSamples);
    analyzer.push (op, mainCh, numSamples);
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

    // Persist snapshots alongside the parameter children (do NOT clear the
    // parameter nodes — copyState() already holds them).
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
            tree.removeChild (snapWrap, nullptr); // strip wrappers, keep PARAM nodes
        }
    }

    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ControlPanelAudioProcessor();
}
