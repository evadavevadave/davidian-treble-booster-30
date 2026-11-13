# Davidian Treble Booster 30

A circuit-modelled guitar amplifier plugin (LV2, VST3 and Audio Unit), built from the manufacturer's works drawing of a British three-channel, 30-watt valve amplifier, with its listed modifications, component for component: three input channels on ECC83s, a 12AU7 vibrato modulator driven by a phase-shift oscillator, the Cut control, four cathode-biased EL84s in class A, a valve rectifier that sags, and the output transformer into a reactive speaker load.

**A close model, not an exact copy.** It is a simulation of the circuit from its component values, not a sampled or measured amp, and some details are reasoned estimates (listed under [Estimates](#estimates)). It has not been compared side by side with a built unit, so expect it to sound close to the hardware rather than identical.

![The GUI](docs/gui.png)

A mono amp (on a stereo track the input is mixed to mono and the output goes to every output channel), with the same GUI in every format. GPL-3.0-or-later.

| Format | Windows | macOS | Linux |
|---|---|---|---|
| LV2 | yes | yes | yes |
| VST3 | yes | yes | yes |
| Audio Unit (AUv2) | | yes | |

## Download

Ready-made builds are on the [Releases page](https://github.com/evadavevadave/davidian-treble-booster-30/releases):

| Download | Contains | For |
|---|---|---|
| `…-windows-x64.zip` | LV2, VST3 | Windows 10/11, 64-bit hosts |
| `…-macos-universal.zip` | LV2, VST3, Audio Unit | macOS 10.13 or later, Apple silicon and Intel |
| `…-linux-x64.zip` | LV2, VST3 | 64-bit Intel/AMD Linux (LV2: glibc 2.29 or later, e.g. Ubuntu 20.04, Debian 11; VST3: glibc 2.34, e.g. Ubuntu 22.04, Debian 12) |
| `…-linux-arm64.zip` | LV2, VST3 | 64-bit ARM Linux, with the same glibc needs |

Unzip, copy the plugin folder for your host's format into its plugin folder, and restart your host (or rescan plugins):

| Format | Copy | Into |
|---|---|---|
| VST3 (Windows) | `Davidian Treble Booster 30.vst3` | `C:\Program Files\Common Files\VST3` |
| VST3 (macOS) | `Davidian Treble Booster 30.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| VST3 (Linux) | `Davidian Treble Booster 30.vst3` | `~/.vst3` |
| Audio Unit (macOS) | `Davidian Treble Booster 30.component` | `~/Library/Audio/Plug-Ins/Components` |
| LV2 (Windows) | `treble30.lv2` | `%APPDATA%\LV2` (create it if it doesn't exist) |
| LV2 (macOS) | `treble30.lv2` | `~/Library/Audio/Plug-Ins/LV2` |
| LV2 (Linux) | `treble30.lv2` | `~/.lv2` |

In the host it is listed as **Treble Booster 30** by **Davidian**. The LV2 plugin URI is `urn:davidian:treble-30`.

The macOS build isn't notarized, so macOS may block it after download. Run these once in Terminal after copying:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Davidian Treble Booster 30.component"
```

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Davidian Treble Booster 30.vst3"
```

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/LV2/treble30.lv2
```

Logic Pro and GarageBand only show an Audio Unit once macOS has registered it. If it doesn't appear, log out and back in, or restart the Mac.

## Controls

| Panel | Control | Circuit part |
|---|---|---|
| Input | Channel, Jack | Which jack the guitar is plugged into: Bright (J1/J2), Normal (J3/J4) or Vib-Trem (J5/J6), Input 1 or Input 2. Input 1 feeds the grid through both 68K resistors; Input 2 through one, with the other to ground (6 dB less). Only one jack is used at a time. |
| Bright | Volume | VR2, 500K log. The Bright channel reaches it only through C3 (500 pF), loaded by R74 (150K), so it is very thin on its own: its bass rolls off from about 2.8 kHz. That is what the drawing shows. |
| Normal | Volume | VR1, 500K log, fed through C2 (0.05 µF). Both channels share V1's cathode resistor, so a little of each bleeds into the other. |
| Vib-Trem | Volume | VR4, 500K log, after the vibrato modulator and its four-section high-pass. This channel goes to the other grid of the phase splitter. |
| Vib-Trem | Speed | SW1: Slow, Medium, Fast (R65 1M, R64 560K, R63 330K in the oscillator's phase-shift ladder). The model runs at about 5.9, 7.0 and 8.4 Hz. |
| Vib-Trem | Mode | SW3 with C38: Vibrato, or Tremolo (one side of the modulator shorted). |
| Tone | Cut | VR3 (250K log) with C10 between the output valves' grids. Turning it up takes the top off. |
| Presets | Depth | VR5, the internal depth preset (500K log with R73). |
| Presets | Input V | Not on the drawing: the volts at the input jack that a full-scale sample stands for (default 1 V). |
| Output | Speaker | Not on the drawing: On adds the acoustic response of an open-back 2x12 combo with a close microphone. Off gives the voltage at the speaker terminals, with the speakers' electrical load still in the circuit. |
| Output | Level | Not on the drawing: output trim. 0 dB puts 30 V at the speaker terminals (about the rated power) at full scale. |
| Footswitches | Vib-Trem | SW2. Off stops the oscillator. The amber lamp beside it follows the oscillator. |

The host's bypass crossfades to the dry signal in 20 ms, delayed to match the plugin's latency.

## How it is modelled

Every resistor, capacitor, pot, switch and valve in the signal path, the oscillator and the power supply filter comes from the drawing, wired as drawn. The model (`src/dsp/`) solves each section as a nonlinear circuit (valve equations, Newton iteration, trapezoidal capacitors and inductors) at about 96 kHz inside.

The drawing is a hand-traced works copy, and some crossings are hard to tell from joins. These were settled by tracing each wire to both ends and checking that the result works as a circuit:

- The two halves of V1 have separate anodes (Bright to C3, Normal to C2) and share the cathode resistor.
- VR1's cold end and R74 return to ground.
- The vibrato: V7b is a split-load stage; two compensated dividers across its cathode load (R54–R57, C22–C25), each with an anti-phase feed from its anode (C26/R58, C27/R59), drive V8's two grids through C20 and C21. V8's anodes share R41/C41 and R40, and its output goes through the C12–C15 high-pass to VR4.
- The oscillator: V9a is a three-section phase-shift oscillator; V9b is a split-load stage whose anode (C34) and cathode (C37) drive V8's grid returns in anti-phase. The depth preset is a divider against R69 at V9b's input.
- SW1 is taken as a three-position switch, and SW2 as shorting the oscillator's ladder.

Left out: the mains voltage selector, the heaters and the mains switch, which do not affect the sound.

### Checked against the drawing

Measured from the model by `test_dsp`: 31.5 W into 15 Ω flat out with 21 V of supply sag; Cut at 10 takes 18.5 dB off at 5 kHz; vibrato swings the pitch by about 43 cents with 4.6 dB of level change, and tremolo swings the level by 38 dB.

### Estimates

The drawing does not give these, so they are reasoned estimates (each is marked "est." in `src/dsp/amp.cpp`):

- Valve curves: Koren's published fits for the ECC83 and 12AU7, and an EL84 fit set to the datasheet's 48 mA point.
- Rectifier and mains transformer: a 390 V source behind 330 Ω, which idles the output stage near 337 V with 12.4 V on the cathode resistor (about 39 mA and 11.6 W a valve, so class A as built). The 100 Hz ripple is left out.
- Choke: 15 H (the drawing says 10–20 H), 150 Ω.
- Output transformer: 4 kΩ anode to anode to the 15 Ω tap (the figure collectors give for this amplifier), with magnetising and leakage inductance set for an 18 Hz to 16 kHz band.
- Speakers: a 15 Ω load with voice-coil inductance and an 85 Hz cone resonance.

## Build

Needs CMake 3.25 or later and a C/C++ compiler; Linux also needs the X11 development files. The VST3 SDK (and on macOS, Apple's AudioUnitSDK) is downloaded while configuring.

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build-cmake --config Release
```

The outputs land in `build-cmake/treble30.lv2`, `build-cmake/VST3/Release/` and, on macOS, `build-cmake/AU/`. Steinberg's validator runs on the VST3 after each build. Turn formats off with `-DTB_VST3=OFF` or `-DTB_AU=OFF`.

### Windows, LV2 only (Visual Studio)

Without CMake, run `build.bat` (Visual Studio 2017 or later with the C++ tools). It builds `build\treble30.lv2` and the test programs in `build-test`.

```bash
build.bat
```

## Tests

With CMake:

```bash
ctest --test-dir build-cmake -C Release --output-on-failure
```

With `build.bat` on Windows:

```bash
build-test/test_dsp.exe build/treble30.lv2
```

```bash
build-test/ui_check.exe build/treble30.lv2
```

```bash
build-test/lilv_check.exe build
```

- `test_dsp` checks the circuit and the plugin, including the figures under [Checked against the drawing](#checked-against-the-drawing), and CPU cost: about 19% of one core at 48 kHz with a guitar-level signal, about 28% with the Vib-Trem channel in use (the vibrato section is skipped while its volume is at 0), on the development machine.
- `params_check` checks the VST3/AU parameter table against the LV2 metadata and the panel.
- `ui_render` draws the panel to a BMP file.
- `ui_check` (Windows) drives the GUI with mouse messages.
- `lilv_check` (Windows) validates the bundle with the LV2 parser from an Ardour 9 install.
- `vst3_check` hosts the VST3, drives its editor and compares its sound with the LV2 build.
- `au_check` (macOS) does the same for the Audio Unit.

GitHub Actions builds every format on Windows, macOS, Linux x64 and Linux arm64 for every push (see `.github/workflows/build.yml`). Pushing a `v*` tag publishes a release with a zip for each platform.

## Use of AI

Made with generative AI (Claude Code), directed and reviewed by Davidian. See [AI_USAGE.md](AI_USAGE.md) for what the author and the AI each did, and what has and hasn't been checked.

## References

- Valve models: Norman Koren, *Improved VT models for SPICE simulations* (1996, revised 2003), and the EL84 datasheet.
- Nonlinear circuit solution with Newton iteration and trapezoidal companion models: standard circuit simulation (e.g. Vlach and Singhal, *Computer Methods for Circuit Analysis and Design*).

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) (third-party parts and sources) and [AI_USAGE.md](AI_USAGE.md). VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units and macOS are trademarks of Apple Inc.
