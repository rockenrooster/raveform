# RAVEFORM — The arena: structures and events

Six structures and eight events (GAME_DESIGN 9). They give a reason to cross the map, something to
play with besides the crowd, and in co-op a reason to split up. None of them is a puzzle: each is one
sentence, and each one sounds like part of the Track.

Data: `design/arena.json` (rules common to all, one entry each, which Track has which, and the shapes
as glyph segments in the format of `design/glyphs.json` — load it, do not retype it). Picture:
`design/previews/arena.png`. Tool: `design/tools/arena.py`. The seventh structure, the mini-bosses'
Pen, is in `design/bosses/minibosses.md`.

---

## Rules for all of them

- **Scale.** The mini-boss health of that Track in design/bosses/minibosses.json (Track 1 uses 300), times the fraction given: *trackScale* below.
- **Where and when.** A structure stands in the world for one Track; an event is a few bars long. Both are placed by the run's seed relative to the players' centre, never inside a player's frame, never within 400 wu of a wall, and never while a boss or a mini-boss is alive.
- **Structures** are placed on the first downbeat of the Track, 1,000 to 1,800 wu from the players' centre, at least 700 wu from each other; the last Track's dissolve over the Track change's Break unless a player is within 600 wu, in which case they stay until that player leaves.
- **Events:** Track 1 has one, the Comet, in the Groove of its second cycle; from Track 2 one event a Track, in the Groove of its second cycle; from Track 4 a second one in the Groove of its first cycle (every Track is two cycles since 2026-11-14). Drawn by the seed from the Track's list, never the event of the Track before. Announced one bar ahead by a gold edge chevron and its name as world text at the place (cap 22, gold), then it runs.
- **Indicators:** structures get no edge chevron (they are found); events get the gold one; a Rift's is the enemy hue with a ring at its tip.
- **Co-op:** placed round the players' centre; every reward is for each player (Boost orbs and Shards are each player's own, as everywhere).
- **Caps:** at most 6 structures and 1 event alive; what an event or a structure spawns counts against the enemy and bullet caps.

---

## Structures

### Vault

*An armoured crystal: break it for a Boost and Shards.*

| | |
|---|---|
| Size | radius 60 wu |
| Rules | Health: trackScale x 0.33 |
|  | Takes damage from shots, Riffs and Novas; nothing else touches it; it does not hurt |
|  | Each quarter of its health lost, one ring of its ornament shatters and it rings one step higher |
|  | Broken: one Boost orb and 40 Shards for each player |
| Look | Three nested hexagons in white and gold with a glass fill and a gold pool at 0.2; it turns a twelfth of a turn a bar and swells 6% on the kick |
| Sound | Each hit a glass tick on a note of the scale, one step up for each quarter lost; the break an arpeggio of bells up the chord with a glass burst |

### Pylon

*A turret you tune by standing still.*

| | |
|---|---|
| Size | radius 26 wu |
| Rules | Its ring is 150 wu, dashed in the palette accent: stand inside for four beats and it fills clockwise; leave and it empties twice as fast |
|  | Tuned, it takes the tuner's colour and on the One of every bar fires a ring that runs to 520 wu at 700 wu/s and deals 6 x the Track's number to each enemy once |
|  | It stays tuned for one cycle (36 bars; 24 in Track 1), then dims and can be tuned again; enemies ignore it |
| Look | A tall diamond with a white spine and four white ticks; untuned at 0.8 in the accent, tuned at 2.4 in the player's colours with a pool at 0.15; its pulse is a Riff ring in that player's colours (rings are a language: look.md 6) |
| Sound | The fill is a woodblock on each of the four beats, rising; tuned is three bells up the chord; each pulse a bell on the chord's root an octave up, on the One |

### Rift

*A tear the crowd comes through: close it.*

| | |
|---|---|
| Size | radius 80 wu |
| Rules | Opens with the Groove of the Track's second cycle and stays through the Build; at the Drop it closes by itself and gives nothing |
|  | While open, four of the Track's fodder come through it every bar (they are part of the crowd target, not on top of it) |
|  | Health: trackScale x 0.25; destroyed: the flow stops, one Boost orb and 30 Shards for each player |
| Look | Three broken rings in the first enemy hue turning against each other, a black disc inside (the only filled black in the game: the lattice is pulled 20 wu into it) and sparks spiralling in; off screen its chevron carries a ring |
| Sound | A drone on the chord's bass note while it is open, its cutoff rising through the Build; a tom for each body that comes through; destroyed: a reversed cymbal cut short, then glass |

### Bumper

*It bounces everything, and it is tuned.*

| | |
|---|---|
| Size | radius 45 wu |
| Rules | Ships bounce off it unhurt with a kick of 600 wu/s; player shots reflect off it (a free bounce, and x 1.5 damage afterwards); enemies and their bullets bounce too |
|  | Every player shot that hits it charges it; at eight it discharges on the next beat: a ring to 260 wu dealing 10 x the Track's number to enemies |
|  | A Track that has Bumpers has three or four, 300 to 500 wu apart: a cluster to play in |
| Look | A ring with a smaller ring inside and four white ticks; its fill brightens with its charge (glass 0.05 to 0.4) and the discharge is a white shock ring |
| Sound | Each Bumper of a cluster owns a chord tone (root, third, fifth, octave): a bounce is a tom on its note, so a shot rattling between them plays the chord; the discharge is the chord's stab with a zap |

### Current

*A lane that carries whatever enters it.*

| | |
|---|---|
| Size | a lane |
| Rules | A lane 240 wu wide and 1,600 long; anything inside moves 260 wu/s faster along it: ships, enemies, enemy bullets |
|  | Player shots travelling with it are 30% faster and deal 30% more; against it, 30% slower and no less |
|  | A Track that has Currents has two, crossing |
| Look | No walls: rows of chevrons in the palette's grid colour at 0.6 streaming along it on the eighths, and the lattice inside it stretched 12 wu downstream |
| Sound | A whoosh that is heard only inside, panned along the flow; entering it is a short upward sweep |

### Lens

*A bar of glass that splits your shots in three.*

| | |
|---|---|
| Size | 320 wu long |
| Rules | A line 320 wu long; a player shot crossing it becomes three: the middle keeps its damage, the two beside it fan 14 degrees out at half damage |
|  | Ships, enemies and enemy bullets pass through it untouched; it cannot be hurt; a shot is split once by each Lens |
|  | The split shots take the hues 60 degrees either side of the player's, as Prism's lanes do |
| Look | A thin lozenge of white glass with a bright spine; it turns slowly, one turn every eight bars, so the fan sweeps |
| Sound | Each split is a glass tick a fifth above the lead's note |

GAME_DESIGN 9 called this structure 'Prism'; it is the Lens here so that it is not confused with the Boost of that name.

---

## Events

### Vault Drop

*A rich Vault that will not wait.* (8 bars)

| | |
|---|---|
| Rules | A gold Vault warps in 1,000 to 1,400 wu from the players' centre under a column of light |
|  | Health trackScale x 0.4; broken: two Boost orbs and 80 Shards for each player |
|  | A ring round it empties over eight bars; then it warps out |
| Look | The Vault in gold with a flare 400 px tall for its first bar and a thin timer ring at 90 wu |
| Sound | It lands with the gong; its ring ticks a woodblock on each bar line, faster in the last bar |

### Rift Surge

*Three Rifts at once.* (as long as it takes)

| | |
|---|---|
| Rules | At the start of a Build three Rifts open on a triangle 900 wu from the players' centre, each letting six through a bar |
|  | They close at the end of the Drop |
|  | Each one destroyed pays as a Rift; all three: a Cell for each hurt player, 100 Shards for the others |
| Look | As the Rift; the three are joined by a faint triangle of their hue until one is closed |
| Sound | Three drones a fifth apart; each closing takes its note away |

### Elite Hunt

*An elite that runs.* (12 bars)

| | |
|---|---|
| Rules | One elite of the Track's highest-threat family warps in 1,200 wu away carrying two traits (three from Track 7) and flees the nearest player at 0.8 of its speed |
|  | Killed within twelve bars: a Trio for each player (as a level's) and 150 Shards; otherwise it warps out |
| Look | The elite's own look with a white edge chevron; a dotted white line trails it for a bar so that it can be followed |
| Sound | The elite's arrival fifth, then its family's own voice receding; the kill is the upgrade chord |

### Comet

*A neutral body crossing the arena, trailing Shards.* (as long as it takes)

| | |
|---|---|
| Rules | It crosses on a straight line passing within 300 wu of the players' centre at 700 wu/s and leaves five Shards a beat behind it |
|  | Health trackScale x 0.1; broken: 60 Shards and one Boost orb for each player |
|  | It hurts nothing and nothing but player fire hurts it |
| Look | A white kite with a three-line tail in gold fading over 200 wu, a sparkle at its head |
| Sound | A saw glide rising over the two beats of its warning, then a bell on each Shard it drops; no name card, it is too fast: a white chevron two beats ahead |

### Eclipse

*Lights out: the crowd is seen on the beat.* (8 bars)

| | |
|---|---|
| Rules | For eight bars the scene falls dark and comes back for a flash on every beat (the kick's envelope, 110 ms) |
|  | The priority layer never dims: every bullet, beam and ship is as bright as ever |
|  | Kills score double while it lasts |
| Look | Scene light x 0.15 between beats (enemy lines, effects, lattice heat), the lattice at 0.05, the background off; players' own light is untouched; a dark ring sweeps across the frame as it begins and ends. Reduce flashing: x 0.4 between beats and the flash adds 0.3 |
| Sound | The music bus low-passed at 1.2 kHz for the eight bars; a soft tick on each flash |

### Meteor Rain

*Impacts on the beat, each one announced.* (4 bars)

| | |
|---|---|
| Rules | For four bars, on every beat, three dashed danger rings of 110 wu appear in the frame, one of them where a player stands |
|  | Two beats later, on the beat, each one lands: a cell from any ship inside, 20 x the Track's number to every enemy inside |
| Look | The ring fills from its edge to its centre over the two beats, on the priority layer, and **the meteor itself falls onto it**: a white-hot head with a gold tail coming down from high on the frame's upper right, faster as it comes, landing at the centre on the beat. **The impact** (the owner, 2026-11-14: "meteor rain needs more of a visual impact on where it lands"): a white-hot flash the size of the ring, three rings past it, a crater glowing in the danger colour for most of a bar, debris in gold and white, a distortion ring, a hot lattice dent and a harder shake (until then a large kill in white with a hot lattice dent) |
| Sound | Each warning is a saw glide falling over its two beats; each impact the sub drop and a crash, on the beat |

### Convoy

*Slow carriers with heavy loot, and an escort.* (12 bars)

| | |
|---|---|
| Rules | Three carriers cross in a line at 60 wu/s, starting 1,500 wu from the players' centre, with eight Hounds of the Track's tier round them |
|  | Each carrier: health trackScale x 0.33; one Boost orb and 50 Shards for each player; the last of the three also drops a Cell |
|  | After twelve bars whatever is left warps out |
| Look | The carrier: an octagon in gold with a white hold and a plus, radius 44, a gold pool; the three joined by a thin gold line |
| Sound | A low tom on every beat while they roll, one voice a carrier; each one broken is the Boost bells |

### Storm Cell

*A field of lightning that roams: a weapon if you lead the crowd into it.* (8 bars)

| | |
|---|---|
| Rules | A circle of 320 wu drifting toward the players' centre at 90 wu/s for eight bars |
|  | On every beat lightning joins up to six bodies inside it: 15 x the Track's number to each enemy |
|  | A ship inside on a beat is warned (its halo crackles); still inside on the next beat it loses a cell |
| Look | A ring in the palette accent with the lattice hot inside it; the strikes are zigzag lines drawn for one beat in white and the accent |
| Sound | A crackle that rises as it nears; each strike a zap on the chord's three notes |

---

## Which Track has which

| Track | Structures | Events (one is drawn; two from Track 4) |
|---|---|---|
| 1 | none | Comet |
| 2 | Vault, Pylon × 2 | Vault Drop, Comet |
| 3 | Vault, Pylon, Rift | Rift Surge, Vault Drop, Comet |
| 4 | Bumper × 3, Vault, Rift | Elite Hunt, Meteor Rain |
| 5 | Current × 2, Pylon, Rift | Convoy, Storm Cell |
| 6 | Lens × 2, Bumper × 2, Vault | Eclipse, Elite Hunt |
| 7 | Pylon × 2, Current, Vault | Vault Drop, Convoy, Comet |
| 8 | Bumper × 4, Lens × 2 | Meteor Rain, Rift Surge |
| 9 | Current × 2, Lens, Rift × 2 | Storm Cell, Eclipse |
| 10 | Vault, Pylon, Rift, Bumper, Current, Lens | Elite Hunt, Meteor Rain, Convoy, Storm Cell |

Track 1 has two cycles since play test 3 and every other Track since 2026-11-14; Track 1's Comet comes in the Groove of the second, and from Track 4 the second event comes in the first cycle's Groove. In the
loop each Track keeps its list. An event never outlasts its Groove: every Groove is 8 bars now, and
an event written longer ends with it (the Elite Hunt's twelve bars are eight).

**A Vault says what it is** (play test 3: after a retry the owner took Track 1's Vault, standing
alone on the first downbeat, for a dead boss's body). Track 1 has no structure. From Track 2 a
Vault carries its name, `VAULT`, as world text above it (cap 22, gold) until its first hit, and a
gold edge chevron while it is off screen within 1,200 wu, like any pickup worth going to.

**Meteor Rain, the meteor** (the owner, 2026-11-14: "Meteor projectiles need to be bigger so that it makes sense with the hit area
size and impact force"): each falls onto its ring over the warning's two beats as a burning rock 88 wu across at the ground (40% of
its 220 wu ring), smaller (55%) high in the sky as it starts, turning; white-hot on the side facing the ground and gold behind; a
cone of fire behind it (five streaks from its rim, 200 wu and more as it speeds up), embers shed behind, and a warm glow, all
brighter as it nears the ground. Drawn only (`PlayView`, `kMeteorBodyWu`); where it hurts is the ring, as before.
