# Notices and licences

## Davidian Treble Booster 30

Copyright (C) 2026 Davidian

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program (`LICENSE`). If not, see <https://www.gnu.org/licenses/>.

GPL-3.0-or-later covers everything in this repository unless listed below: the DSP and GUI source, the tests, the build files, the LV2 metadata (`bundle/`), the GUI artwork and stroke font (both drawn in code), `docs/gui.png` (rendered by the plugin's own GUI code) and the documentation.

AI was used to make this software. See [AI_USAGE.md](AI_USAGE.md).

## Davidian code reused

- The circuit solver (`src/dsp/circuit.h`), oversampler and speaker filter shapes are adapted from the Davidian Ashgrove amplifiers.
- The GUI renderer, stroke font, house style and window layers (`src/ui/`), and the VST3 and Audio Unit wrappers, are adapted from the Davidian Bi-Lumen.

Both are Davidian projects under GPL-3.0-or-later.

## Third-party material

### LV2 interface declarations (ISC)

`src/lv2_abi.h` and the UI ABI block at the top of `src/ui/ui_lv2.c` declare the LV2 C interface with the same names and layout as the LV2 headers, so the build needs no LV2 SDK. No other LV2 code is included.

- `lv2/core/lv2.h`: Copyright 2006-2020 Steve Harris, David Robillard
- `lv2/ui/ui.h`: Copyright 2009-2016 David Robillard, Lars Luthman

> Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
>
> THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

ISC is compatible with the GPL, so the combined work is distributed under GPL-3.0-or-later.

### Steinberg VST3 SDK (MIT), in the VST3 plugin

The VST3 build compiles parts of the Steinberg VST3 SDK 3.8.1 (`base`, `pluginterfaces`, `public.sdk`) into `Davidian Treble Booster 30.vst3`. CMake downloads the SDK while configuring; it is not stored in this repository. The tests also use its hosting classes and validator.

- Copyright (c) 2026, Steinberg Media Technologies GmbH
- Licence: MIT. The full text is in [licenses/VST3-SDK-MIT.txt](licenses/VST3-SDK-MIT.txt), and the build copies it into the VST3 bundle's `Contents/Resources`.

The build leaves out the SDK's default folder icon, which is Steinberg's VST logo.

### Apple AudioUnitSDK (Apache 2.0), in the Audio Unit

The Audio Unit build compiles Apple's AudioUnitSDK 1.2.0 (the AUBase and AUEffectBase classes and their helpers, unmodified) into `Davidian Treble Booster 30.component`. CMake downloads the SDK while configuring; it is not stored in this repository.

- Copyright © 2000-2023 Apple Inc.
- Licence: Apache License 2.0. The full text is in [licenses/Apache-2.0.txt](licenses/Apache-2.0.txt), and the build copies it into the component's `Contents/Resources`. The SDK has no NOTICE file.

MIT and Apache 2.0 are both compatible with GPL version 3, so the VST3 plugin and the Audio Unit are distributed under GPL-3.0-or-later as combined works.

### Used by the build but not included

| Component | Licence | How it is used |
|---|---|---|
| Microsoft C and C++ runtimes | Microsoft Visual Studio licence | Linked statically into the Windows DLLs and the VST3 plugin (`/MT`). They are system libraries under GPL-3.0 section 1. |
| Xlib (libX11) | MIT/X11 | Linked dynamically by the Linux GUIs. |
| Cocoa, Core Graphics, AudioToolbox, AudioUnit, CoreAudio, libc++ | Apple system frameworks | Linked dynamically by the macOS GUIs and the Audio Unit. |
| GitHub Actions (actions/checkout, upload-artifact, download-artifact) | MIT | Used to build the release bundles. Not shipped. |
| lilv, from the Ardour 9 install | ISC | Loaded at run time by `test/lilv_check.c` only. Not shipped in the bundle. |
| Win32 user32/gdi32, C maths library | System libraries | Linked dynamically. |

## Circuit and reference sources

- **Circuit:** the model follows the component values and topology of a manufacturer's works drawing of a three-channel 30 watt valve amplifier, dated 29 April 1960 with modifications to 8 May 1961, as published in scan by a collectors' archive. Component values and circuit topology are facts used to write an original simulation. None of the drawing's image is included in this repository or the bundles.
- **Published figures:** the output transformer's anode-to-anode impedance (about 4 kΩ) and the valve line-up's roles come from collectors' and parts suppliers' pages. Only those figures were used, and no text or images from them are included.
- **Trademarks:** the original amplifier's brand and model names are trademarks of their owners. They are not used in the plugin's name, URIs, files, GUI or documentation, and Davidian is not affiliated with them. "Treble Booster 30" is an original model name. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units and macOS are trademarks of Apple Inc. They name the plugin formats only; Davidian is not affiliated with either company.
- **GUI design:** original, in the Davidian house style. It does not copy the original amplifier's logo, lettering, grille cloth or panel artwork.
