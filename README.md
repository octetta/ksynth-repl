# KSynth-REPL

![KSynth-REPL Logo](logo.jpg)

**Version:** 0.1.0  
**License:** MIT  
**Copyright (c)** Joseph Stewart / Octetta  

KSynth-REPL is an interactive, notebook-style graphical coding environment built around the **k-synth** array language engine. It serves as a powerful bridge between low-level digital signal processing mathematics and a live-coding musical instrument.

By separating executable code from markdown notes, and integrating a robust command system, KSynth-REPL enables you to instantly generate waveforms, visualize them, map them to MIDI keygroups, and shape their playback parameters—all in real-time.

---

## Features

- **Interactive Notebook Editor**: Mix pure `k-synth` code blocks with rich Markdown documentation.
- **Dedicated Command Cells**: Lines starting with `\` automatically format into "Meta" command cells (distinct purple style) for audio routing and playback without triggering full array evaluations.
- **Waveform Banking (128 slots)**: Store up to 128 generated waveforms in memory slots that map directly to standard MIDI note numbers.
- **Micro-Polyphony**: Built-in configurable polyphonic audio engine leveraging `miniaudio` (default 16 voices; set via Preferences or `-v`).
- **Advanced Playback Parameters**: Real-time control over semitone shifting, fine cents detuning, decibel (dB) gain staging, and sample attenuation.
- **Programmable Velocity Curves**: Generate custom arrays in `k-synth` (linear, logarithmic, inverted) and route them as Look-Up Tables (LUTs) for dynamic velocity scaling.
- **Silent Macro Bindings**: `Alt+A` to `Alt+Z` keyboard shortcuts instantly and silently play variables without cluttering your REPL console.
- **MIDI + UDP Control**: Hardware MIDI, virtual port `ksynth-repl`, UDP command port, and UDP note events.

---

## Commands

Commands are executed by typing a backslash `\` at the start of a line in the editor or terminal.

### Core Playback

| Command | Description |
|---|---|
| `\p [var]` | Play a variable immediately (mono). Example: `\p A` |
| `\pq [var]` | Play quietly — no console echo (used by Alt+A–Z macros) |
| `\ps [var]` | Play a variable in stereo mode |
| `\? [var]` | Visualize a variable with the Braille ASCII oscilloscope |

### Banking and Keygroups

| Command | Description |
|---|---|
| `\b [0-127] [var] [opts]` | Bank a wave into one MIDI slot. Starts as **one-shot**. |
| `\bm [var] [base_note]` | Bank one wave across **all** 128 MIDI notes, pitched relative to `base_note` (default 60). Starts as **one-shot**. |
| `\pb [0-127] [vel] [opts]` | Play a banked slot with optional velocity and overrides |
| `\loop <note\|all> [start end]` | Enable a sustain loop, or disable looping if start/end are omitted |

### Audio and MIDI Configuration

| Command | Description |
|---|---|
| `\mv [dB]` | Set global master volume in decibels |
| `\vc [var]` | Load a 128-element array as the global velocity curve (LUT) |
| `\mc [all\|1-16]` | MIDI channel filter (`all` / `omni` = listen to every channel) |
| `\sg`, `\ss`, `\s?` | Scope IPC start, stop, and status |
| `\ra [var] [path]` | Read an audio file into a variable |
| `\l [file.ks]` | Load a KSynth file (handled by Hazel) |

### Playback Parameters (`\b` and `\pb`)

When banking (`\b`) or playing (`\pb`), you can optionally pass parameters in order:

1. **velocity** (only for `\pb`, default `127`): MIDI velocity 1–127
2. **semis** (default `0`): Pitch offset in semitones
3. **cents** (default `0`): Fine pitch detuning
4. **gain_db** (default `0.0`): Base amplitude in dB
5. **atten** (default `1.0`): Per-sample linear decay
6. **vel_sens** (default `1.0`): Velocity sensitivity (`0.0` = fixed level, `1.0` = full range via `\vc`)

Examples:

```
\b 36 A 0 0 0.0 1.0 0.0
\pb 36 64
\mv -3.0
```

---

## One-shot vs sustained (Note-Off)

This is the rule for MIDI and UDP note events.

### Default: one-shot

After `\b` or `\bm`, banks are **one-shots**:

- Key down starts the wave
- **Note-Off is ignored** — the sample plays through to the end of the buffer

Good for drums, hits, and short tones.

```
\bm kick 36
```

### Sustained: enable a loop

To hold a sound while the key is down, set a loop region:

```
\bm pad 60
\loop all 11025 88200
```

Or for a single note:

```
\loop 60 11025 88200
```

Then:

- Key down starts the wave and wraps inside `[start, end)` while held
- **Note-Off exits the loop**

Release behavior when looped:

| Buffer layout | On Note-Off |
|---|---|
| Loop only (no samples after `end`) | Short fade-out |
| Loop plus tail (`loop_end` is less than buffer length) | Stop wrapping; play the remaining samples as a release tail |

### Disable looping (back to one-shot)

```
\loop all
```

or:

```
\loop 60
```

### Important

Re-running `\b` or `\bm` **clears** loop settings. Banks always start one-shot again. Run `\loop` again if you need sustain.

---

## Quick start: play from a MIDI keyboard

1. Create a wave in a code cell (conventional output variable `W`) and run it.
2. Map it across the keyboard:

```
\bm W 60
```

3. Connect a controller, or point **VMPK** MIDI output at the virtual port **`ksynth-repl`**.
4. Play. This is one-shot mode (Note-Off does not cut the sound).
5. Optional — held pad:

```
\loop all 11025 88200
```

---

## Network and MIDI Control

### MIDI Input (Controllers and VMPK)

On launch, `ksynth-repl` initializes the `minimidio` backend. It tries to open a hardware MIDI input and also exposes a **virtual MIDI port** named `ksynth-repl`.

If you use **VMPK (Virtual MIDI Piano Keyboard)**, set its MIDI Output connection to `ksynth-repl`.

**Mapping notes to audio**

```
/ 1. Load or generate a wave into a variable
\ra snare my_snare.wav

/ 2. Map it to MIDI note 60 (Middle C)
/ Syntax: \b [note] [var] [semis] [cents] [gain_db] [atten] [vel_sens]
\b 60 snare 0 0 0 1 1
```

Or map one wave across the whole keyboard:

```
\bm snare 60
```

**MIDI channel filtering**

By default the REPL listens in **Omni** mode (all 16 channels). Isolate a channel with `\mc`:

```
\mc 1
\mc 10
\mc all
```

### UDP Commands (port 60442)

Send raw ASCII strings to UDP port `60442`. They are evaluated as if typed in the terminal.

```
echo "\p snare" | nc -u -w0 127.0.0.1 60442
```

### UDP Events (port 60443)

Trigger voices with Skred-style 4-byte packets: `[status, channel, data1, data2]`.

Example Note On for Middle C (60) at velocity 127:

```
[ 0x90, 0x00, 0x3C, 0x7F ]
```

Note Off (or Note On with velocity 0) follows the same one-shot vs sustained rules as hardware MIDI.

---

## Command-line flags

| Flag | Meaning |
|---|---|
| `-v N` | Max voices |
| `-m NAME` | Virtual MIDI port name (default `ksynth-repl`) |
| `-p PORT` | UDP command port |
| `-e PORT` | UDP events port |
| `--scope` / `-s` | Start scope IPC as `ksynth-scope` |

---

## Future Roadmap

- Programmable microtuning maps (`\tm`)
- Array-driven envelopes (`\env`) beyond simple attenuation / loop tails
- Richer voice-stealing policies and per-bank modes beyond loop on/off

---

## License

MIT — see the repository for full terms.
