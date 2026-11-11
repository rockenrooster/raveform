# RAVEFORM — Music Design

Composition and sound design for the generative soundtrack. A Track is data (`track01.json`); the
prototype renderer (`design/tools/musicproto.py`) turns it into audio with the same synthesis recipes the
engine will use, so the music can be judged by ear before the audio engine exists (milestone M2).

```
python design/tools/musicproto.py design/music/track01.json design/music/previews/track01_cycle.wav
```

## Listening guide — `previews/track01_cycle.mp3` (Track 1 "First Light", 120 BPM, A minor)

One Short phrase cycle, staged to show the music being built by play:

| Time | Section | What you hear | What it stands for in the game |
|---|---|---|---|
| 0:00 | Groove | Kick, off-beat hats, bass | The bed. Nobody is firing |
| 0:04 | Groove | + the lead arpeggio | The player holds fire — the gun is the melody |
| 0:08 | Groove | + clap, off-beat chord stabs | Intensity rises; first Riff (Sentinel) picked up |
| 0:12 | Groove | + thumps, shaker, pad | Two more Riffs (Pulse Ring, Orbitals) |
| 0:16 | Build | Snare roll accelerating, noise riser, bass thins out, last bar empties | The swarm gathers |
| 0:24 | Drop | Impact, full kit, rolling bass, lead at double time | Surge |
| 0:32 | Drop | Lead back to normal speed | Surge over |
| 0:40 | Break | Pad, sparse echoing lead, soft hats | Upgrade choice |

Questions for the listener (the composer cannot hear this):

1. Does the groove make you nod within the first four seconds?
2. Is the lead pleasant for minutes at a time, or does it grate? Too bright, too dull, too loud?
3. Does the Drop feel bigger than the Groove? Is the gap before it long enough?
4. Kick and bass on a phone speaker: present, or missing?
5. Anything harsh, muddy or annoying?

## Measured (v1)

Integrated loudness −14.0 LUFS (the bed is kept there to leave headroom for effects), peak −5.2 dBFS,
crest 11 dB. Energy by band, dB of total: sub −5.4, bass −5.1, low-mid −6.2, mid −9.7, high-mid −13.6,
high −18.1. The 280 Hz – 9 kHz band a phone speaker reproduces keeps −4.2 dB of the mix.

## Track data format

`chords`: per chord name the bass root, the pad voicing, the stab triad and a 16-step arpeggio, all as
MIDI notes. `patterns`: 16 steps per bar, `.` is a rest and digits are velocity ninths. `bass`: lists of
`[step, semitones from the chord's bass note, velocity, length in steps]`. `arrangement`: sections in
order with the layers that play; a number is a level, a string selects a variant (`bass: "drop"`,
`lead: "double" | "sparse"`). In the game the director decides layers from intensity and the players'
loadouts; the arrangement here is one plausible run written out.

## Recipes for the engine port

The reference parameters are the `PATCH`, `MIX`, `SEND`, `DUCK` and `SECTION_GAIN` tables at the top of
`musicproto.py`. Move them into shared data when the engine's audio module is written.

| Voice | Recipe |
|---|---|
| Kick | Sine with pitch `55 + 240·e^(−t/18 ms) + 30·e^(−t/100 ms)` Hz, amplitude `e^(−t/115 ms)`, 1.5 ms attack; plus a 1.8–6 kHz noise click decaying in 5 ms; `tanh(1.8x)`. The high starting pitch is deliberate: it is the part a phone speaker reproduces |
| Clap | 750–3600 Hz noise; three 6 ms bursts 10 ms apart, then an 85 ms tail |
| Snare (rolls) | Sine 325 → 185 Hz decaying in 70 ms plus 1.8–8.5 kHz noise decaying in 90 ms |
| Hats | Six square waves at 3136, 4410, 5274, 6211, 7102, 8372 Hz high-passed at 6.5 kHz, mixed 60/40 with 7 kHz high-passed noise; decay 30 ms closed, 170 ms open |
| Bass | Saw + square (50/50) → state-variable low-pass, cutoff `360 + 2040·e^(−t/90 ms)` Hz, **Q 0.8** → `tanh(1.5x)`; sub sine one octave down at 0.35. Resonance stays low so the closed filter does not ring |
| Lead (P1) | Two saws 7 cents apart + a quiet square one octave down → low-pass, cutoff `750 + 3450·e^(−t/70 ms)` scaled by `(f/440)^0.5`, Q 2.2; amplitude 2 ms attack, 160 ms decay. Dotted-eighth ping-pong delay, feedback 0.36 |
| Stab | Triad of detuned saw pairs → low-pass 3600 → 900 Hz in 55 ms, Q 1.5; 110 ms decay |
| Pad | Four notes, each a detuned saw pair, mirrored detune per channel → low-pass 950–1600 Hz moved by a 0.125 Hz LFO, Q 0.9; 350 ms attack, 700 ms release; high-passed at 220 Hz |
| Pulse (Riff) | Sine sweeping 820 → 170 Hz in 45 ms, 130 ms decay, plus a short 1.5–5 kHz noise tick |
| Riser | Twelve noise bands from 300 Hz to 9 kHz fading in one after another over the Build |
| Impact | Sine 120 → 38 Hz, 700 ms decay, plus 140–650 Hz noise boom and a 3.5–11 kHz crash |

Oscillators are band-limited (PolyBLEP). The filter is the zero-delay state-variable form (Simper),
which stays stable under per-sample cutoff modulation. Sidechain: every kick sets a duck envelope
`e^(−t/110 ms)`; depth per bus is in `DUCK`. Reverb: 1.4 s decay, 15 ms pre-delay, band-limited
320 Hz – 7 kHz. Master: 30 Hz high-pass, per-section bed gain, `tanh(1.25x)`.

Mixing rule that fixed the first render: decide levels from the per-stem band table the prototype
prints, not from peak levels — a kick normalised to full scale buried everything else by 10–20 dB.
