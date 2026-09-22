# k-synth cheatsheet (for KSynth-REPL)

A short guide to the array language inside KSynth-REPL. Sample rate is fixed at **44100 Hz**. Lines are one expression each. `/` starts a comment.

---

## Mental model

- Everything is a **vector of floats**. A scalar is a 1-element vector.
- Evaluation is **right-associative**: `a op b op c` means `a op (b op c)`.
- Assignment: `Name: expression`
- Conventional output name: **`W`** (many examples peak-normalize into `W`)
- After you have a wave in a variable, in the REPL use:
  - `\p W` — play it
  - `\? W` — graph it
  - `Alt+A` … `Alt+Z` — play variables A–Z quietly
  - `\b 60 W` or `\bm W 60` — bank for MIDI (see main README)

---

## Literals and names

```
42
3.14
0.5
1 2 3          / vector
N: 44100
freq: 440
```

Names may be single letters `A`–`Z` or multi-letter (`freq`, `pad1`).

Negation inside expressions: prefer `0-X` rather than a bare leading minus on a name.

---

## Core generators

| Code | Meaning |
|------|---------|
| `!N` | iota `[0, 1, …, N-1]` |
| `~N` | phase ramp `0 … 2π` over N samples |
| `r V` | white noise, length of V |
| `m V` | metallic ± noise, length of V |
| `p 0` | sample rate **44100** |
| `p 2` | **2π** |
| `p2%p0` | phase increment for **1 Hz** |

Time base pattern:

```
N: 44100
T: !N
C: p2%p0
```

---

## Arithmetic and compare

| Op | Meaning |
|----|---------|
| `+ - *` | add, sub, mul |
| `%` | **division** (not modulo) |
| `^` | power `abs(A)^B` |
| `&` `\|` | min / max |
| `< > =` | compare → 0.0 or 1.0 |
| `N # V` | tile/cycle V to length N |
| `A , B` | concatenate |

Fractional part of X: `X - _(X)`.

---

## Monadic verbs (prefix)

| Verb | Meaning |
|------|---------|
| `s c t` | sin, cos, tan |
| `h d` | soft clip / harder soft clip |
| `a q l e` | abs, sqrt(abs), log, exp |
| `x V` | fast decay shape `exp(-5V)` |
| `_ V` | floor |
| `w V` | **peak-normalize** to ±1 (use for output) |
| `i V` | reverse |
| `n V` | MIDI note → Hz |
| `+V` `>V` | sum / peak (scalar) |
| `u V` | short anti-click ramp (monadic default) |
| `v V` | quantize (monadic default levels) |
| `j k` | left / right channel from interleaved stereo |

---

## Scan (`\`) — phase accumulator

| Code | Meaning |
|------|---------|
| `+\V` | running sum (phase accumulator) |
| `*\V` | running product |
| `-\ %\ &\ \|\ ^\` | other running ops |

Classic oscillator:

```
N: 44100
T: !N
C: p2%p0
P: +\(N#(440*C))
W: w s P
```

Or with phase ramp helper:

```
P: ~N
W: w s (P*(440%44100)*N)
```

(Prefer the `+\` form; it matches most patches.)

---

## Synthesis verbs

| Verb | Form | Meaning |
|------|------|---------|
| `$` | `P $ A` | weighted harmonics: `A[j]*sin(P*(j+1))` |
| `o` | `P o H` | equal-amp harmonics for indices in H |
| `t` | `Table t freq dur` | wavetable oscillator (freq Hz, dur samples) |
| `b` | `freq b V` | pitched buzz, length of V |
| `y` | `d g y signal` | feedback delay (d samples, g feedback) |
| `f` | `ct f signal` | 2-pole lowpass (coeff ~0–0.95) |
| `g` | `hz g signal` | 2-pole lowpass in Hz |
| `z` | `L z R` | interleave stereo |

Highpass / bandpass from lowpass:

```
H: R - (0.1 f R)
B: (0.4 f R) - (0.05 f R)
```

---

## Envelopes (common recipes)

```
/ exponential decay (~-60 dB by end when k≈6.9)
E: e(T*(0-6.9%N))

/ percussive rise-then-fall
X: T*e(T*(0-8%N))

/ short onset ramp (dyadic u)
On: 100 u T
W: w On * s P
```

---

## Right-associativity traps

```
/ mix of two sines — need parens
W: w (s P)+(s Q)     / correct
W: w s P + s Q       / becomes sin(P + sin(Q)) — FM, not a mix

/ linear mix
W: w (S*0.3)+(U*0.7) / correct
W: w S*0.3+U*0.7     / wrong grouping

/ hard clip
C: (S & 0.5) | 0-0.5 / clip to [-0.5, 0.5]
```

---

## Functions

```
addOne: { x+1 }
addOne 3

mix: { x+y }
2 mix 3
```

Inside `{...}`, `x` and `y` are the arguments.

---

## Mini tutorial

### 1. Sine tone (0.1 s)

```
N: 4410
T: !N
C: p2%p0
P: +\(N#(440*C))
W: w s P
```

Then: `\p W`

### 2. Decaying sine

```
N: 44100
T: !N
C: p2%p0
P: +\(N#(220*C))
E: e(T*(0-6.9%N))
W: w E * s P
```

### 3. FM bell

```
N: 88200
T: !N
C: p2%p0
A: e(T*(0-3%N))
I: 3.5*e(T*(0-40%N))
P: +\(N#(440*C))
Q: +\(N#(440*C))
W: w A*(s P+(I*s Q))
```

### 4. Filtered noise

```
N: 44100
T: !N
R: r T
W: w 0.1 f R
```

### 5. Additive saw-ish

```
N: 44100
T: !N
C: p2%p0
P: +\(N#(110*C))
A: 1 0.5 0.333 0.25 0.2 0.166
W: w P $ A
```

### 6. Bank for MIDI and play

```
/ after any of the above set W
\bm W 60
```

One-shot by default. For a held pad:

```
\loop all 11025 88200
```

(See the main README for one-shot vs sustained Note-Off rules.)

---

## Word aliases (optional)

Many single-letter verbs also have names, e.g. `sin`, `cos`, `norm` (`w`), `idx` (`!`), `phase` (`~`), `sum` (`+` monadic).

```
W: norm(sin(P))
```

---

## REPL-oriented tips

- Prefer **multi-letter names** for intermediates; keep `W` as the buffer you play/bank.
- `\? var` to inspect shape before banking.
- Long buffers + polyphony: leave headroom (`\mv -12` is the engine default headroom idea).
- Comments are `/` only — do not use `//` inside pure k-synth lines unless your notebook mode treats them as notes.

For the full language reference, see the [k-synth](https://github.com/octetta/k-synth) repo (`ksynth_reference.md`).
