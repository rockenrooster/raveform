# RAVEFORM — Modes: the sound

> **A plan (2026-11-15), not built.** The rules of `design/music/README.md` hold for all of it: effects are
> part of the music and in the Track's key; whatever a kill, a hit or a bounce sets off is aggregated on
> the thirty-second step; nothing but hits and blasts ducks the music; an upgrade is heard within a bar.
> Every sound here is given in the words of the engine's recipes (`src/audio/cues.h`, `Recipe`, and
> `Band`'s calls). **A sound those cannot express is reported to the lead, not improvised**; the four
> things the band does not have yet are listed at the end.

Notes are named against the chord, as the fights' pages name them: `chordNote(steps, octave)` is so
many scale steps above the bar's chord root in that octave; "the home chord" is the first chord of the
Track's progression; "the title's eight" are A3 C4 E4 A4 C5 E5 G5 A5 (the notes the title's word is
played with), moved to the Track's key outside First Light: the home chord's root, third and fifth
over two octaves, then the seventh and the root.

---

## The two bonus Tracks

`music/track11.json` (Mirrorball) and `music/track12.json` (Toybox): `README.md` 11 says what they are
and why. Measured with the prototype renderer by `design/tools/bonus_tracks.py` (2026-11-15), against
the range of Tracks 1 to 3 as `design/tools/setlist.py` measures the set:

| Track | LUFS | Peak dBFS | Crest dB | Below 250 Hz | 250 Hz-2.5 kHz | 2.5-6 kHz | 6-16 kHz | Phone band | Lead centroid Hz | Flags |
|---|---|---|---|---|---|---|---|---|---|---|
| The accepted range (Tracks 1-3) | -14.0 | | | -2.3 to -1.8 | | -14.0 to -12.5 | -18.5 to -17.3 | -4.9 to -4.2 | 1,059 to 1,581 | |
| 11 Mirrorball | -14.0 | -5.1 | 11.3 | -2.5 | -4.3 | -13.4 | -17.7 | -3.8 | 1,151 | none |
| 12 Toybox | -14.0 | -3.1 | 13.6 | -3.4 | -3.3 | -13.0 | -16.5 | -2.8 | 1,253 | none |

Every note of both is in its scale. Toybox's rule holds (nothing above the bass but C D E G A). The
auto-aim's line (every second note of each bar's arpeggio, which is what a player who never aims hears)
is a tune in both: Mirrorball `G G B G G A B D`, `E G A G E F# A C#`, `F# F# A F# F# E A C#`, `F# A B A F#
A D F#`; Toybox `E C G G E A A E`, `C A E E C G G C`, `C E E C C G G D`, `C G G C C G A D`.

**Numbers say a Track is balanced like the ones that were liked; they cannot say it is liked.** To
listen (rendered by the prototype, as the set's previews were):

| File | What |
|---|---|
| `music/previews/track11_cycle.mp3` | Mirrorball's staged cycle: the bed, then the lead, the Riffs' layers, the Build, the Drop, the Break |
| `music/previews/track12_cycle.mp3` | Toybox's |
| `music/previews/bonus_sampler.mp3` | Six bars of each: the end of the Build and the start of the Drop (Mirrorball at 0:00, Toybox at 0:12) |

Questions for the listener: does Mirrorball make the foot move in four seconds, and is its bass a walk
or a wobble? Is Toybox sweet or twee after two minutes? Is its kick still a kick on a phone speaker?
Toybox's peak sits two decibels above the set's and its crest two above: its soft kick leaves the
bells on top. If the bells are sharp, their level comes down (`mix.stab`, `mix.lead`) before the kick
goes up.

For the engine: each needs its loudness calibration (`scripts\calibrate-audio.ps1`), its riser baked
with the others, and a row in the tempo and name tables (`docs/MODES_PLAN.md`). `kDspVersion` does not
change: no DSP does.

---

## Every mode

| Moment | Sound |
|---|---|
| **The load-in**, each icon landing | `Recipe::Chirp` at `chordNote(n, 5)`, n the landing's number, wrapping back to the middle after two octaves; aggregated a sixteenth (one voice, its gain `1 - e^(-count / 2.5)`); panned by where the icon came from |
| A Riff landing | That Riff's own first note, in its own voice (the band plays it from the loadout as it changes: nothing to add) |
| The load-in's last landing | The "upgrade taken" chord (`Sfx`), on bar 3's downbeat |
| The bed during the load-in | The game posts the intensity: kick and hats at the start, one more bed layer every four landings, the full Groove on bar 3 |
| **A mode's name card** | The band's stab on the home chord at 1.0 as the name lands, and a `Sweep` (opening, one beat) into it |
| **`LAST CALL`** | Two `Bell`s, the home chord's fifth then its root an octave up, an eighth apart |
| **The last bar** | `Roll` over four beats and the Build's riser, both ending on the last downbeat |
| **The last downbeat** | `Impact`, with the music bus low-passed at 600 Hz for the beat of half time and opened over the next (`lowPassBed`) |
| **The knock-out** | The hit's own sound; then `SawGlide` falling an octave over one beat. Coming back: `Sweep` (opening, one beat) ending on the bar line, and the Cell's note |
| `BACK IN 2`, `BACK IN 1` | The count-in's tick, on those bar lines |
| **A new best** | As built |

## ONE TRACK

| Moment | Sound |
|---|---|
| The outro's first downbeat | `Gong` on the home chord's root, under the boss's own death |
| The outro | The band's Break with the home chord held (the game holds the progression on its first chord: see "What the band needs", 3), the lead off |
| `TRACK COMPLETE` | The "upgrade taken" chord an octave up, as the card finishes writing |
| Into the result | `RevCymbal`, four beats, ending on the outro's last bar line |

## HEADLINERS

| Moment | Sound |
|---|---|
| The act's card | `EliteGated`'s plain form: the elite's arrival fifth in the new Track's key; then one `Bell` for each act already won, a sixteenth apart, `chordNote(k, 5)` for the k-th: the bill read out |
| The top-up | The load-in's chirps |
| `ACT 3: P2` (versus) | `Bell` on the home chord's root two octaves up, and that player's lead on the root (`leadNote`, degree 2) |
| `HEADLINERS COMPLETE` | `Gong`; the band's stab on Last Light's home chord at 1.2; the title's eight in the lead, a sixteenth apart |

## FULL BLAST

| Moment | Sound |
|---|---|
| The torrent | The load-in's chirps: three and four a sixteenth, aggregated, so two bars are one climbing run |
| A wave's name | `RevCymbal` (four beats) ending on the Drop's downbeat; the band's stab on the bar's chord as the letters are struck |
| The power cut | The music bus low-passed at 600 Hz while the ship is out (`setMuffle`), opened over one beat as it returns, with `Impact` as a one-shot on that bar line |
| The load-out (the count) | The load-in's chirps running down the scale, a sixteenth each, into the back chord |

## TRACERS

| Moment | Sound |
|---|---|
| **The tracer** | The player's lead on the Track's arpeggio, a note a sixteenth while the line is laid: the lead rule with fire held (`setFireMode(1)`) while the ship is faster than 120 wu/s, off (`0`) when it is not. In co-op, each teammate's lead by distance as in the set |
| LONG (the gel) | The lead's notes ring twice as long: "What the band needs", 2 |
| The closing hint filling (within the snap) | Nothing: the lock speaks |
| **A lock** | `Glass` at `chordNote(0, 7)`, at once (not placed on a step: a lock is the player's own act, R1) |
| A caught body | Nothing of its own (the lock's glass is one sound for all it caught) |
| **A fizzle** | `Woodblock`, unpitched |
| **A pop**, on its eighth line | The kills, aggregated as always, under the ring's chord: |
| x 1 | `Tom` on the chord's root |
| x 2 | `Stab` with the chord's triad, at 0.6 |
| x 3 | `Stab` with the chord (the bar's, as written), at 0.8 |
| x 5 | The same at 1.0, with `Tom` on the root an octave down |
| x 8 | The same at 1.2, with `SubDrop` and a `Crack` |
| ECHO (the gel) | The ring's chord again a beat later, an octave up, at half its level |
| A duet ring | Both players' leads on the chord's root and third (`leadNote` degree 0, and the third: "What the band needs", 1) under the ring's chord at one step larger |
| A tile lighting | Nothing |
| **FULL HOUSE** | `TomRoll` over the chase's two beats, rising an octave, into `Impact` and `Gong` on the bar line; the band's stab on the home chord at 1.2 |
| The floor going dark after it | `Sweep` (closing, four beats) |
| **The lights go out** (a knock-out) | Two `Glass` ticks falling a fifth, on the eighths the tiles blink on |
| A cut (versus) | `Crack` at the cut, panned to it |
| Spun out (versus) | `DashReverse` |
| **The count** | `Chirp` for each column of tiles the line passes that holds a lit tile, `chordNote(c / 3, 5)` climbing; in versus one voice a colour, each player's in their own lead's octave; then the "upgrade taken" chord on the bar line |

## SONGLINE

| Moment | Sound |
|---|---|
| **The Spark** | The band's lead, always playing, in the bell voice (timbre 2, P3's): a slot the game adds to the mix beside the players' (`setPlayers`, `setPlayerTimbre`, `setPlayerFire`), its grid the section's: eighths in a Groove and a Build (every second step **from the bar's first**: the beat's half of the grid, where a Track's tune is; the assist's mode is the other half), sixteenths in a Drop (mode 1), quarters in a Break ("What the band needs", 4). At the players' own level, not the teammates' quieter one: it is the tune. With a third player in the room the bell is also theirs: their gates are told from the Spark's by their interval in co-op and by sounding after it in versus. If the owner's ear says that is not enough, the Spark takes the pad's patch with a short release instead |
| The Build's last bar | The Spark's slot off; `PadVoice` held on the highest note of the bar's arpeggio (`NoteFrom::ArpRank`, its last rank), swelling, released on the Drop's downbeat |
| **A gate threaded** | Its note in the player's own lead voice, on the next thirty-second: "What the band needs", 1. In time: a third above it (two scale steps). In co-op each seat's own interval (0, 2, 4, 7 scale steps) |
| A gate missed | Nothing |
| A step down of the streak | `Glass` at `chordNote(0, 5)`, soft (0.3) |
| A tier of the streak | The "Boost picked up" bells, one more of the four at each tier (one at 16, all four at 128) |
| **The gun** | No lead notes (the lead rule is off for the players' slots; the sim's own grid fires the shots, as it does without a band). Each volley: `Hat` at 0.3 |
| A gold gate | The Boost bells, as any Boost |
| A chord (co-op) | The band's stab on the bar's chord at 0.8 |
| The line bending back to a lost player | `SawGlide` rising a fifth over two beats, panned to where the Spark is |
| **The finale** | `TomRoll` over the spiral's two bars, rising an octave; on the burst `Impact` and `Gong`; eight `Bell`s up the home chord, a thirty-second apart, one a ring of the firework |
| **The count** | `Chirp` for every sixteen gates threaded, climbing; a `Bell` on top for each tier the best streak reached |

## JAM

| Moment | Sound |
|---|---|
| A mixer pad flown over | The interface tick at once; on the bar line the layer joins or leaves under a `Sweep` (opening for on, closing for off, one beat ending on the line) |
| The mixer | "What the band needs", 5: the bed's layers held by a mask |
| A hold pad filling | The Trio's hold: two rising notes of the player's lead (root, fifth), then the confirm chord |
| **DROP** | The band's own Build and Drop (`setPhrase`), as the set plays them, with every bed layer on for the Drop |
| DROP ready again | One `Bell` on the home chord's fifth |
| A shelf opening | `Sweep` (opening, half a beat) and the Trio's chime as its orbs light |
| A toy taken | The "upgrade taken" chord; each further stack a `Bell` a scale step higher |
| A fifth Riff sending the oldest home | The back chord, soft |
| EMPTY | The back chord and `Sweep` (closing, one beat) |
| SOUND | The settings' own: the whole band changes on the bar line |
| A Track pad | The band's Track-change Break |
| POP-OFF's count | A `Chirp` a ten pops for each colour, side by side, climbing |

## The screens

| Moment | Sound |
|---|---|
| The mode pill tapped | The confirm chord |
| The rule lifting | `Sweep` (opening, one beat) |
| `MODES` played onto the rule | The lead: the first five of the title's eight, a sixteenth apart |
| The cards thrown down | The centre card's note, then each side's: the next three of the eight |
| Another mode to the centre | The interface tick as the row starts to slide; that card's note as it is struck (card k of the line-up: note k of the eight, so the row is a scale) |
| A Track chip tapped | The interface tick; on the bar line the band's change with its crash |
| TEAM / VERSUS, the help switch | The interface tick |
| BACK | The back chord |
| PLAY | The drop-in's own |
| The lobby's mode changing (every device) | `Glass` at `chordNote(0, 6)` |
| **The podium's table** | A `Tick` for each step of each row's number, each row at its player's pitch: the home chord's root, third, fifth and octave by seat |
| **The winner named** | The crown's landing: `Bell` on the home chord's root two octaves up with `Impact` at half level. Then the fanfare: the winner's own lead playing the title's eight, a sixteenth apart, over the band's stab on the home chord at 1.2 |
| A draw | The stab alone |
| The crown changing hands in a run | `Bell` on the bar's chord fifth, an octave up, at 0.5 |

Haptics, by the HAPTICS setting and the set's limits (one call an event, rate-limited): a light tick
for a gate threaded and for a node's note (at most one in 80 ms); medium for a lock, a pad's hold
closing and a toy taken; heavy for a pop of x 5 and more, FULL HOUSE, a wave's Drop, the finale's
burst and the winner's crown.

---

## What the band needs

Five small things the engine does not have. Each is a call on `Band` and a command to the audio thread,
on the same contract as the rest (nothing in the callback allocates, locks or logs).

| # | Need | For | The call |
|---|---|---|---|
| 1 | **A player's lead at a given note**: `leadNote` takes a degree of the root (root, fifth, octave); the modes need any note, named as the recipes name notes | A gate's note in the player's voice; a duet ring's third | `leadNoteAt(player, midi or chordNote, gain, pan, tick, timbre)` |
| 2 | **The lead's length, scaled for a player** for some bars | TRACERS' LONG gel | A field of the player's loadout: `leadLength` (1 as built, 2 under LONG) |
| 3 | **Hold the progression on one chord** from a bar line until released | ONE TRACK's outro; `HEADLINERS COMPLETE`; the result's bed | `holdChord(chordIndex or -1)` |
| 4 | **A lead on the beat's eighths and on quarters** (fire modes of every second step from the bar's first, and of every fourth; today's half-rate mode is the assist's, on the off-beat's steps) and **a slot that is nobody's** | SONGLINE's Spark in a Groove, a Build and a Break; the Spark as a voice of its own beside four players (a fifth slot, or the Spark takes the slot of a seat nobody sits in: with four players it needs the fifth) | `setFireMode(3)` and `(4)`; `kMaxPlayers` + 1 lead slots in the engine's mix |
| 5 | **The bed's layers by a mask** instead of by intensity | JAM's mixer; POP-OFF's and JAM's Drop (every layer) | `setBedMask(bits)`, `-1`: by intensity as built; applies from the next bar line |

Also: the Track tables grow from ten to twelve (`audio::kMaxTracks`), and the audio thread's load with
a fifth lead slot is measured once on the Moto with the band's fullest arrangement (`--band full`),
since the contract's audio budget was measured with four.
