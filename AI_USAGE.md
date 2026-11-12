# AI usage

This plugin was built with generative AI.

- **Tool:** Claude Code, Anthropic's coding assistant, using Anthropic's Claude models (Opus).
- **Human role:** Davidian set the requirements and the design direction, supplied the reference schematic, chose the name, and decides what is released.
- **What the AI did:**
  - Read the topology and component values off the reference schematic, including tracing wires through its hand-drawn crossings.
  - Wrote the circuit netlists and adapted the Davidian circuit solver, valve models and oversampler to them.
  - Wrote the LV2 plugin and GUI, and adapted the VST3 and Audio Unit wrappers around the same engine and panel.
  - Wrote the build files, the LV2 metadata, the tests, the CI workflow and the documentation.
  - Looked up published figures online (the output transformer's impedance, the valves' roles) to fill gaps in the drawing.
- **How it was checked:**
  - Automated tests in `test/`: the operating point, the channels' voicing, the Cut control, the oscillator's three speeds, vibrato, tremolo and the footswitch, output power and supply sag, agreement between sample rates, and loading the plugin library the way a host does.
  - The GUI is driven with simulated mouse input in a test host, and the bundle is checked with the lilv LV2 parser.
  - The VST3 passes Steinberg's validator, and a test host compares its output with the LV2 build sample for sample (identical on Windows). On macOS the Audio Unit is compared the same way and checked with Apple's `auval` in CI.
  - CMake (`ctest`) and `build.bat` build the tests, and the README lists the commands that run them.
  - The macOS and Linux builds are built and tested by CI only; they have not been run on a desk.
  - No listening test in a DAW has been recorded yet.
- **Training data and copyright:** no third-party code was knowingly copied. The only reproduced material is the LV2 interface declarations, credited in [NOTICE.md](NOTICE.md). The VST3 and Audio Unit builds link the Steinberg VST3 SDK and Apple's AudioUnitSDK unmodified, under their own licences (also in NOTICE.md). The GUI artwork and font are generated from code, not from images.

The AI's output is released under the project licence, GPL-3.0-or-later (see `LICENSE`). Under the GPL, there is no warranty.
