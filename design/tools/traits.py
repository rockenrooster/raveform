"""Elites and their twelve traits (GAME_DESIGN 7.3): the numbers.

Writes design/traits.json and design/traits.md from the table below. The look of every trait is in
design/look.md section 4 ("Traits"), the elite's own light in section 3.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\traits.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")

ELITE = {
    "note": "An elite is a species of the Track's roster made dangerous: one body the player has to deal with on purpose. World units; factors multiply the species' own numbers (design/species.json).",
    "who": "any species of the Track's roster except Shoal darts and Wyrm segments (a Wyrm's head may be elite: its chain follows)",
    "scale": 1.3,
    "health": 6.0,
    "score": 10.0,
    "threat": 4.0,
    "shards": 20,
    "drops": "one Cell, whatever the players' health (GAME_DESIGN 6.1), and its Shards",
    "coop": "health x (1 + 0.5 x (players - 1)), as mini-bosses and bosses",
    "look": "look.md 3: hull + 0.6, 2.3 px, a white copy of the hull, glass, an aura at 0.22; a white edge chevron while it is off screen; never dimmed by the light budget",
    "arrival": "warps in like any enemy but from 90 wu and with the elite's low fifth (root and fifth of the Track's key); its trait's mark sounds a beat later",
    "when": {
        "from": "Track 2, cycle 2",
        "pace": "the director buys an elite with the threat budget about once in 16 bars in Track 2, falling evenly to once in 8 bars by Track 6 and staying there; never in a Break, a boss fight or a mini-boss fight",
        "alive": {"tracks2to6": 3, "tracks7to10": 5, "note": "at most this many at once, for all players together"},
    },
    "traitsCarried": {"tracks2to6": 1, "tracks7to10": 2, "loop": "one more for each time round, to four at most", "note": "drawn by the seed from the traits the run has met; never two that hide (Cloaked and Phasing) on one body"},
}

TRAITS = [
    dict(id="armored", name="Armored", fromTrack=2, effect="health x 2 again. While its outer hull holds (the first half of its health) shots spark off it without the hit flash; at half health the outer hull shatters and it is an ordinary elite",
         numbers={"healthFactor": 2.0, "outerHullShare": 0.5}, mark="a gong, short"),
    dict(id="swift", name="Swift", fromTrack=2, effect="moves at 1.6 times its speed and turns 1.3 times as fast",
         numbers={"speedFactor": 1.6, "turnFactor": 1.3}, mark="an upward sweep"),
    dict(id="giant", name="Giant", fromTrack=3, effect="1.8 times the size (on top of the elite's 1.3), health x 2.5, cannot be shoved; its death is a large kill",
         numbers={"scaleFactor": 1.8, "healthFactor": 2.5}, mark="the sub drop"),
    dict(id="splitting", name="Splitting", fromTrack=3, effect="when it dies two copies leave it, 0.6 of its size, each with 30% of its health and no trait; they do not split again",
         numbers={"copies": 2, "copyScale": 0.6, "copyHealthShare": 0.3}, mark="a glass crack"),
    dict(id="volatile", name="Volatile", fromTrack=4, effect="its death is a bomb with a warning: a danger ring of 160 wu fills over two beats and bursts on the beat, taking a cell from any ship inside and dealing 30 x the Track's number to every enemy inside (other Volatiles go off in turn)",
         numbers={"ringWu": 160.0, "fuseBeats": 2, "enemyDamagePerTrack": 30.0}, mark="a crackle"),
    dict(id="regenerating", name="Regenerating", fromTrack=4, effect="after two beats without being hit it heals 8% of its health on every beat",
         numbers={"quietBeats": 2, "healPerBeat": 0.08}, mark="two bells rising"),
    dict(id="shielded", name="Shielded", fromTrack=5, effect="its front 120 degrees sheds every shot; it turns to keep the shield on the nearest player at 1.2 radians a second",
         numbers={"arcDegrees": 120.0, "turnRate": 1.2}, mark="two woodblocks"),
    dict(id="twin", name="Twin", fromTrack=5, effect="arrives as two bodies with one health bar (x 1.6): damage to either counts for both, they keep 200 to 300 wu apart, and they die together",
         numbers={"healthFactor": 1.6, "apartWu": [200.0, 300.0]}, mark="the fifth again, a beat later"),
    dict(id="magnetic", name="Magnetic", fromTrack=6, effect="steals loose Shards within 300 wu, up to forty; they orbit it, and its death releases them doubled",
         numbers={"reachWu": 300.0, "maxShards": 40, "releaseFactor": 2.0}, mark="Shard chirps falling"),
    dict(id="commander", name="Commander", fromTrack=6, effect="enemies within 400 wu of it move 30% faster; when it dies they are stunned for one beat",
         numbers={"reachWu": 400.0, "speedFactor": 1.3, "stunBeats": 1}, mark="a snare roll of one beat"),
    dict(id="cloaked", name="Cloaked", fromTrack=7, effect="drawn at 8% between beats and in full on each beat's flash (the 110 ms envelope); it can be hit at any time, and whatever it fires is on the priority layer as always",
         numbers={"betweenBeats": 0.08}, mark="a short reversed cymbal"),
    dict(id="phasing", name="Phasing", fromTrack=7, effect="solid only in the first half of each beat: in the second half shots pass through it and its touch does not hurt",
         numbers={"solidShareOfBeat": 0.5}, mark="the fifth, gated on the eighths"),
]


def main():
    data = {"note": "Generated by design/tools/traits.py. GAME_DESIGN 7.3: elites and their twelve traits. Looks: design/look.md sections 3 and 4.",
            "elite": ELITE, "traits": TRAITS,
            "byTrack": {str(t): [x["id"] for x in TRAITS if x["fromTrack"] <= t] for t in range(2, 11)}}
    with open(os.path.join(DESIGN, "traits.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1)
    e = ELITE
    md = ["# RAVEFORM — Elites and their traits", "",
          "An elite is one body the player has to deal with on purpose (GAME_DESIGN 7.3). It is a species of",
          "the Track's roster made dangerous, and it carries a **trait**: one more rule, shown on its body and",
          "heard as it arrives. Twelve traits, two new ones a Track from Track 2 to Track 7.", "",
          "Data: `design/traits.json` (load it, do not retype it). Tool: `design/tools/traits.py`. Looks:",
          "`design/look.md` section 3 (the elite's light) and section 4 (each trait's treatment).", "",
          "---", "", "## Every elite", "", "| | |", "|---|---|",
          "| Who | %s |" % (e["who"][0].upper() + e["who"][1:]),
          "| Size, health, score, threat | × %s, × %s, × %s, × %s of its species |" % (e["scale"], int(e["health"]), int(e["score"]), int(e["threat"])),
          "| Drops | %s Shards; %s |" % (e["shards"], e["drops"]),
          "| Co-op | %s |" % (e["coop"][0].upper() + e["coop"][1:]),
          "| Look | %s |" % (e["look"][0].upper() + e["look"][1:]),
          "| Arrival | %s |" % (e["arrival"][0].upper() + e["arrival"][1:]),
          "| From | %s |" % e["when"]["from"],
          "| How often | %s |" % (e["when"]["pace"][0].upper() + e["when"]["pace"][1:]),
          "| At once | %d in Tracks 2 to 6, %d in Tracks 7 to 10, for all players together |" % (e["when"]["alive"]["tracks2to6"], e["when"]["alive"]["tracks7to10"]),
          "| Traits carried | One in Tracks 2 to 6, two in Tracks 7 to 10; in the loop %s. Drawn by the seed from the traits the run has met; never Cloaked and Phasing on one body |" % e["traitsCarried"]["loop"],
          "", "---", "", "## The twelve", "", "| Trait | From Track | What it does | Heard as it arrives |", "|---|---|---|---|"]
    for t in TRAITS:
        md.append("| **%s** | %d | %s | %s |" % (t["name"], t["fromTrack"], t["effect"][0].upper() + t["effect"][1:], t["mark"][0].upper() + t["mark"][1:]))
    md += ["", "The two that hide come last, in Track 7, when the player knows everything else an elite can be.",
           "A trait changes what the player does, not only how long the elite lasts: flank the Shielded, stop",
           "firing at the Phasing on the off-beat, walk away from the Volatile, kill the Commander first.", ""]
    open(os.path.join(DESIGN, "traits.md"), "w", encoding="utf-8", newline="\n").write("\n".join(md))
    print("traits", len(TRAITS), "by Track", {k: len(v) for k, v in data["byTrack"].items()})


if __name__ == "__main__":
    main()
