Ready-made **Davidian Treble Booster 30** plugins for Windows, macOS and Linux, built and tested from the same source by GitHub Actions: a circuit model of a 1960–61 three-channel 30-watt valve amplifier with vibrato and tremolo.

## Downloads

| File | For | Contains |
|---|---|---|
| `...-windows-x64.zip` | 64-bit Windows 10 and 11 | VST3 and LV2 |
| `...-macos-universal.zip` | macOS 10.13 and later, Apple silicon and Intel | Audio Unit, VST3 and LV2 |
| `...-linux-x64.zip` | 64-bit Linux on x86 (glibc 2.29 or later, such as Ubuntu 20.04 or Debian 11) | VST3 and LV2 |
| `...-linux-arm64.zip` | 64-bit Linux on ARM, such as a Raspberry Pi 4 or 5 | VST3 and LV2 |

## Install

Unzip, close your host, and copy the format it uses:

- **VST3:** copy `Davidian Treble Booster 30.vst3` into `C:\Program Files\Common Files\VST3` (Windows), `~/Library/Audio/Plug-Ins/VST3` (macOS) or `~/.vst3` (Linux).
- **Audio Unit (macOS):** copy `Davidian Treble Booster 30.component` into `~/Library/Audio/Plug-Ins/Components`. It isn't notarised, so clear the download quarantine once in Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Davidian Treble Booster 30.component"`.
- **LV2:** copy the `treble30.lv2` folder into `%APPDATA%\LV2` (Windows), `~/Library/Audio/Plug-Ins/LV2` (macOS, then clear the quarantine the same way) or `~/.lv2` (Linux).

Restart the host (or rescan its plugins) and look for **Treble Booster 30** by **Davidian**.

## Testing status

- Every build passes the circuit tests, the parameter check and the GUI render test.
- **VST3:** passes Steinberg's validator on every system. On Windows a test host drives its editor with simulated mouse input, and its output matches the LV2 build sample for sample.
- **Audio Unit:** checked with Apple's `auval` in strict mode, for both Apple silicon and Intel, and compared with the LV2 build sample for sample.
- The Linux bundles are also checked with the lilv LV2 parser.
- Not yet tried in a DAW by a person.

## Licence and AI usage

- **Licence:** GPL-3.0-or-later. The licence, `NOTICE.md` and `AI_USAGE.md` are inside each bundle. The VST3 includes the Steinberg VST3 SDK (MIT) and the Audio Unit includes Apple's AudioUnitSDK (Apache 2.0). Their licences are in the bundles too.
- **AI usage:** this plugin was made with generative AI (Claude Code). See `AI_USAGE.md`.

VST is a registered trademark of Steinberg Media Technologies GmbH.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
