# CLOWNS FROM MARS — Master Control Panel

**An intelligent, analog-modelled mastering processor — by *Clowns From Mars***

Master Control Panel is a single-window mastering chain: corrective/tonal EQ, a
two-stage transformer-balanced compressor, oversampled Class-A tube drive,
tape saturation, and mid/side stereo finishing — with honest metering, matched
A/B, and a rusted-brass "control panel bolted together on Mars" interface.

It is a real, buildable [JUCE](https://juce.com) plugin (VST3 / Standalone, and
AU on macOS), written for top-tier DSP quality and boring, reliable behaviour
across DAWs, sample rates and buffer sizes.

### Fidelity

- **64-bit double precision end to end.** Every filter, envelope, saturator and
  gain stage runs at 64-bit; when the host offers double-precision buffers we
  process them natively, otherwise we widen 32→64-bit, process, and narrow back
  — so the DSP is always at reference precision regardless of host settings.
- **Exact transcendentals.** Saturation uses the true `tanh` (not a rational
  approximation) so the harmonic series is correct; nonlinear stages run inside
  double-precision oversampling (default 4×, up to 8×) for clean anti-aliasing.
- **IEEE-correct math** (no `-ffast-math`), denormals flushed, per-sample
  smoothed gain staging (no zipper), and **true-peak (4× inter-sample) metering**.

---

## Signal flow

```
 Input Trim
   → EQ            5 bands + HP/LP + Baxandall-style AIR/TIGHT, proportional-Q
   → Compressor    2-stage (optical + discrete), Stereo / Dual-Mono / Mid-Side,
                   transformer voicing (Nickel/Iron/Steel), external sidechain
   → [ +Headroom → Oversample( Tube MOJO → Tape ) → −Headroom ]
   → Stereo        Mono Maker + Width (M/S)
   → Circuit Bend  creative bit-crush / degrade
   → Output Trim
   → Auto-Gain match → Dry/Wet mix   (or Delta / Listen)
```

The two nonlinear stages (tube + tape) run inside a selectable oversampling
block so their harmonics fold back cleanly. The dry path is delayed to match
the oversampling latency, so **Mix**, **Delta** and **Bypass** stay
phase-aligned.

## Feature checklist

Every bullet from the product brief, and how it's implemented:

| Requirement | Where |
|---|---|
| Clean nonlinear processing (anti-aliasing) | `juce::dsp::Oversampling` (FIR half-band), Off/2×/4×/8×, around tube + tape |
| Unique sonic character / harmonic coloration | Asymmetric triode/pentode/starved shapers with steerable bias (even/odd balance) |
| Modeling dynamic behavior, not the static curve | Program-dependent optical release; tape hysteresis term; DC-servo operating point |
| Tuned, musical parameter ranges | All ranges in `Parameters.cpp` are skewed to their sweet spots |
| Trustworthy metering & tight UX | Peak+RMS output meter with −18 dBFS reference, live GR meter, FFT analyzer |
| Thread safety, denormals, SIMD, latency reporting | Atomic param reads, `ScopedNoDenormals` + explicit flush, `FloatVectorOperations`, `setLatencySamples` |
| A clear point of view | Opinionated mastering chain + presets that teach it |
| CPU efficiency | Header-only per-sample DSP, block ops, oversampling only around nonlinearities |
| Preset quality | 37 factory presets across genres/use-cases, each teaching a different facet |
| Consistent gain staging / reference levels | Headroom control + Auto-Gain match + honest meters |
| Stable across DAWs / SRs / buffers | Verified across 44.1/48/96 kHz × 32/128/512 buffers (see `Tools/verify_main.cpp`) |
| Selectable oversampling | Off / 2× / 4× / 8× with reported latency |
| Auto gain-compensation for matched A/B | Continuous input-vs-output RMS match |
| Delta / listen mode | Outputs `processed − dry` to hear exactly what's added/removed |
| Built-in dry/wet mix | Equal-power global Mix, plus per-module compressor Mix |
| Component variation / analog drift | Serial-seeded per-channel randomisation (`DriftModel`) |
| Flexible sidechain (external + detector filtering) | Optional stereo sidechain bus + detector high-pass |
| Mid/side & stereo-width handling | M/S compressor mode, Mono Maker, Width |
| A/B/C/D snapshots | Four in-editor snapshot slots, persisted with state |
| Analog-matched filter/phase design, clean near Nyquist | RBJ biquads, frequencies clamped below Nyquist |
| Full format coverage, no dongle | VST3 + Standalone (+ AU on macOS), Apple-Silicon-ready CMake |

## Controls

- **Global** — Input, Output, Headroom, Mix, Drift, Circuit Bend; Bypass, Delta,
  Auto-Gain; Oversampling.
- **Tubes · MOJO** — three amp voicings (Triode / Pentode / Starved), Drive,
  Bias (even↔odd harmonic steer), Tone.
- **Equaliser** — five bands (low-shelf, three peaks, high-shelf), HP/LP filters,
  AIR and TIGHT shelves, Proportional-Q toggle, live spectrum + response curve.
- **Compressor** — Threshold, Ratio, Attack, Release, Knee, Makeup (Auto),
  Opt/Disc blend, Mix, SC-HP, Mode, Transformer, GR meter.
- **Tape** — Speed (15/30 ips), Drive, Low, High.
- **Stereo** — Width, Mono Maker frequency.

The window is aspect-locked and fully scalable — drag any corner.

---

## Building

Prerequisites: CMake ≥ 3.22 and a C++17 compiler. JUCE 8.0.14 is fetched
automatically. On Linux, install the usual JUCE dev packages:

```bash
sudo apt-get install libasound2-dev libx11-dev libxext-dev libxinerama-dev \
    libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype6-dev libfontconfig1-dev
```

Then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MasterControlPanel_VST3 -j
# also: --target MasterControlPanel_Standalone
```

The VST3 is written to `build/MasterControlPanel_artefacts/Release/VST3/` and copied to
your user plugin folder.

### Offline against a local JUCE checkout

```bash
cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
```

### Verification harness

```bash
cmake -B build -DTT_BUILD_VERIFY=ON
cmake --build build --target tt_verify -j
./build/tt_verify ui.png     # runs DSP smoke tests, renders the UI to ui.png
```

## Windows installer

JUCE does not support MinGW, so a Windows build must be produced with MSVC on
Windows. The `.github/workflows/windows-installer.yml` workflow does this on a
GitHub-hosted Windows runner: it builds the VST3 with MSVC, packages an
[Inno Setup](installer/master-control-panel.iss) installer, uploads it as a
build artifact, and attaches it to a rolling **`windows-latest`** pre-release.

- Trigger it from **Actions → Windows Installer → Run workflow**, or by pushing
  to the dev branch.
- Download `ClownsFromMars-MasterControlPanel-1.0.0-Windows-x64-Setup.exe` from
  the run's artifacts or the `windows-latest` release.
- Run it (close your DAW first). It installs the plugin to
  `C:\Program Files\Common Files\VST3` and it appears after a plugin rescan.

To build the installer locally on a Windows machine:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DTT_COPY_PLUGIN=OFF
cmake --build build --config Release --target MasterControlPanel_VST3
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\master-control-panel.iss
```

## Project layout

```
CMakeLists.txt
Source/
  Parameters.{h,cpp}        parameter registry (IDs, musical ranges, layout)
  Presets.h                 factory presets
  PluginProcessor.{h,cpp}   chain assembly, oversampling, latency, A/B, state
  PluginEditor.{h,cpp}      scalable control-panel layout
  dsp/
    DspCommon.h  DriftModel.h  TubeStage.h  EqualizerModule.h
    CompressorModule.h  TapeModule.h  StereoModule.h  Metering.h
  gui/
    Theme.h  ControlPanelLookAndFeel.{h,cpp}  RotarySlider.{h,cpp}
    LevelMeter.{h,cpp}  GainReductionMeter.h  SpectrumAnalyzer.{h,cpp}
Tools/verify_main.cpp       DSP smoke test + UI snapshot (not shipped)
```

## Notes

- AU/AAX: the CMake targets VST3 + Standalone by default. Add `AU` to `FORMATS`
  on macOS (Xcode); AAX additionally needs Avid's SDK via `juce_add_aax_sdk`.
- The "unique digital fingerprint" is a serial-seeded drift model persisted in
  the plugin state, so a given instance keeps its subtle channel character.
