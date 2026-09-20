# KSynth-REPL

![KSynth-REPL Logo](logo.jpg)

**Version:** 0.1.0  
**License:** MIT  
**Copyright (c)** Joseph Stewart / Octetta  

KSynth-REPL is an interactive, notebook-style graphical coding environment built around the **k-synth** array language engine. It serves as a powerful bridge between low-level digital signal processing mathematics and a live-coding musical instrument. 

By separating executable code from markdown notes, and integrating a robust command system, KSynth-REPL enables you to instantly generate waveforms, visualize them, map them to MIDI keygroups, and shape their playback parameters—all in real-time.

---

## 🚀 Features

- **Interactive Notebook Editor**: Mix pure `k-synth` code blocks with rich Markdown documentation. 
- **Dedicated Command Cells**: Lines starting with `\` automatically format into "Meta" command cells (distinct purple style) for audio routing and playback without triggering full array evaluations.
- **Waveform Banking (128 Slots)**: Store up to 128 generated waveforms in memory slots that map directly to standard MIDI note numbers.
- **Micro-Polyphony**: Built-in 8-voice polyphonic audio engine leveraging `miniaudio`.
- **Advanced Playback Parameters**: Real-time control over semitone shifting, fine cents detuning, decibel (dB) gain staging, and sample attenuation.
- **Programmable Velocity Curves**: Generate custom arrays in `k-synth` (linear, logarithmic, inverted) and route them as Look-Up Tables (LUTs) for dynamic velocity scaling.
- **Silent Macro Bindings**: `Alt+A` to `Alt+Z` keyboard shortcuts instantly and silently play wavetables without cluttering your REPL console.

---

## 💻 Commands

Commands are executed by typing a backslash `\` at the start of a line in the editor or terminal. 

### Core Playback

| Command | Description |
|---|---|
| `\p [var]` | Play a variable immediately (e.g., `\p A`). |
| `\pq [var]` | "Play Quiet" - Play a variable without echoing output to the REPL console. |
| `\ps [var]` | Play a variable in Stereo mode. |
| `\? [var]` | Explicitly visualize a variable using KSynth's Braille ASCII oscilloscope graph. |

### Banking & Keygroups

| Command | Description |
|---|---|
| `\b [0-127] [var] [opts]` | Bank a wave into a slot with default tuning/vol (e.g., `\b 60 A`). |
| `\pb [0-127] [vel] [opts]` | Play a banked wave slot with optional velocity and overrides. |

### Audio Configuration

| Command | Description |
|---|---|
| `\mv [dB]` | Set the global master volume in decibels. |
| `\vc [var]` | Load a 128-element array as the global velocity curve (LUT). |
| `\sg`, `\ss`, `\s?` | Scope IPC Start (`g`), Stop (`s`), and Status (`?`). |
| `\l [file.ks]` | Load a KSynth file (handled by Hazel). |

---

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

---

## 🔮 Future Roadmap (Futures)

The KSynth-REPL is constantly evolving. Upcoming architectural targets include:

- **UDP MIDI Event Listener**: Hooking up the engine to a local UDP port to receive live `Note On`/`Note Off` events, turning the REPL into a fully playable software instrument.
- **Programmable Microtuning Maps (`\tm`)**: Similar to `\vc`, allowing the user to generate a 128-element frequency/pitch scalar array to define custom, non-12-TET musical scales (e.g., Just Intonation, Bohlen-Pierce).
- **Array-driven Envelopes (`\env`)**: Replacing simple `attenuation` scalars with robust ADSR or custom envelope shapes defined entirely by arrays.
- **Polyphonic Voice Stealing**: Intelligent routing of oldest-voice-stealing when exceeding the 8-voice maximum.


## Network and MIDI Control

`ksynth-repl` is fully controllable via hardware MIDI, virtual MIDI, and UDP network sockets. 

### MIDI Input (Controllers & VMPK)

When you launch `ksynth-repl`, it automatically initializes the `minimidio` backend. It will attempt to connect to your primary hardware MIDI controller automatically. Additionally, it exposes a **Virtual MIDI Port** named `ksynth-repl`. 

If you are using a software controller like **VMPK (Virtual MIDI Piano Keyboard)**, simply open VMPK's settings and set the MIDI Output connection to `ksynth-repl`. 

**Mapping Notes to Audio:**
To trigger an audio sample via MIDI, you must assign a K-Synth array variable to a specific MIDI note (0-127) using the `` (bank) command:

```ksynth
/ 1. Load an audio file into the variable 'snare'
a snare my_snare.wav

/ 2. Map 'snare' to MIDI Note 60 (Middle C)
/ Syntax:  [note] [var_name] [semis] [cents] [gain_db] [atten] [vel_sens]
 60 snare 0 0 0 1 1
```
Now, whenever you press Middle C on your MIDI keyboard, `ksynth-repl` will instantly trigger the `snare` array. *(Note: Note Off messages are currently ignored as voices act as one-shot triggers).*

### UDP Commands (Port 60442)

You can remote-control the REPL by sending raw ASCII strings to UDP port `60442`. Anything received here is evaluated exactly as if you had typed it into the terminal.

You can test this from another terminal using `nc` (netcat):
```bash
echo "\p snare" | nc -u -w0 127.0.0.1 60442
```

### UDP Events (Port 60443)

You can trigger voices over the network using Skred-style UDP binary events on port `60443`. The server expects lightweight 4-byte packets formatted as `[status, channel, data1, data2]`.

To send a Note On (0x90) for Middle C (60) with max velocity (127):
`[ 0x90, 0x00, 0x3C, 0x7F ]`
