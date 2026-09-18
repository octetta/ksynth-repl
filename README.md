# KSynth-REPL

An interactive, notebook-style REPL environment for [KSynth](https://github.com/octetta/k-synth), a fast, array-oriented audio programming language. 

Built using FLTK, the Hazel UI framework, and Miniaudio, KSynth-REPL provides a live, interactive environment for algorithmic sound design, wave generation, and musical playback.

## Features

- **Interactive Notebook Interface:** seamlessly mix markdown notes and KSynth code blocks.
- **Batched Block Evaluation:** Execute entire blocks of audio generation code at once with automatic array-preview suppression to keep the console clean.
- **Live Polyphonic Audio Engine:** An 8-voice polyphonic mixer powers instant, latency-free playback of your generated arrays.
- **Pitch & Envelopes:** High-quality linear interpolation for pitch shifting (semitones/cents) and per-sample attenuation directly in the playback engine.
- **Waveform Banking:** Store up to 128 generated waveforms in memory slots mapping to MIDI keys for complex playback orchestration.
- **Braille Scope Viewer:** Visually inspect generated arrays in the REPL using an embedded ASCII Braille waveform grapher.
- **Silent Macros:** Press `Alt + A` through `Alt + Z` to instantly and silently play the array stored in the corresponding variable.

## Notebook Syntax

- **`//`** Starts a Note/Markdown block.
- **`/`** Continues a Note/Markdown block.
- **Normal lines** are evaluated as KSynth code.
- **`\`** Lines starting with a backslash are intercepted as REPL commands (see below) before evaluation.

## Slash Commands

| Command | Description |
|---|---|
| `\? [var]` | View the ASCII braille waveform graph of a variable (e.g., `\? A`). |
| `\p [var]` | Play the variable in Mono. |
| `\ps [var]` | Play the variable in Stereo. |
| `\pq [var]` | Quietly play the variable in Mono (no text output in the REPL). |
| `\b [0-127] [var] [opts]` | Bank a wave into a slot with default tuning/vol (e.g., `\b 60 A`). |
| `\pb [0-127] [vel] [opts]` | Play a banked wave slot with optional velocity and overrides. |
| `\mv [dB]` | Set the global master volume in decibels. |
| `\vc [var]` | Load a 128-element array as the global velocity curve (LUT). |
| `\l [file.ks]` | Load a KSynth file (handled by Hazel). |
| `\w [ms]` | Wait for N milliseconds. |
| `\s [var]` | Save the variable to a Mono WAV file (TBD). |
| `\ss [var]` | Save the variable to a Stereo WAV file (TBD). |

### Playback Parameters (`\pb` and `\b`)

When banking (`\b`) or playing (`\pb`), you can optionally define/override the following parameters in order:
1. **`velocity`** (only for `\pb`, default `127`): MIDI Velocity (1-127).
2. **`semis`** (default `0`): Pitch offset in semitones.
3. **`cents`** (default `0`): Fine pitch detuning.
4. **`gain_db`** (default `0.0`): Base amplitude in dB (e.g., `-6.0`).
5. **`atten`** (default `1.0`): Per-sample linear decay.
6. **`vel_sens`** (default `1.0`): Velocity sensitivity (0.0 = fixed volume, 1.0 = full range mapped via `\vc`).

*Example:* `\b 36 A 0 0 0.0 1.0 0.0` banks a kick drum into slot 36 with `0.0` sensitivity (always loud).
*Example:* `\pb 36 64` plays slot 36 at velocity 64. Because of the `0.0` sensitivity, it will still play at maximum volume.
*Example:* `\mv -3.0` sets the global master volume to -3dB.

## Building

Make sure you have CMake and FLTK installed, then run:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./ksynth-repl
```

## License

MIT License

Copyright (c) Joseph Stewart / Octetta

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
