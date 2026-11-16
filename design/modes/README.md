# RAVEFORM — Modes

**Status: a plan (2026-11-15). Nothing here is built.** The owner asked for more modes: "visual and audio
spectacles ... fun, creativity, and satisfaction", one of them "playing a specific track 1-10", new music
where a mode needs it, co-op and versus for each, and menus that carry them. This folder is the design; the
engineer's plan (code map, order of work, checks) is `docs/MODES_PLAN.md`.

| File | What it holds |
|---|---|
| `README.md` (this) | The line-up, the rules every mode keeps, the Stage, the kit, how a timed run ends, versus, records, the two bonus Tracks |
| `one_track.md`, `headliners.md`, `full_blast.md`, `tracers.md`, `songline.md`, `jam.md` | One page a mode: rules, numbers, look, sound, HUD, co-op, versus, the result |
| `screens.md` | The MODES screen, the title's mode pill, the lobby, the HUD's goal line, name cards, results, the podium, every transition |
| `sound.md` | Every new sound in the words of the engine's recipes; what the band needs that it does not have |
| `modes.json` | The numbers, to be embedded and read like every other design file (never retyped) |
| `music/track11.json`, `track12.json` | The two bonus Tracks, in the set list's format (`design/tools/bonus_tracks.py` renders and measures them); `music/previews/*.mp3` to listen to |
| `design/previews/modes_screen.png`, `modes_tracers.png`, `modes_songline.png` | Layout references drawn by `design/tools/modes.py`: the MODES screen, TRACERS on the Stage, SONGLINE (each with a `.small.png`) |

Units: world units (wu) for the world, reference pixels at 2400 x 1080 for screens, bars and beats for time.

---

## 1. The line-up

Seven modes. The first is the game as it is; six are new. **Nothing is locked**: every mode and every Track
can be played from the first launch (PLAN 2, "no unlock gating").

| # | Mode | In one line | What the player does | Length | Track | Players |
|---|---|---|---|---|---|---|
| 0 | **THE SET** | All ten Tracks, then the encore | The game as built | 20 minutes and on | all ten | 1-4, team or versus |
| 1 | **ONE TRACK** | Pick a Track and beat its boss | One Track of the set, whole, with the build a run would have by then | 2 to 3 minutes | 1-10 | 1-4, team or versus |
| 2 | **HEADLINERS** | The ten bosses, back to back | Boss after boss, each on its own music, the build topped up between acts | 10 to 12 minutes | all ten | 1-4, team or versus |
| 3 | **FULL BLAST** | Every power on, about three minutes | Four long Drops with every Mod at its cap, four Riffs at level 5 and Boosts raining | 84 bars: 2:10 to 3:20 | 1-12 | 1-4, team or versus |
| 4 | **TRACERS** | Draw rings of light. No aiming | The ship leaves a line of light; close a ring and everything inside pops and the dance floor inside lights up | 48 bars: 1:15 to 1:55 | 1-12 | 1-4, team or versus |
| 5 | **SONGLINE** | Fly along the tune | A spark flies the melody through the arena and leaves each note behind as a ring; fly with it and thread them | 48 bars: 1:15 to 1:55 | 1-12 | 1-4, team or versus |
| 6 | **JAM** | Nothing hurts. Every toy is yours | A play room: switch the band's layers on by flying over them, take any upgrade, press the big button for a Drop | as long as you like | 1-12 | 1-4, team; versus is POP-OFF |

Why these six:

- **ONE TRACK, HEADLINERS and FULL BLAST** are the game's own best parts in portions: a favourite Track,
  the fights, the peak. They cost little to build and prove the frame the other three need.
- **TRACERS, SONGLINE and JAM** are new ways to play, each built on the first pillar: sound becomes
  light. In TRACERS the line you draw is the melody. In SONGLINE the melody is a thing in the world you
  fly beside. In JAM each layer of the band you switch on lights one more part of the room.
- All three new ways are **one-thumb games**: no aiming, nothing that asks for a second hand. They are
  for the youngest player in the room first (the owner, 2026-11-15: "playable by 5-6 year olds").

This changes two of PLAN 2's locked decisions, on the owner's request of 2026-11-15: "one Endless mode"
becomes seven modes, and "LAN co-op" becomes LAN co-op and versus. Every other locked decision holds:
the engine, the targets, the performance contract, run-only permanence, the name.

---

## 2. Rules every mode keeps

1. **Playing in five seconds.** PLAY on the title starts the mode last chosen at once (THE SET until
   one is chosen). Choosing a mode is one screen (`screens.md`, MODES), never a tree.
2. **The music never stops** and everything lands on it: a mode starts, changes and ends on bar lines,
   lengths are in bars, and its sounds are in the Track's key.
3. **Easy.** Every mode can be finished with one thumb and the assist (GAME_DESIGN 7.6, the difficulty
   bar). The three new ways cannot be lost at all: they end when their music ends, and what skill
   changes is the score.
4. **Seen and heard within a bar.** Whatever a mode adds to the world (a tracer, a ring, a gate, a pad)
   is drawn with the game's light and has its own sound (look.md, GAME_DESIGN 6.6).
5. **The look's rules hold.** Light on black; nothing filled; the danger colour and the hollow diamond
   are for what hurts and nothing else; gold is a gift; a player's things are in that player's colour;
   an area looks like what it does (look.md 5, "What an area means"). Reduce flashing holds for every
   new flash.
6. **Inside the caps.** No mode raises a pool (TECH_SPEC 7.1). What a mode adds has its own small
   fixed arrays (its page says how many), is part of the world's state and of its hash, and allocates
   nothing after `World::init`.
7. **The contract.** The Moto holds 60 FPS P95 in each mode's densest moment (PLAN 5); each page names
   that moment and `docs/MODES_PLAN.md` its benchmark.
8. **Co-op is the same game on every device.** What a mode adds is either computed from the song's
   clock and the run's seed (nothing on the wire), owned by the player it belongs to and told as that
   player's event, or decided by the host. Each page says which.
9. **Versus is friendly.** No shot, Nova or Riff of one player ever hurts another. Rivals race, shove
   and take each other's ground; nobody is put out of a match (section 7).
10. **Run-only.** Nothing gained in a mode outlasts the run. Only records persist (section 9).

---

## 3. A mode and a Track

A mode says what you do; a Track says what plays: its music, tempo, palette and crowd. THE SET and
HEADLINERS play all ten in order. Every other mode takes one Track, chosen on the MODES screen:

| Mode | Tracks it takes | Home (chosen until the player picks another) | Why |
|---|---|---|---|
| ONE TRACK | 1-10 (a bonus Track has no boss) | 1 First Light | The start of the set |
| FULL BLAST | 1-12 | 10 Last Light | The fastest: the most shots a second |
| TRACERS | 1-12 | 11 Mirrorball | The dance floor |
| SONGLINE | 1-12 | 4 Aurora | A tune under a held note; ribbons of light in a night sky |
| JAM | 1-12 | 12 Toybox | Written for layers switched on in any order |

The mode's last Track is remembered (section 9). What a Track gives a mode:

- **Music, tempo, key, palette**: as in the set. The run's name card names both (`screens.md`).
- **The crowd**: that Track's roster, threat budget and crowd target from `design/species.json`, scaled
  as the mode's page says. A bonus Track borrows a set Track's (`modes.json` `bonusTracks[].crowdOf`:
  Mirrorball Track 3's, Toybox Track 1's).
- **Enemy bullets by Track** (GAME_DESIGN 8) and `tuning::kTrackEnemyScale` hold as in the set.
- A faster Track is a harder one in every timed mode, because lengths are in bars: less time, a
  shorter tracer, gates closer together. The MODES screen says the tempo beside the Track's name.

---

## 4. The Stage

TRACERS and JAM are played on **the Stage**: one room, the whole of it on every screen.

| | |
|---|---|
| Size | **1,720 x 840 wu**, centred on the arena's origin, bounded by the arena's glowing walls moved in: ships, bodies and bouncing shots meet them as they meet the arena's |
| Camera | Fixed on the Stage's centre. Zoom: the largest at which the Stage and 40 wu round it fit the screen's view (about 0.78 on a 20:9 phone, 0.75 on the 16:10 tablet), never under 0.75 (the lowest zoom the contract was measured at, GAME_DESIGN 9). No look-ahead, no lean, no breathing; the kick's 0.5% punch and the shake stay |
| The floor | The lattice, as everywhere. In TRACERS it is also a dance floor of **tiles 40 wu square: 43 x 21 = 903** (`tracers.md`) |
| The rig | Along the top wall, eight small fixtures 190 wu apart (a short bar and two ticks each, in the grid's colour at 0.8). They are where the Stage's beams come from (`jam.md`, `tracers.md`); with no beam lit they are furniture |
| Spawning | Bodies warp in on the Stage (R4: two beats, their ring) at least 260 wu from every ship; never under the HUD's score block, phrase bar or Nova stock. No edge indicators: nothing is off screen |
| The HUD | As in the set. On a phone the Stage leaves about 200 px clear at each side, where the sticks' rings rest |
| Co-op | Every device shows the same room, so every player sees every other: other players' ships, shots and tracers are drawn as look.md 6 "Co-op" has them (0.7 of the ship's level, 0.6 of the shot's) |

Why one room: a floor to fill and a room to play in must be seen whole, and in versus everyone must see
the same thing. The other four modes keep the arena and the follow camera as built.

---

## 5. The kit and the load-in

A run that starts in the middle of the set (ONE TRACK, each act of HEADLINERS) starts with **the kit**:
the build a run of THE SET would have by then, so the Track and its boss meet what they were tuned
against (GAME_DESIGN 7.6: a boss's health is tuned "with the build a player has by then").

- **How many.** `modes.json` `kit.atTrackStart[track]` and `kit.atBoss[track]`: the upgrades (Trios
  taken) the assist bot holds as that Track's first bar begins and as its boss enters, the median of
  20 runs of THE SET. The numbers in the file are estimates from GAME_DESIGN 6.1's level pace; the
  engineer measures them (`--sim crowd --policy assist` prints the level and the upgrades by Track)
  and writes what was measured, with the date.
- **Which.** Dealt by the game's own rules (GAME_DESIGN 6.1: the run's first Trio offers Split and
  Double-Time, the first orb is firepower, the weights, the Fusions when both halves are maxed) and
  picked by the bot's score (`kBotUpgradeScore`), from the run's seed. So every run's kit is a build
  the game could have dealt, no two runs' the same, and none is a trap (6.6).
- **In co-op** each player has their own kit, drawn from the seed and their seat.
- **Cells and Novas.** The kit's Extra Cells and Nova Stock count; the run starts with every cell and
  three Novas plus the stock's.

**The load-in** is how the kit arrives, and it is a show: the band walks on.

| When | What is seen | What is heard |
|---|---|---|
| The run's first downbeat | The ship draws itself (the drop-in, `design/screens.md` "Transitions") with nothing on it | The Track's bed: kick and hats only |
| Bars 1 and 2 | The kit's icons appear on a circle of 300 wu round the ship, in the order they were dealt, and fly in on a quarter turn of spiral, one landing on each sixteenth (two a sixteenth past 32). Each is its orb's icon at half size with a thin trail in the player's colour. Landing: eight sparks in the icon's two colours, the icon flies on to the HUD's band row, and what it gives appears at once: a lane, an arc of the halo, a Riff's ring | Each landing is the Shard's chirp one note further up the Track's scale (a climbing run, two octaves, then again from the middle); a Riff's landing is that Riff's first note in its own voice. The bed gains a layer every four landings |
| Bar 3's downbeat | A ring in the player's colours leaves the ship (90 to 400 wu in half a beat); the HUD's band row flashes once | The "upgrade taken" chord, and the Track's full Groove |

Nothing hostile comes within 600 wu during the load-in (the drop-in's grace, lengthened to two bars
whatever the kit). A kit of none (ONE TRACK on Track 1) has no load-in. Reduce flashing: no ring.

---

## 6. How a timed run ends

FULL BLAST, TRACERS and SONGLINE last as many bars as their page says and end on the music.

| When | What happens |
|---|---|
| Eight bars before the end | `LAST CALL` (cap 28, the mode's colours) under the phrase bar for two bars; the HUD's run bar (`screens.md`) blinks on the beat from here |
| The last four bars | Whatever the form says (each page ends in a Break or a Drop's tail); nothing new that can hurt spawns in the last bar |
| The last bar | A riser (the Build's) and a snare roll into the line; the picture tightens as a Build's last bar does (vignette, lattice stiff), without the 85% time |
| **The last downbeat** | The Drop's impact; every body alive pops in one ring from the ship outward (no score, no Shards); time dips to 50% for one beat; the lattice rings once; the mode's own count (`tracers.md`, `songline.md`) plays over the next bar |
| A bar later | The result (`screens.md`), the run behind it at a fifth |

Back and the pause work as in the set; a paused timed run resumes with the count-in where it stood.

---

## 7. Nobody is out: the knock-out

In the set, a hit with no cells left ends a solo run and downs a co-op player. In the three timed modes
and in every versus match there is no game over and no beacon:

- The hit is met as always first: Auto Nova, then an Extra Life where the mode has them.
- Then the ship is **knocked out**: it shatters (the large kill recipe in its colour) and comes back on
  the second bar line after (one to two bars away), where it was (on the Stage: at its centre), with
  two cells, a panic pulse, one bar in which it cannot be hurt, and half its Chain.
- What else it costs is on the mode's page (ground in TRACERS, the streak in SONGLINE). It is never
  more than a few seconds and a little score.
- While out, the player's camera stays where the ship was and the HUD shows `BACK IN 2`, `BACK IN 1`
  (bars) where the ship stood, in the player's colour.

JAM needs none of this: nothing there can hit a ship.

---

## 8. Versus

Two to four players, each for themselves, every mode. The host chooses TEAM or VERSUS in the lobby
(`screens.md`). The common rules; each page adds its own:

| Rule | Statement |
|---|---|
| V1 | **What is counted** is one number, named on the page (score, acts won, tiles, gates, pops), and every player sees every player's, ranked, all match long (`screens.md`, "The versus HUD") |
| V2 | **Nothing of one player's hurts another** (rule 9 above). Shots pass through rivals; a Nova does nothing to them |
| V3 | **The shove.** A Dash's shock ring (140 wu) pushes each rival ship in it 200 wu away from where the Dash ended, over a quarter of a second, as Event Horizon's pushes move a ship (a push it rides, never a jump: `PlayerState::shoves`). The rival's own device applies it. A ship that cannot be hurt cannot be shoved. No damage |
| V4 | **Loot.** Shards, Cells and Trios are each player's own, as in co-op. **Boost orbs are shared: one orb, and the first ship to touch it has it** (in the set and in co-op every player has their own) |
| V5 | **The crown.** The leader's ship wears a small crown (18 wu wide, 26 wu above the ship, in the pickup's white-gold at 2.0, on every device). It changes hands only on a bar line, with a bell and eight sparks. No crown while the count is level |
| V6 | **Nobody is out** (section 7): a knocked-out player is back within two bars |
| V7 | **Help for the one behind** (the host's switch, on unless switched off). On each bar line the player in last place, if they hold under 70% of the leader's count, is the **underdog** until the next line: the page says what that gives. Their name in the ranking carries `UNDERDOG` and their halo a second thin ring, so the help is seen |
| V8 | **The match ends** as the mode ends; then the podium (`screens.md`). The host's REMATCH starts the same match again on a new seed; the room keeps each seat's wins as crowns for as long as it stands |
| V9 | **Scaling.** The crowd and the fights scale by the number of players as in co-op (GAME_DESIGN 8) |

What each mode counts, and how rivals meet:

| Mode | Counts | The contest |
|---|---|---|
| THE SET, ONE TRACK | Score when the last boss falls | A race through one crowd; shared Boosts; the underdog's Shards count double for the Chain |
| HEADLINERS | Acts won | Each boss goes to whoever hurt it most; its health bar fills in the players' colours |
| FULL BLAST | Score at the last downbeat | The same storm for all; shared Boosts; the underdog's Chain is not halved by a hit |
| TRACERS | Tiles when the music stops | Rings repaint rivals' ground; crossing a rival's tracer cuts it; a ring spins a rival inside it out |
| SONGLINE | Gates taken | One string of gates, first come; the tune's shape says where the next will be |
| JAM | Pops in one long Drop | POP-OFF: five picks each, then the flood |

Not in this plan: teams of two against two, more than four players, play over the internet.

---

## 9. Records

`modes.txt` in the app's private storage, beside `settings.txt` and `highscores.txt`, written the same
way (key=value lines, a temporary file renamed over the old one; a missing or damaged file is no
records). THE SET's table is untouched.

| Key | Value | Shown |
|---|---|---|
| `mode` | The mode last played (0-6) | The title's mode pill |
| `track_<mode>` | The Track last chosen for that mode | The MODES screen's strip |
| `one_<track>` | Best score, cleared (0 or 1), seconds | ONE TRACK's strip and result |
| `headliners` | Acts cleared, seconds | Its card and result |
| `blast_<track>`, `tracers_<track>`, `songline_<track>` | Best score and the mode's second number (best wave, full houses, gates in a hundred) | The card (the best of all Tracks) and the strip |
| `team_<mode>_<track>` | A co-op run's best team score and its player count | The co-op result: `TEAM BEST 2,104,880  3P` |

A solo record is a solo run's only. Versus keeps nothing but the room's crowns. JAM keeps nothing.
A cleared ONE TRACK and a finished HEADLINERS carry the gold star the set's finished run does.

---

## 10. Colours, emblems and words

Each mode has two colours (OKLCH, full chroma at that lightness) for its card, its emblem, its name
card, its goal line and its result; and an emblem: a small moving picture in the game's lines that
shows what you do. `screens.md` draws them.

| Mode | From | To | Emblem (moves on the beat) |
|---|---|---|---|
| THE SET | P1 cyan, 0.86 at 200 | Violet, 0.70 at 300 | Ten upright bars in the ten Tracks' accents, rising and falling as a level meter, one step a beat |
| ONE TRACK | The chosen Track's accent | Its first enemy hue | The same ten bars at rest, the chosen one tall and lit, a white ring round it |
| HEADLINERS | Amber, 0.82 at 70 | Orchid, 0.72 at 318 | Ten four-point stars on a ring; one flares on each beat, going round |
| FULL BLAST | Ice blue, 0.85 at 230 | Electric violet, 0.72 at 295 | The ship with a fan of seven shot lanes in Prism's colours, each lane firing on its sixteenth |
| TRACERS | Spring green, 0.90 at 140 | Aqua, 0.86 at 190 | The ship drawing a ring; it closes once a bar and the nine tiles inside light |
| SONGLINE | Green, 0.86 at 150 | Violet, 0.70 at 295 | A waving string of eight rings; a spark runs along it once a bar and each ring it passes lights |
| JAM | Tangerine, 0.80 at 55 | Sky, 0.82 at 245 | Eight pads in two rows, lighting in turn on the beats |

None is within 30 degrees of the danger hue. Words a player reads (`screens.md` has where):

| Mode | Caption on its card | Name card's second line | Goal line (HUD) |
|---|---|---|---|
| THE SET | `ALL TEN TRACKS, THEN THE ENCORE` | (the Track's card, as built) | — |
| ONE TRACK | `PICK A TRACK AND BEAT ITS BOSS` | `BEAT ITS BOSS` | — (the phrase bar says it) |
| HEADLINERS | `THE TEN BOSSES, BACK TO BACK` | `ACT 3 OF 10` | `ACT 3 OF 10   4:12` |
| FULL BLAST | `EVERY POWER ON. THREE MINUTES` | `EVERY POWER IS ON` | `WAVE 2 OF 4   BARS LEFT 51` |
| TRACERS | `DRAW RINGS OF LIGHT. NO AIMING` | `RING THEM. LIGHT THE FLOOR` | `FLOOR 62%   HOUSE 2` |
| SONGLINE | `FLY ALONG THE TUNE` | `FOLLOW THE SPARK` | `GATES 148   STREAK 32` |
| JAM | `NOTHING HURTS. EVERY TOY IS YOURS` | `PLAY` | `POPS 1,204` |

The words' rules are the upgrades' (`design/screens.md`): plain, no word a player must know music for,
at most 34 characters on a card and 26 in the HUD.

---

## 11. The two bonus Tracks

Two modes want music the set does not have. Both are new Tracks in the set's own format and voices, so
the band plays them as it plays the ten; they are outside the set list (THE SET, ONE TRACK and
HEADLINERS never play them) and can be chosen in FULL BLAST, TRACERS, SONGLINE and JAM.

| | **11 Mirrorball** | **12 Toybox** |
|---|---|---|
| Home of | TRACERS | JAM |
| Tempo, key | 122, D major: Em9, A13, Dmaj9, Bm7 (two, five, one, six) | 100, C major: C add 9, Am7, Fmaj7, G with its fourth |
| What is new to the ear | The first Track on seventh chords; the open hat on every off-beat; a walking bass (root, octave, fifth) | The slowest Track; a music box; every note above the bass is one of five (C D E G A) |
| Drums | Four on the floor, clap on two and four | A soft four on the floor |
| Lead | A string line of chord tones: a low note answered by a high one | Bells: up five notes, down three |
| Why the mode needs it | TRACERS is a dance floor that lights under the player's feet; it wants the music a lit floor was made for, and a lead whose every second note is still a tune (the tracer drops a note a sixteenth) | JAM's player switches layers on in any order, and is five years old: no combination of this Track's layers can clash |
| File | `music/track11.json` | `music/track12.json` |

Measured by `design/tools/bonus_tracks.py` against the range of the first three Tracks (the set's own
yardstick); the numbers are in `sound.md`. **Neither has been heard by anyone.** They need the owner's
ear before they ship, as every Track did: `music/previews/track11_cycle.mp3`, `track12_cycle.mp3` and
`bonus_sampler.mp3`. If one is not liked, its mode's home becomes a set Track (TRACERS: Zenith; JAM:
Arcadia) and nothing else in this plan changes.

Each has a palette, to be added to `design/tools/palettes.py`'s `PALETTES` as written here and
exported (`Palettes::kMax` is 12: there is room for exactly these two). Both keep the gold rule and
the danger gap; the tool picks the three enemy hues by its rules.

```python
    # Mirrorball (bonus Track 11): a night club. Silver on midnight; the colours are the lights' own.
    dict(id="mirrorball", name="Mirrorball", track=11,
         bg0=dict(L=0.08, h=275, c=0.030), bg1=dict(L=0.17, h=290, c=0.060),
         grid=dict(L=0.50, h=285, c=0.045), gridHot=dict(L=0.95, h=285, c=0.035),
         accent=dict(L=0.96, h=280, c=0.03),
         enemies=[dict(L=0.84, h=150, cf=0.97), dict(L=0.68, h=258, cf=0.97), dict(L=0.72, h=318, cf=0.95),
                  dict(L=0.80, h=55, cf=0.95), dict(L=0.86, h=172, cf=0.95)]),
    # Toybox (bonus Track 12): crayons on a dark blanket. Softer than Arcadia's primaries: pastel, cf under 0.8.
    dict(id="toybox", name="Toybox", track=12,
         bg0=dict(L=0.11, h=275, c=0.035), bg1=dict(L=0.20, h=250, c=0.060),
         grid=dict(L=0.48, h=265, c=0.080), gridHot=dict(L=0.92, h=265, c=0.050),
         accent=dict(L=0.90, h=160, cf=0.60),
         enemies=[dict(L=0.82, h=245, cf=0.75), dict(L=0.80, h=55, cf=0.80), dict(L=0.86, h=170, cf=0.70),
                  dict(L=0.84, h=150, cf=0.75), dict(L=0.76, h=295, cf=0.75)]),
```

Mirrorball's crowd is emerald, its hunters electric blue, its specials orchid, on a silver lattice: the
room is grey until something in it is lit. Toybox's are sky, tangerine and mint. The engineer runs the
tool and looks at `design/previews/palettes.png`; a hue that reads wrong on the sheet comes back to the lead.

What the engine needs for a twelfth Track is in `docs/MODES_PLAN.md` (the Track tables' size, the
calibration, the baked risers, the tempo table).
