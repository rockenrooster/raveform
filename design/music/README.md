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

## Tracks so far

| File | Track | Tempo, key | Character |
|---|---|---|---|
| `track01.json` | First Light | 120 BPM, A minor, i–VI–III–VII | Warm four-on-the-floor, off-beat bass, up-down chord arps |
| `track02.json` | Solar Wind | 126 BPM, F minor, i–VII–VI–VII | Driving: bouncing octave bass, syncopated stabs, brighter and snappier lead, eighth-note delay |
| `track03.json` | Reef | 132 BPM, D dorian, i–IV vamp (Dm9 – G6) | Rolling techno, hypnotic and liquid: the bass rolls on the three sixteenths between the kicks; the lead is a three-note cell that turns against the beat and meets it again at the bar line, with a quarter-note echo; dub chord stabs into a longer room |

A Track can carry `mix`, `send` and `patch` overrides (Tracks 2 and 3 do): that is how each Track gets
its own character from the same voices. `patch.kick_hz` tunes the kick's body to the key (Track 3 keeps
A1, the fifth of D: the root an octave down is too low to hear, an octave up too high to be a kick).
All previews use the same staged arrangement, so they can be compared bar for bar.

Track 3 measured: −14.0 LUFS, peak −4.9 dBFS, crest 11.5 dB; bands sub −5.2, bass −5.3, low-mid −7.4,
mid −8.4, high-mid −12.5, high −17.2; the phone-speaker band keeps −4.2 dB, the same as Track 1. Its
lead is more resonant (Q 2.8) and so sits 1.2 dB lower in the mix. Question for the listener: does the
three-note lead stay hypnotic, or does it get stuck in the head the wrong way?

## Sound variants — `previews/sound_variants.mp3`

Three characters of the same music for the M2 listening checkpoint, each a set of overrides on
`patches.json` (`variants.json`). The file plays the same twelve bars of Track 1 in each — four bars of
full Groove with the lead, a four-bar Build, four bars of Drop — at the same loudness:

| Time | Variant | Character | Measured against Club |
|---|---|---|---|
| 0:00 | **Club** | The reference. Warm four-on-the-floor, round bass, a pluck that sits inside the mix | — |
| 0:27 | **Arcade** | Crisp and bright: snappier kick, glassy lead, shorter bass notes, brighter hats, drier room | Lead a third brighter, whole mix 15% brighter, reverb tail 2 dB lower |
| 0:55 | **Velvet** | Deep and soft: longer rounder kick, mellow lead with more body, wide pads, more echo | Lead a third darker, whole mix 16% darker, reverb tail 2.6 dB higher |

All three sit at −14.0 LUFS with peaks below −6 dBFS, and each keeps the phone-speaker band within
1 dB of the reference. Questions for the listener: which one would you want in your ears for twenty
minutes? Is Arcade's lead exciting or piercing? Is Velvet's kick still a kick on a phone speaker? A mix
of two is a fine answer ("Club with Arcade's kick").

```
python design/tools/musicvariants.py     # renders the comparison and prints the guard-rail numbers
```

Decided by the user on 2026-11-11: all three ship. The player picks one in the audio settings (Club is
the default); the change takes hold on the next bar line.

## Who plays what — `conductor.json`

The Track files hold one staged run written out bar by bar, for listening. In the game the same
patterns are switched by rules: which bed layers play at which director intensity, what each section
of the phrase cycle does to the bed, how the lead follows the fire button and the Mods, and which
instrument each Riff brings. Those rules are `conductor.json`. It is the creative brief for the audio
engine's mix state; the prototype renderer does not read it.

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

The reference parameters — mix levels, sends, sidechain depths, section gains and every patch — are
data in `patches.json`. The prototype loads them from there and the engine should too, so the two can
never drift apart. The recipes below say what the parameters drive.

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

## Sound effects — `design/tools/sfxproto.py`

```
python design/tools/sfxproto.py      # writes sfx_sheet, lead_timbres and gameplay_mock into previews/
```

Rules:

- Effects are part of the music. Kills, pickups and warp-ins are pitched to the current chord (kills
  take a chord tone: small one octave up, medium at pitch, large one octave down).
- Kills are **aggregated**: per 1/32 step, at most one sound per size class, its gain following the
  count as `1 − e^(−count/2.5)`. A thousand kills a second costs the same voices as ten.
- Shards climb the scale: each pickup within a run plays the next note up, so a streak is a rising run.
- Warp-ins swell for half a second and end exactly on the beat the enemy becomes solid.
- A Nova is two sounds: an inhale from the press to the next beat, and the blast on that beat.
- Hits and blasts duck the music (0.25–0.5 depth, 120–250 ms); nothing else does.
- In the mock the bed sits at −14 LUFS and the effects sum to about −17.5 LUFS; together −12.9 LUFS.
  Effect levels are the `LEVEL` table in the script.

`previews/sfx_sheet.mp3`:

| Time | Effect | Recipe |
|---|---|---|
| 0.3 | Kill, small (three pitches) | Sine gliding from 1.6× to the pitch in 12 ms, 45 ms decay, plus a 2–7 kHz click |
| 1.5 | Kill, medium | Sine gliding from 2× to the pitch, 90 ms decay, ring-modulated at 2.76×, plus 0.7–4.5 kHz noise |
| 2.5 | Kill, large | Sub sweep 150 → 55 Hz, a saw at the pitch through a closing low-pass, and a 0.5–6.5 kHz crash |
| 3.9 | Shard run | Sine chirping up into the note, 30 ms decay; notes A5 C6 D6 E6 G6 A6 C7 D7 |
| 5.2 | Boost picked up | Four FM bells A4 E5 A5 C6, 55 ms apart, with a high sparkle |
| 7.0 | Upgrade taken | Am9 saw chord swelling through a low-pass, with a bell arpeggio E5 G5 B5 E6 |
| 9.0 | Dash | Noise through a low-pass opening from 600 Hz to 6 kHz in 110 ms |
| 9.9 | Perfect Dash | Dash plus a bell on E6 |
| 11.4 | Hit taken | Saw falling 220 → 55 Hz through a closing low-pass and hard clip, noise burst, low thud |
| 12.7 | Nova | Rising noise and tone (inhale), then the impact with a wide noise burst |
| 16.3 | Enemy warp-in | Sine at the chord root with a cubic fade-in that stops on the beat |
| 18.0 | Telegraph tick, enemy shot | 3 kHz tick one beat ahead; 180 Hz square blip on the shot |
| 20.0 | Elite arrives | Low fifth (A2 + E3) of detuned saws through a dark filter, with a sub |
| 22.0 | Interface | Tick; confirm E6 → A6; back A6 → E6 |
| 23.7 | Shield cell restored | Soft E5 → A5 |
| 24.6 | Game over | Falling bell arpeggio A5 E5 C5 A4 E4 A3 and a low boom |

`previews/lead_timbres.mp3` — the same phrase on each player's lead: 0.3 s P1 saw pluck, 4.8 s P2 square
pluck, 9.3 s P3 bell, 13.8 s P4 wide stab.

`previews/gameplay_mock.mp3` — sixteen bars of Track 1 with a simulated run on top: 3.5 s Boost,
5.0 s Perfect Dash, 10.8 s hit, 16.0 s the Drop, 20.6 s Dash, 24.0 s elite, 27.1 s Nova pressed,
27.5 s Nova lands.

Mixing rule that fixed the first render: decide levels from the per-stem band table the prototype
prints, not from peak levels — a kick normalised to full scale buried everything else by 10–20 dB.
