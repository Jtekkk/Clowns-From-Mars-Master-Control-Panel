#include "PluginEditor.h"
#include "gui/Theme.h"
#include "Parameters.h"
#include "BinaryData.h"

using namespace cfm;

//==============================================================================
void ControlPanelAudioProcessorEditor::ArtPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::bg0);
    g.fillRoundedRectangle (r, 6.0f);

    if (image.isValid())
    {
        auto dst = r.reduced (2.0f);
        juce::Path clip; clip.addRoundedRectangle (dst, 5.0f);
        g.saveState();
        g.reduceClipRegion (clip);

        // Cover-fit: fill the panel, cropping the overflow (centred).
        const float dr = dst.getWidth() / dst.getHeight();
        const float ir = image.getWidth() / (float) image.getHeight();
        juce::Rectangle<int> src;
        if (ir > dr) { const int w = juce::roundToInt (image.getHeight() * dr);
                       src = { (image.getWidth() - w) / 2, 0, w, image.getHeight() }; }
        else         { const int h = juce::roundToInt (image.getWidth() / dr);
                       src = { 0, (image.getHeight() - h) / 2, image.getWidth(), h }; }
        g.drawImage (image, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(),
                     src.getX(), src.getY(), src.getWidth(), src.getHeight());

        // Warm scrim top & bottom so overlaid chrome stays legible and it reads
        // as part of the panel rather than a pasted photo.
        juce::ColourGradient scrim (theme::bg0.withAlpha (0.55f), 0.0f, dst.getY(),
                                    theme::bg0.withAlpha (0.0f), 0.0f, dst.getY() + 90.0f, false);
        g.setGradientFill (scrim); g.fillRect (dst.withHeight (90.0f));
        juce::ColourGradient scrim2 (theme::bg0.withAlpha (0.0f), 0.0f, dst.getBottom() - 60.0f,
                                     theme::bg0.withAlpha (0.7f), 0.0f, dst.getBottom(), false);
        g.setGradientFill (scrim2); g.fillRect (dst.withY (dst.getBottom() - 60.0f).withHeight (60.0f));
        g.restoreState();
    }

    g.setColour (theme::brass.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 1.5f);
    g.setColour (theme::textMid);
    g.setFont (theme::font (10.5f, true));
    g.drawText ("CLOWNS FROM MARS", getLocalBounds().removeFromBottom (20),
                juce::Justification::centred);
}

//==============================================================================
void ControlPanelAudioProcessorEditor::Canvas::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    // Martian dusk backdrop.
    juce::ColourGradient bg (theme::bg1, full.getCentreX(), full.getY(),
                             theme::bg0, full.getCentreX(), full.getBottom(), false);
    bg.addColour (0.5, theme::bg1.darker (0.1f));
    g.setGradientFill (bg);
    g.fillRect (full);

    // Subtle vignette.
    juce::ColourGradient vig (juce::Colours::transparentBlack, full.getCentreX(), full.getCentreY(),
                              juce::Colours::black.withAlpha (0.35f), full.getX(), full.getBottom(), true);
    g.setGradientFill (vig);
    g.fillRect (full);

    // Header wordmark.
    g.setColour (theme::rustBright);
    g.setFont (theme::font (29.0f, true));
    g.drawText ("MASTER CONTROL PANEL", juce::Rectangle<int> (20, 10, 480, 36), juce::Justification::centredLeft);
    g.setColour (theme::tubeGlow.withAlpha (0.25f));
    g.setFont (theme::font (29.0f, true));
    g.drawText ("MASTER CONTROL PANEL", juce::Rectangle<int> (21, 11, 480, 36), juce::Justification::centredLeft);
    g.setColour (theme::textLo);
    g.setFont (theme::font (12.0f, true));
    g.drawText (juce::String::fromUTF8 ("CLOWNS FROM MARS  \xc2\xb7  INTELLIGENT MASTERING"),
                juce::Rectangle<int> (22, 44, 480, 16), juce::Justification::centredLeft);

    // Panels.
    for (auto& p : panels)
    {
        auto r = p.area.toFloat();
        g.setColour (theme::bg2);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::brassDark.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);

        if (p.title.isNotEmpty())
        {
            auto titleBar = r.removeFromTop (22.0f).reduced (10.0f, 2.0f);
            g.setColour (p.accent);
            g.fillRect (titleBar.removeFromLeft (4.0f).withTrimmedTop (2.0f).withTrimmedBottom (2.0f));
            g.setColour (theme::textHi);
            g.setFont (theme::font (13.0f, true));
            g.drawText (p.title, titleBar.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
        }
    }
}

//==============================================================================
gui::RotarySlider* ControlPanelAudioProcessorEditor::addKnob (const char* id, const juce::String& name, juce::Colour accent)
{
    auto k = std::make_unique<gui::RotarySlider> (processor.apvts, id, name, accent);
    auto* raw = k.get();
    canvas.addAndMakeVisible (*raw);
    knobStore.push_back (std::move (k));
    return raw;
}

juce::ToggleButton* ControlPanelAudioProcessorEditor::addToggle (const char* id, const juce::String& text, juce::Colour accent)
{
    auto b = std::make_unique<juce::ToggleButton> (text);
    auto* raw = b.get();
    raw->setColour (juce::TextButton::buttonOnColourId, accent);
    canvas.addAndMakeVisible (*raw);
    buttonAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, *raw));
    toggleStore.push_back (std::move (b));
    return raw;
}

juce::ComboBox* ControlPanelAudioProcessorEditor::addCombo (const char* id, const juce::StringArray& items)
{
    auto c = std::make_unique<juce::ComboBox>();
    auto* raw = c.get();
    raw->addItemList (items, 1);
    canvas.addAndMakeVisible (*raw);
    comboAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, *raw));
    comboStore.push_back (std::move (c));
    return raw;
}

//==============================================================================
ControlPanelAudioProcessorEditor::ControlPanelAudioProcessorEditor (ControlPanelAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);

    using namespace cfm::params;

    // ---- Global -----------------------------------------------------------
    kInput  = addKnob (id::inputTrim,   "INPUT",    theme::brass);
    kOutput = addKnob (id::outputTrim,  "OUTPUT",   theme::brass);
    kHead   = addKnob (id::headroom,    "HEADROOM", theme::brass);
    kMix    = addKnob (id::mix,         "MIX",      theme::brass);
    kDrift  = addKnob (id::drift,       "DRIFT",    theme::steel);
    kBend   = addKnob (id::circuitBend, "BEND",     theme::clownRed);
    tBypass = addToggle (id::bypass,   "BYPASS",    theme::clownRed);
    tDelta  = addToggle (id::delta,    "DELTA",     theme::clownTeal);
    tAuto   = addToggle (id::autoGain, "AUTO GAIN", theme::tubeGlow);
    cOS     = addCombo  (id::oversample, oversampleChoices());

    // ---- Tube -------------------------------------------------------------
    cModel = addCombo  (id::tubeModel, tubeModelChoices());
    kDrive = addKnob   (id::tubeDrive, "DRIVE", theme::tubeGlow);
    kBias  = addKnob   (id::tubeBias,  "BIAS",  theme::tubeGlow);
    kTone  = addKnob   (id::tubeTone,  "TONE",  theme::tubeGlow);
    tTube  = addToggle (id::tubeOn,    "ON",    theme::tubeGlow);

    // ---- EQ ---------------------------------------------------------------
    tEq     = addToggle (id::eqOn,     "ON",  theme::clownTeal);
    tHP     = addToggle (id::hpOn,     "HPF", theme::clownTeal);
    tLP     = addToggle (id::lpOn,     "LPF", theme::clownTeal);
    tPropQ  = addToggle (id::propQ,    "P-Q", theme::clownTeal);
    tLinear = addToggle (id::eqLinear, "LIN-PHASE", theme::clownRed);
    kHP    = addKnob (id::hpFreq, "HP", theme::clownTeal);
    kLP    = addKnob (id::lpFreq, "LP", theme::clownTeal);
    kAir   = addKnob (id::air,    "AIR", theme::clownTeal);
    kTight = addKnob (id::tight,  "TIGHT", theme::clownTeal);
    const char* bandNames[5] = { "LOW", "LO-MID", "MID", "HI-MID", "HIGH" };
    for (int b = 0; b < 5; ++b)
    {
        kbGain[b] = addKnob (id::bandGain[b], bandNames[b], theme::clownTeal);
        kbFreq[b] = addKnob (id::bandFreq[b], "Hz",  theme::steel);
        kbQ[b]    = addKnob (id::bandQ[b],    "Q",   theme::steel);
    }

    // ---- Compressor -------------------------------------------------------
    tComp   = addToggle (id::compOn,     "ON",    theme::rustBright);
    tAutoMk = addToggle (id::compAutoMk, "AUTO",  theme::rustBright);
    cMode   = addCombo  (id::compMode,    compModeChoices());
    cXfmr   = addCombo  (id::transformer, transformerChoices());
    kThr  = addKnob (id::compThresh,   "THRESH",  theme::rustBright);
    kRat  = addKnob (id::compRatio,    "RATIO",   theme::rustBright);
    kAtt  = addKnob (id::compAttack,   "ATTACK",  theme::rustBright);
    kRel  = addKnob (id::compRelease,  "RELEASE", theme::rustBright);
    kKnee = addKnob (id::compKnee,     "KNEE",    theme::rustBright);
    kMk   = addKnob (id::compMakeup,   "MAKEUP",  theme::rustBright);
    kOpt  = addKnob (id::compOptBlend, "OPT/DISC",theme::rustBright);
    kCMix = addKnob (id::compMix,      "MIX",     theme::rustBright);
    kSc   = addKnob (id::scHpf,        "SC-HP",   theme::steel);

    // ---- Tape -------------------------------------------------------------
    tTape   = addToggle (id::tapeOn, "ON", theme::brass);
    cSpeed  = addCombo  (id::tapeSpeed, tapeSpeedChoices());
    kTDrive = addKnob (id::tapeDrive, "DRIVE", theme::brass);
    kTLF    = addKnob (id::tapeLF,    "LOW",   theme::brass);
    kTHF    = addKnob (id::tapeHF,    "HIGH",  theme::brass);

    // ---- Stereo -----------------------------------------------------------
    kWidth = addKnob (id::width,    "WIDTH", theme::clownTeal);
    kMono  = addKnob (id::monoFreq, "MONO",  theme::clownTeal);

    // ---- Header widgets ---------------------------------------------------
    presetPrev.onClick = [this] { selectProgram (-1); };
    presetNext.onClick = [this] { selectProgram (+1); };
    canvas.addAndMakeVisible (presetPrev);
    canvas.addAndMakeVisible (presetNext);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) processor.setCurrentProgram (idx);
    };
    canvas.addAndMakeVisible (presetBox);

    static const char* snapNames[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        snapButtons[i].setButtonText (snapNames[i]);
        snapButtons[i].setClickingTogglesState (false);
        snapButtons[i].onClick = [this, i]
        {
            if (processor.getActiveSnapshot() == i || ! processor.hasSnapshot (i))
                processor.storeSnapshot (i);
            else
                processor.recallSnapshot (i);
            refreshSnapshotButtons();
        };
        canvas.addAndMakeVisible (snapButtons[i]);
    }
    processor.storeSnapshot (0); // seed slot A with the loaded state

    fingerprint.setJustificationType (juce::Justification::centredRight);
    fingerprint.setColour (juce::Label::textColourId, theme::textLo);
    fingerprint.setFont (theme::font (11.0f));
    fingerprint.setText ("UNIT #" + juce::String::toHexString (processor.getSerial()).toUpperCase(),
                         juce::dontSendNotification);
    canvas.addAndMakeVisible (fingerprint);

    autoGainLabel.setJustificationType (juce::Justification::centred);
    autoGainLabel.setColour (juce::Label::textColourId, theme::tubeGlow);
    autoGainLabel.setFont (theme::font (11.0f, true));
    autoGainLabel.setText ("AUTO 0.0 dB", juce::dontSendNotification);
    canvas.addAndMakeVisible (autoGainLabel);

    // ---- Meters / analyzer ------------------------------------------------
    analyzer = std::make_unique<gui::SpectrumAnalyzer> (processor.getAnalyzer(),
                [this] (double hz) { return processor.getEqMagnitudeDb (hz); });
    canvas.addAndMakeVisible (*analyzer);

    levelMeter = std::make_unique<gui::LevelMeter> (processor.getOutputMeter());
    canvas.addAndMakeVisible (*levelMeter);

    grMeter = std::make_unique<gui::GainReductionMeter> ([this] { return processor.getGainReductionDb(); });
    canvas.addAndMakeVisible (*grMeter);

    // ---- Right-side art (Mars clown) --------------------------------------
    artPanel.image = juce::ImageCache::getFromMemory (BinaryData::clown_jpg, BinaryData::clown_jpgSize);
    canvas.addAndMakeVisible (artPanel);

    // ---- Loudness (LUFS) readout ------------------------------------------
    lufsCaption.setText (juce::String::fromUTF8 ("LOUDNESS  \xc2\xb7  LUFS"), juce::dontSendNotification);
    lufsCaption.setFont (theme::font (10.5f, true));
    lufsCaption.setColour (juce::Label::textColourId, theme::textMid);
    lufsCaption.setJustificationType (juce::Justification::centredLeft);
    canvas.addAndMakeVisible (lufsCaption);

    auto styleL = [this] (juce::Label& l, juce::Colour c)
    {
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, c);
        l.setFont (theme::font (13.0f, true));
        canvas.addAndMakeVisible (l);
    };
    styleL (lufsM, theme::clownTeal);
    styleL (lufsS, theme::tubeGlow);
    styleL (lufsI, theme::rustBright);
    lufsM.setText ("M  --", juce::dontSendNotification);
    lufsS.setText ("S  --", juce::dontSendNotification);
    lufsI.setText ("I  --", juce::dontSendNotification);

    lufsReset.onClick = [this] { processor.resetIntegratedLoudness(); };
    canvas.addAndMakeVisible (lufsReset);

    refreshSnapshotButtons();

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
        setResizeLimits (baseW * 55 / 100, baseH * 55 / 100, baseW * 3 / 2, baseH * 3 / 2);
    }
    setSize (baseW * 88 / 100, baseH * 88 / 100);

    startTimerHz (12);
}

ControlPanelAudioProcessorEditor::~ControlPanelAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void ControlPanelAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg0);   // letterbox behind the scaled canvas
}

void ControlPanelAudioProcessorEditor::timerCallback()
{
    if (analyzer) analyzer->setSampleRate (processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0);

    const int prog = processor.getCurrentProgram();
    if (presetBox.getSelectedId() != prog + 1)
        presetBox.setSelectedId (prog + 1, juce::dontSendNotification);

    autoGainLabel.setText ("AUTO " + juce::String (processor.getAutoGainDb(), 1) + " dB", juce::dontSendNotification);

    auto fmt = [] (float v) { return v <= -99.0f ? juce::String::fromUTF8 ("-\xe2\x88\x9e")
                                                 : juce::String (v, 1); };
    lufsM.setText ("M  " + fmt (processor.getMomentaryLufs()),  juce::dontSendNotification);
    lufsS.setText ("S  " + fmt (processor.getShortTermLufs()),  juce::dontSendNotification);
    lufsI.setText ("I  " + fmt (processor.getIntegratedLufs()), juce::dontSendNotification);

    refreshSnapshotButtons();
}

void ControlPanelAudioProcessorEditor::refreshSnapshotButtons()
{
    for (int i = 0; i < 4; ++i)
    {
        const bool active = processor.getActiveSnapshot() == i;
        snapButtons[i].setToggleState (active, juce::dontSendNotification);
        snapButtons[i].setColour (juce::TextButton::buttonColourId,
                                  processor.hasSnapshot (i) ? theme::brassDark : theme::bg2);
    }
}

void ControlPanelAudioProcessorEditor::selectProgram (int delta)
{
    const int n = processor.getNumPrograms();
    if (n <= 0) return;
    int idx = (processor.getCurrentProgram() + delta + n) % n;
    processor.setCurrentProgram (idx);
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

//==============================================================================
void ControlPanelAudioProcessorEditor::resized()
{
    // Scale the fixed-size canvas to fit while keeping aspect ratio.
    const float scale = juce::jmin ((float) getWidth() / baseW, (float) getHeight() / baseH);
    const float offX  = (getWidth()  - scale * baseW) * 0.5f;
    const float offY  = (getHeight() - scale * baseH) * 0.5f;
    canvas.setBounds (0, 0, baseW, baseH);
    canvas.setTransform (juce::AffineTransform::scale (scale).translated (offX, offY));

    auto all = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (16);
    canvas.panels.clear();

    // ---- Header row (wordmark drawn by Canvas) ----------------------------
    auto header = all.removeFromTop (54);
    {
        auto right = header.removeFromRight (360);
        // snapshots + fingerprint (top), auto-gain (below)
        auto snapRow = right.removeFromTop (26);
        fingerprint.setBounds (snapRow.removeFromRight (150));
        for (int i = 0; i < 4; ++i)
            snapButtons[i].setBounds (snapRow.removeFromRight (34).reduced (2, 0));
        autoGainLabel.setBounds (right.removeFromRight (150));

        auto centre = header.removeFromRight (330);
        presetPrev.setBounds (centre.removeFromLeft (30).reduced (2));
        presetNext.setBounds (centre.removeFromRight (30).reduced (2));
        presetBox.setBounds (centre.reduced (4, 6));
    }
    all.removeFromTop (6);

    // ---- Global strip -----------------------------------------------------
    auto global = all.removeFromTop (112);
    canvas.panels.push_back ({ "GLOBAL", global, theme::brass });
    {
        auto g = global.reduced (10); g.removeFromTop (22);

        // Loudness (LUFS) block pinned to the right of the strip.
        auto loud = g.removeFromRight (250);
        lufsCaption.setBounds (loud.removeFromTop (16).reduced (2, 0));
        auto la = loud.removeFromTop (24);
        lufsM.setBounds (la.removeFromLeft (la.getWidth() / 2).reduced (2, 0));
        lufsS.setBounds (la.reduced (2, 0));
        auto lb = loud.removeFromTop (26);
        lufsReset.setBounds (lb.removeFromRight (92).reduced (2, 2));
        lufsI.setBounds (lb.reduced (2, 0));

        auto knobs = g.removeFromLeft (6 * 92);
        for (auto* k : { kInput, kOutput, kHead, kMix, kDrift, kBend })
            k->setBounds (knobs.removeFromLeft (92).reduced (4));

        auto togs = g.removeFromLeft (140);
        tBypass->setBounds (togs.removeFromTop (24));
        tDelta ->setBounds (togs.removeFromTop (24));
        tAuto  ->setBounds (togs.removeFromTop (24));

        auto osArea = g.removeFromLeft (150).withSizeKeepingCentre (150, 26);
        cOS->setBounds (osArea);
    }
    all.removeFromTop (8);

    // ---- Split main area: art + meter on the right ------------------------
    auto artCol = all.removeFromRight (224);
    artPanel.setBounds (artCol);
    all.removeFromRight (10);
    auto meterCol = all.removeFromRight (92);
    levelMeter->setBounds (meterCol);
    all.removeFromRight (12);

    // ---- EQ panel ---------------------------------------------------------
    auto eq = all.removeFromTop (372);
    canvas.panels.push_back ({ "EQUALISER", eq, theme::clownTeal });
    {
        auto body = eq.reduced (10); body.removeFromTop (22);
        analyzer->setBounds (body.removeFromTop (150));
        body.removeFromTop (8);

        auto filters = body.removeFromRight (300);
        body.removeFromRight (8);

        // 5 band columns: Gain (top) + Freq/Q pair (bottom).
        const int colW = body.getWidth() / 5;
        for (int b = 0; b < 5; ++b)
        {
            auto col = body.removeFromLeft (colW);
            kbGain[b]->setBounds (col.removeFromTop (96).reduced (3));
            auto pair = col;
            kbFreq[b]->setBounds (pair.removeFromLeft (pair.getWidth() / 2).reduced (2));
            kbQ[b]   ->setBounds (pair.reduced (2));
        }

        // Filters: 2x2 knobs + two toggle rows.
        auto togRow2 = filters.removeFromBottom (20);
        tLinear->setBounds (togRow2.removeFromLeft (150));
        tEq    ->setBounds (togRow2.removeFromRight (66));
        auto togRow1 = filters.removeFromBottom (20);
        tHP   ->setBounds (togRow1.removeFromLeft (72));
        tLP   ->setBounds (togRow1.removeFromLeft (72));
        tPropQ->setBounds (togRow1.removeFromLeft (86));
        filters.removeFromBottom (4);
        auto r1 = filters.removeFromTop (filters.getHeight() / 2);
        kHP ->setBounds (r1.removeFromLeft (r1.getWidth() / 2).reduced (4));
        kLP ->setBounds (r1.reduced (4));
        kAir  ->setBounds (filters.removeFromLeft (filters.getWidth() / 2).reduced (4));
        kTight->setBounds (filters.reduced (4));
    }
    all.removeFromTop (8);

    // ---- Bottom row: Tube | Comp | Tape | Stereo --------------------------
    auto bottom = all; // remaining height
    auto tubeP = bottom.removeFromLeft (244); bottom.removeFromLeft (8);
    auto compP = bottom.removeFromLeft (392); bottom.removeFromLeft (8);
    auto tapeP = bottom.removeFromLeft (208); bottom.removeFromLeft (8);
    auto stereoP = bottom;

    canvas.panels.push_back ({ "TUBES / MOJO", tubeP, theme::tubeGlow });
    canvas.panels.push_back ({ "COMPRESSOR", compP, theme::rustBright });
    canvas.panels.push_back ({ "TAPE", tapeP, theme::brass });
    canvas.panels.push_back ({ "STEREO", stereoP, theme::clownTeal });

    // Tube
    {
        auto b = tubeP.reduced (10); b.removeFromTop (22);
        auto top = b.removeFromTop (26);
        cModel->setBounds (top.removeFromLeft (150));
        tTube ->setBounds (top.removeFromRight (60));
        b.removeFromTop (4);
        const int w = b.getWidth() / 3;
        kDrive->setBounds (b.removeFromLeft (w).reduced (3));
        kBias ->setBounds (b.removeFromLeft (w).reduced (3));
        kTone ->setBounds (b.reduced (3));
    }

    // Comp
    {
        auto b = compP.reduced (10); b.removeFromTop (22);
        auto top = b.removeFromTop (26);
        tComp  ->setBounds (top.removeFromLeft (70));
        tAutoMk->setBounds (top.removeFromLeft (74));
        cMode  ->setBounds (top.removeFromLeft (110).reduced (2, 1));
        cXfmr  ->setBounds (top.removeFromLeft (110).reduced (2, 1));
        grMeter->setBounds (b.removeFromTop (16).reduced (0, 1));
        b.removeFromTop (4);

        auto row1 = b.removeFromTop (b.getHeight() / 2);
        gui::RotarySlider* r1[5] = { kThr, kRat, kAtt, kRel, kKnee };
        gui::RotarySlider* r2[4] = { kMk, kOpt, kCMix, kSc };
        const int w1 = row1.getWidth() / 5;
        for (int i = 0; i < 5; ++i) r1[i]->setBounds (row1.removeFromLeft (w1).reduced (2));
        const int w2 = b.getWidth() / 4;
        for (int i = 0; i < 4; ++i) r2[i]->setBounds (b.removeFromLeft (w2).reduced (2));
    }

    // Tape
    {
        auto b = tapeP.reduced (10); b.removeFromTop (22);
        auto top = b.removeFromTop (26);
        cSpeed->setBounds (top.removeFromLeft (100));
        tTape ->setBounds (top.removeFromRight (56));
        b.removeFromTop (4);
        const int w = b.getWidth() / 3;
        kTDrive->setBounds (b.removeFromLeft (w).reduced (3));
        kTLF   ->setBounds (b.removeFromLeft (w).reduced (3));
        kTHF   ->setBounds (b.reduced (3));
    }

    // Stereo
    {
        auto b = stereoP.reduced (10); b.removeFromTop (22);
        kWidth->setBounds (b.removeFromTop (b.getHeight() / 2).reduced (3));
        kMono ->setBounds (b.reduced (3));
    }
}
