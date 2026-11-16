// M8 part 5 (design/modes/songline.md; docs/MODES_PLAN.md 2.3): SONGLINE. A spark of light flies through the arena playing the
// Track's tune, and every note it plays hangs behind it as a ring: a gate. Fly with the Spark and through its gates: each feeds the
// Chain and the streak, and the streak powers the gun. The crowd comes along the line. Everything here is computed from the run's
// seed and the song's clock (nothing on the wire but rule 4's bend, decided by the host); it runs only in SONGLINE (World::tick).

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr double kSixteenth = kTicksPerBeat / 4.0;
Vec2 perpL(Vec2 d) { return Vec2{-d.y, d.x}; }
// The turn (+1 left, -1 right) whose circle of `radius` from `p` heading `hd` stays furthest inside the walls; level, the side of
// the arena's centre. `prefer` (rule 4's homing) wins while its circle keeps 400 wu of room.
float safeTurn(Vec2 p, float hd, float radius, float hw, float hh, float prefer = 0.0f) {
    const Vec2 dir = dirOf(hd);
    float room[2];
    for (u32 j = 0; j < 2; ++j) {
        const Vec2 c = p + perpL(dir) * (j == 0 ? radius : -radius);
        room[j] = std::fmin(hw - std::fabs(c.x), hh - std::fabs(c.y)) - radius;
    }
    if (prefer != 0.0f && room[prefer > 0.0f ? 0 : 1] >= 400.0f) return prefer;
    if (std::fabs(room[0] - room[1]) > 1.0f) return room[0] > room[1] ? 1.0f : -1.0f;
    return (dir.x * (-p.y) - dir.y * (-p.x)) >= 0.0f ? 1.0f : -1.0f;
}
}  // namespace

float World::gateRadius(const SongGate& g) const {
    const ModesData::Songline& d = species_->modes.songline;
    return g.kind == 1 ? d.breakRadiusWu : (g.kind == 2 ? d.goldRadiusWu : d.radiusWu);
}

i32 World::songSection(u32 step) const {
    // The section a sixteenth lies in (the timed form at its bar), -1 where the Spark lays nothing: the Build's last bar, past the end.
    const u32 bar = step / 16u;
    const Phrase ph = phraseAt(director_, bar);
    if (ph.ended) return -1;
    if (ph.buildLastBar()) return -1;
    switch (ph.section) {
        case Section::Build: return 1;
        case Section::Drop: return 2;
        case Section::Break: return 3;
        default: return 0;
    }
}

void World::resetSongline() {
    // The Spark starts startAheadWu ahead of the first ship, heading the way the ships face (east), its waves' phases from the seed;
    // its line is laid a bar ahead.
    const ModesData::Songline& d = species_->modes.songline;
    spark_ = SongSpark{};
    u32 h = hash32(static_cast<u32>(seed_ ^ (seed_ >> 32)), 0x50A6u);
    spark_.phase[0] = randRange(h, 0.0f, kTau);
    spark_.phase[1] = randRange(h, 0.0f, kTau);
    spark_.ahead[0] = players_[0].pos + Vec2{d.startAheadWu, 0.0f};
    spark_.aheadHeading[0] = 0.0f;
    spark_.aheadFlown[0] = 0.0;
    spark_.step0 = 0;
    for (u32 k = 1; k <= 16; ++k) sparkPoint(k);
    spark_.pos = spark_.prevPos = spark_.ahead[0];
    spark_.heading = 0.0f;
    for (SongGate& g : songGates_) g = SongGate{};
    songGateNext_ = 0;
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        streak_[p] = bestStreak_[p] = 0;
        streakDropBar_[p] = ~0u;
        gatesLaid_[p] = gatesThreaded_[p] = gatesInTime_[p] = 0;
        songTaken_[p] = 0;
    }
    sparkSync_ = DirectorSync{};
    layGates(0, spark_.ahead[0], spark_.aheadHeading[0]);  // (the run's first note)
}

void World::sparkPoint(u32 k) {
    // One sixteenth of the line (rule 2): the heading turned at every 8 wu by the two slow waves (by distance flown, scaled by the
    // section), by the walls when the line 600 wu ahead comes within 700 wu of one, and by rule 4's bend; never tighter than
    // maxCurvature. The finale's last two bars close a spiral. Its speed is the section's share of a ship's.
    const ModesData::Songline& d = species_->modes.songline;
    const u32 step = spark_.step0 + k - 1u;
    Vec2 p = spark_.ahead[k - 1u];
    float hd = spark_.aheadHeading[k - 1u];
    double flown = spark_.aheadFlown[k - 1u];
    const u32 bar = step / 16u;
    const Phrase ph = phraseAt(director_, bar);
    const u32 sec = ph.section == Section::Build ? 1u : (ph.section == Section::Drop ? 2u : (ph.section == Section::Break ? 3u : 0u));
    const double tps = static_cast<double>(rf::sim::tempoAt(director_, songStepTick(step))) * kTicksPerBeat / 60.0;
    float speed = d.speedOfShip[sec] * kPlayerSpeed * (spark_.comeBack ? d.comeBackSpeed : 1.0f);
    float len = static_cast<float>(static_cast<double>(speed) * kSixteenth / tps);
    // The finale (the form's last bars): a closing spiral, its radius from 280 wu to 40.
    const u32 total = director_.formCount ? [&] {
        u32 n = 0;
        for (u32 j = 0; j < director_.formCount; ++j) n += director_.formBars[j];
        return n;
    }() : 0u;
    const bool finale = total && bar + d.finaleBars >= total && bar < total;
    if (finale && !spark_.comeBack && spark_.spiralSign == 0.0f) spark_.spiralSign = safeTurn(p, hd, 280.0f, halfW(), halfH());  // (its circles on the open side)
    const float hw = halfW(), hh = halfH();
    float wallSign = spark_.aheadWall[k - 1u];
    float homeSign = spark_.aheadHome[k - 1u];
    float left = len;
    while (left > 1e-3f) {
        const float ds = left < 8.0f ? left : 8.0f;
        left -= ds;
        float k0 = 0.0f;
        if (finale && !spark_.comeBack) {  // (rule 4 first: nobody is left behind by a song, not even by its last bars)
            const double u = (static_cast<double>(step - (total - d.finaleBars) * 16u) + static_cast<double>(len - left) / static_cast<double>(len)) /
                             static_cast<double>(d.finaleBars * 16u);
            const float radius = 280.0f - 240.0f * static_cast<float>(u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u));
            k0 = spark_.spiralSign / radius;
        } else {
            // (bending back for a lost ship the waves rest: they could out-turn the bend)
            if (!spark_.comeBack)
                for (u32 w = 0; w < 2; ++w) k0 += d.curveAmp[w] * std::sin(kTau * static_cast<float>(flown / d.curvePeriodWu[w]) + spark_.phase[w]);
            k0 *= d.sectionScale[sec];
            const Vec2 dir = dirOf(hd);
            const Vec2 look = p + dir * d.wallLookAheadWu;
            float err = 0.0f, want = 0.0f;
            if (spark_.comeBack) {
                const Vec2 to = spark_.comeTo - p;
                err = std::atan2(dir.x * to.y - dir.y * to.x, dot(dir, to));
                want = err >= 0.0f ? 1.0f : -1.0f;
                // (a ship behind the line: one way round, chosen once and held until the ship is ahead again; a sign taken afresh
                // at every step would flip about the ship's bearing and fly the Spark straight on)
                if (std::fabs(err) > 2.6f && homeSign == 0.0f) homeSign = wallSign != 0.0f ? wallSign : safeTurn(p, hd, 250.0f, hw, hh);
                if (std::fabs(err) < 1.57f) homeSign = 0.0f;
                if (homeSign != 0.0f) want = homeSign;
            }
            if (std::fabs(look.x) > hw - d.wallNearWu || std::fabs(look.y) > hh - d.wallNearWu) {
                // Back toward the centre at wallTurn at least, the way round chosen as the line comes near (the turn whose circle
                // stays furthest inside the walls) and held until it is clear; the waves may turn it harder that way, never against it.
                if (wallSign == 0.0f) wallSign = safeTurn(p, hd, 1.0f / d.wallTurn, hw, hh, want);
                k0 = wallSign * std::fmax(d.wallTurn, wallSign * k0);
            } else {
                wallSign = 0.0f;
            }
            if (spark_.comeBack) {
                // (homing: at least comeBackTurn, up to the line's tightest curve as the ship falls to the side, so the Spark never
                // circles a slow ship from outside its reach)
                const float turn = std::fmax(d.comeBackTurn, std::fmin(d.maxCurvature, std::fabs(err) / 250.0f));
                if (wallSign == 0.0f || want == wallSign) k0 += want * turn;  // (never against the walls)
            }
            k0 = clampf(k0, -d.maxCurvature, d.maxCurvature);
        }
        hd += k0 * ds;
        p = p + dirOf(hd) * ds;
        flown += static_cast<double>(ds);
    }
    // (inside the walls whatever the waves did)
    p = Vec2{clampf(p.x, -hw + 200.0f, hw - 200.0f), clampf(p.y, -hh + 200.0f, hh - 200.0f)};
    spark_.ahead[k] = p;
    spark_.aheadHeading[k] = std::remainder(hd, kTau);
    spark_.aheadFlown[k] = flown;
    spark_.aheadWall[k] = finale && !spark_.comeBack ? 0.0f : wallSign;
    spark_.aheadHome[k] = spark_.comeBack ? homeSign : 0.0f;
}

void World::sparkRebuild() {
    for (u32 k = 1; k <= 16; ++k) sparkPoint(k);
}

void World::layGates(u32 step, Vec2 at, float heading) {
    // Rule 3: a gate where the Spark played its note, across the line by the note's height above the middle of the bar's arpeggio
    // (7 wu a semitone, at most 56); co-op: a string a player, each at its offset (the braid swapping them every four bars) and on its
    // own harmony; the Break's large and worth three; a gold gate every eight bars on a bar line.
    const ModesData::Songline& d = species_->modes.songline;
    const ModesData::Tune& tn = species_->modes.tunes[(songTrack_ >= 1 && songTrack_ <= kModeTracks ? songTrack_ : 1u) - 1u];
    const i32 sec = songSection(step);
    if (sec < 0 || !tn.chords) return;
    if (step % d.every[sec]) return;
    const u32 bar = step / 16u, inBar = step % 16u;
    const u32 chord = static_cast<u32>((static_cast<i64>(bar) + static_cast<i64>(songShift_)) % static_cast<i64>(tn.chords) + tn.chords) % tn.chords;
    const u8* arp = tn.arp[chord];
    u8 lo = 255, hi = 0;
    for (u32 j = 0; j < 16; ++j) {
        lo = arp[j] < lo ? arp[j] : lo;
        hi = arp[j] > hi ? arp[j] : hi;
    }
    const u8 note = arp[inBar];
    const float middle = 0.5f * (static_cast<float>(lo) + static_cast<float>(hi));
    float across = clampf(d.weaveWuPerSemitone * (static_cast<float>(note) - middle), -d.weaveMaxWu, d.weaveMaxWu);
    if (sec == 1) {
        // (a Build's wave shrinks to nothing by its last bar: the runway)
        const Phrase ph = phraseAt(director_, bar);
        const float span = static_cast<float>(ph.sectionBars > 1 ? (ph.sectionBars - 1u) * 16u : 16u);
        across *= clampf(1.0f - static_cast<float>(ph.sectionBar * 16u + inBar) / span, 0.0f, 1.0f);
    }
    const Vec2 side = perpL(dirOf(heading));
    // Who gets a string: solo the one player; co-op every seat present; versus one string for everyone.
    u8 seats[kMaxPlayers];
    u32 n = 0;
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].present) seats[n++] = static_cast<u8>(p);
    const bool shared = setup_.versus || n <= 1;
    const u32 strings = shared ? 1u : n;
    const u8 kind = sec == 3 ? 1 : (spark_.spiral ? 3 : 0);
    const bool gold = d.goldEveryBars && inBar == 0 && bar > 0 && bar % d.goldEveryBars == 0;
    const u8 goldBoost = gold ? static_cast<u8>(upgradeRng_.below(kBoostCount)) : static_cast<u8>(0xFF);  // (each player's to take once)
    for (u32 sIdx = 0; sIdx < strings; ++sIdx) {
        float offset = 0.0f;
        u8 sNote = note;
        if (!shared) {
            // The braid: every braidEveryBars the strings change places over one bar.
            const float* offs = d.coopOffsets[n - 2u];
            const u32 block = bar / d.braidEveryBars;
            const float t = clampf(static_cast<float>(bar % d.braidEveryBars) + static_cast<float>(inBar) / 16.0f, 0.0f, 1.0f);
            const float a = offs[(sIdx + block + n - 1u) % n], b = offs[(sIdx + block) % n];
            offset = block ? a + (b - a) * (t * t * (3.0f - 2.0f * t)) : b;
            // Each seat's harmony: its scale steps above the tune (the tune's own string the first seat's).
            const u32 steps = d.harmonySteps[seats[sIdx] < 4 ? seats[sIdx] : 0];
            if (steps && tn.scaleCount) {
                i32 pc = ((static_cast<i32>(note) - static_cast<i32>(tn.tonic)) % 12 + 12) % 12;
                u32 idx = 0;
                for (u32 j = 0; j < tn.scaleCount; ++j)
                    if (tn.scale[j] <= pc) idx = j;
                const i32 base = static_cast<i32>(note) - (pc - tn.scale[idx]);
                const u32 to = idx + steps;
                sNote = static_cast<u8>(base + tn.scale[to % tn.scaleCount] - tn.scale[idx] + 12 * static_cast<i32>(to / tn.scaleCount));
            }
        }
        SongGate* slot = nullptr;
        for (u32 tries = 0; tries < kSongGates && !slot; ++tries) {
            SongGate& g = songGates_[songGateNext_];
            songGateNext_ = (songGateNext_ + 1u) % kSongGates;
            if (g.born < 0.0) slot = &g;
        }
        if (!slot) {
            ++counters_.netOpsDropped;  // (the pool is full: cannot happen at 256 with four strings two bars long)
            return;
        }
        SongGate& g = *slot;
        g = SongGate{};
        g.pos = at + side * (across + offset);
        g.born = songStepTick(step);
        g.step = step;
        g.note = sNote;
        g.kind = gold ? 2 : kind;
        g.string = shared ? static_cast<u8>(0xFF) : seats[sIdx];
        g.boost = gold ? goldBoost : static_cast<u8>(0xFF);
        for (u32 q = 0; q < kMaxPlayers; ++q)
            if (players_[q].present && (g.string == 0xFF || g.string == q)) ++gatesLaid_[q];
        Event ev;
        ev.type = EventType::GateLay;
        ev.a = g.string;
        ev.b = g.note;
        ev.c = g.kind;
        ev.x = g.pos.x;
        ev.y = g.pos.y;
        ev.tick = g.born;
        emit(ev);
    }
}

void World::setStreak(u32 p, u16 s) {
    // Rule 9: the tier the streak reaches; the ship's powers follow it at once.
    const ModesData::Songline& d = species_->modes.songline;
    streak_[p] = s;
    if (s > bestStreak_[p]) bestStreak_[p] = s;
    u8 tier = 0;
    for (u32 k = 0; k < 4; ++k)
        if (s >= d.tiers[k]) tier = static_cast<u8>(k + 1u);
    PlayerState& pl = players_[p];
    if (tier != pl.streakTier) {
        Event ev;
        ev.type = EventType::StreakTier;
        ev.a = static_cast<u8>(p);
        ev.b = tier;
        ev.c = tier > pl.streakTier ? 1 : 0;
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        emit(ev);
        pl.streakTier = tier;
    }
}

void World::songMiss(u32 p) {
    // Rule 8: a gate faded unthreaded takes the streak down to the start of the tier below, at most once a bar.
    const ModesData::Songline& d = species_->modes.songline;
    const u32 bar = barOf(m1_ + 1.0);
    if (streakDropBar_[p] == bar) return;
    streakDropBar_[p] = bar;
    const u8 tier = players_[p].streakTier;
    const u16 to = tier >= 2 ? d.tiers[tier - 2u] : 0;
    Event ev;
    ev.type = EventType::GateMiss;
    ev.a = static_cast<u8>(p);
    emit(ev);
    if (to < streak_[p]) setStreak(p, to);
}

void World::threadGates(u32 p) {
    // Rules 6 and 7: a gate threaded when the ship's path this tick passes within its ring: 10 x the Chain (the Break's 30, gold's 100
    // with its Boost), x 2 in time (the Spark within inTimeWu), one more to the Chain and the streak.
    const ModesData::Songline& d = species_->modes.songline;
    PlayerState& pl = players_[p];
    const Vec2 a = pl.prevPos, b = pl.pos, ab = b - a;
    const float l2 = lengthSq(ab);
    const bool inTime = lengthSq(spark_.pos - pl.pos) <= d.inTimeWu * d.inTimeWu;
    const double life = static_cast<double>(d.lifeBars) * kTicksPerBar;
    for (SongGate& g : songGates_) {
        if (g.born < 0.0 || g.born > m1_ || m1_ - g.born > life || g.missed) continue;
        if (g.string != 0xFF && g.string != p) continue;
        if ((g.threaded >> p) & 1u) continue;
        if (setup_.versus && g.owner != 0xFF) continue;
        // (versus: the underdog threads from half as far again; V-rules, README.md 8)
        const float r = gateRadius(g) * (setup_.versus && director_.underdog == p ? 1.5f : 1.0f);
        const float u = l2 > 1e-6f ? clampf(dot(g.pos - a, ab) / l2, 0.0f, 1.0f) : 0.0f;
        if (lengthSq(a + ab * u - g.pos) > r * r) continue;
        g.threaded = static_cast<u8>(g.threaded | (1u << p));
        if (setup_.versus) {
            // (first come: the host gives it at once to its own player's claim; a client's is a guess until the host's word)
            g.owner = static_cast<u8>(p);
            g.ownerInTime = inTime;
            g.takeTick = m1_;
            if (role_ != NetRole::Client) songDecide(g, p, inTime, m1_);
        }
        if (inTime) g.inTime = static_cast<u8>(g.inTime | (1u << p));
        const float pay = (g.kind == 2 ? d.goldScore : (g.kind == 1 ? d.breakScore : d.score)) * (inTime ? d.inTimeScale : 1.0f) *
                          (pl.streakTier >= 1 ? 1.5f : 1.0f);
        if (isLocal(p)) pl.score += static_cast<u64>(std::llround(static_cast<double>(pay) * static_cast<double>(pl.chain)));
        pl.chain += 1;
        if (pl.chain > pl.bestChain) pl.bestChain = pl.chain;
        if (g.kind == 2 && g.boost < kBoostCount) activateBoost(p, g.boost);
        ++gatesThreaded_[p];
        if (inTime) ++gatesInTime_[p];
        setStreak(p, static_cast<u16>(streak_[p] < 0xFFFF ? streak_[p] + 1u : 0xFFFFu));
        Event ev;
        ev.type = EventType::GateThread;
        ev.a = static_cast<u8>(p);
        ev.b = g.note;
        ev.c = inTime ? 1 : 0;
        ev.f = static_cast<float>(g.kind);
        ev.x = g.pos.x;
        ev.y = g.pos.y;
        ev.y2 = static_cast<float>(m1_ - g.born);  // (the claim's tick after the gate's birth: the wire's)
        ev.tick = g.born;
        emit(ev);
        if (!setup_.versus && role_ == NetRole::Host) songChordCheck(g.born);
    }
}

// ---------------------------------------------------------------------------------------------- together (songline.md "Co-op",
// "Versus: the race", "Caps and the wire")

SongGate* World::gateOf(double born, u8 string) {
    for (SongGate& g : songGates_)
        if (g.born >= 0.0 && std::fabs(g.born - born) < 0.01 && g.string == string) return &g;
    return nullptr;
}

void World::songRemoteThread(u32 p, double born, bool inTime, double at) {
    // Another device's player threaded a gate (kPeGate): co-op, its string's gate is gone here too and the host looks for the
    // step's chord; versus, the host decides who has it (a client waits for the host's word).
    if (p >= kMaxPlayers) return;
    if (setup_.versus) {
        if (role_ != NetRole::Host) return;
        SongGate* g = gateOf(born, 0xFF);
        if (g) songDecide(*g, p, inTime, at);
        return;
    }
    SongGate* g = gateOf(born, static_cast<u8>(p));
    if (!g) return;
    g->threaded = static_cast<u8>(g->threaded | (1u << p));
    if (inTime) g->inTime = static_cast<u8>(g->inTime | (1u << p));
    if (role_ == NetRole::Host) songChordCheck(born);
}

void World::songDecide(SongGate& g, u32 p, bool inTime, double at) {
    // Versus, the host (or a world alone): the first claim by its tick has the gate; a later one that held it gives it back.
    if (g.decided) {
        if (g.owner == p) return;
        if (g.takeTick <= at) return;  // (an earlier claim stands)
    }
    songOwner(g.born, p, inTime);
    g.takeTick = at;
    if (role_ == NetRole::Host) logArena(13, static_cast<u8>(p), static_cast<u8>(inTime ? 1 : 0), g.pos, Vec2{}, g.born);
}

void World::songOwner(double born, u32 p, bool inTime) {
    // Versus: the gate is p's (the host's word, or its own decision): it turns p's colour and is gone for the others; a player
    // that had it (a guess here, or an earlier word) gives back its count, and this device's own player its gate.
    SongGate* gp = gateOf(born, 0xFF);
    if (!gp || p >= kMaxPlayers) return;
    SongGate& g = *gp;
    const u8 was = g.decided ? g.owner : static_cast<u8>(0xFF);
    if (was == p && g.decided) return;
    if (was < kMaxPlayers) {
        const u16 back = g.ownerInTime ? 2 : 1;
        songTaken_[was] = static_cast<u16>(songTaken_[was] > back ? songTaken_[was] - back : 0);
    }
    for (u32 q = 0; q < playerCount_; ++q) {
        if (q == p || !((g.threaded >> q) & 1u) || !isLocal(q)) continue;
        // (this device's player had it: its gate, its count and its time given back; its streak and score stay: no miss either)
        g.threaded = static_cast<u8>(g.threaded & ~(1u << q));
        if (gatesThreaded_[q]) --gatesThreaded_[q];
        if (((g.inTime >> q) & 1u) && gatesInTime_[q]) --gatesInTime_[q];
        g.inTime = static_cast<u8>(g.inTime & ~(1u << q));
    }
    g.owner = static_cast<u8>(p);
    g.ownerInTime = inTime;
    g.decided = true;
    g.threaded = static_cast<u8>(g.threaded | (1u << p));
    songTaken_[p] = static_cast<u16>(songTaken_[p] + (inTime ? 2u : 1u));
    Event ev;
    ev.type = EventType::GateOwner;
    ev.a = static_cast<u8>(p);
    ev.b = was;
    ev.c = inTime ? 1 : 0;
    ev.x = g.pos.x;
    ev.y = g.pos.y;
    ev.tick = g.born;
    emit(ev);
}

void World::songChordCheck(double born) {
    // Co-op, the host: every player present has threaded their gate of this step: a chord (each Chain one more, the step white).
    u32 present = 0;
    for (u32 q = 0; q < playerCount_; ++q) {
        if (!players_[q].present) continue;
        ++present;
        const SongGate* g = gateOf(born, static_cast<u8>(q));
        if (!g || g->chord || !((g->threaded >> q) & 1u)) return;
    }
    if (present < 2) return;
    const SongGate* g0 = nullptr;
    for (u32 q = 0; q < playerCount_ && !g0; ++q)
        if (players_[q].present) g0 = gateOf(born, static_cast<u8>(q));
    logArena(12, 0, 0, g0 ? g0->pos : Vec2{}, Vec2{}, born);
    songChord(born);
}

void World::songChord(double born) {
    const SongGate* at = nullptr;
    for (SongGate& g : songGates_)
        if (g.born >= 0.0 && std::fabs(g.born - born) < 0.01 && g.string != 0xFF) {
            if (g.chord) return;
            g.chord = true;
            if (!at) at = &g;
        }
    ++counters_.songChords;
    for (u32 q = 0; q < playerCount_; ++q) {
        PlayerState& pl = players_[q];
        if (!pl.present || !isLocal(q)) continue;
        pl.chain += 1;
        if (pl.chain > pl.bestChain) pl.bestChain = pl.chain;
    }
    Event ev;
    ev.type = EventType::SongChord;
    ev.x = at ? at->pos.x : spark_.pos.x;
    ev.y = at ? at->pos.y : spark_.pos.y;
    ev.tick = born;
    emit(ev);
}

void World::songBack(bool back, Vec2 to) {
    // Rule 4 as the host decided it (a client): the line bends back from here, or flies on.
    if (back == spark_.comeBack) return;
    spark_.comeBack = back;
    spark_.comeTo = to;
    spark_.aheadWall[0] = spark_.aheadHome[0] = 0.0f;
    sparkRebuild();
    Event ev;
    ev.type = EventType::SparkBack;
    ev.a = back ? 1 : 0;
    ev.x = spark_.pos.x;
    ev.y = spark_.pos.y;
    emit(ev);
}

void World::updateSongline() {
    if (!species_) return;
    const ModesData::Songline& d = species_->modes.songline;
    // Rule 4 (the host's, or solo's): every ship lost beyond comeBackWu bends the line back toward the nearest until one is within
    // comeBackUntilWu.
    if (role_ != NetRole::Client) {
        float nearest = 1e30f;
        Vec2 to{};
        bool any = false;
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (!pl.present || (!pl.alive && !pl.knocked)) continue;
            const float dd = length(pl.pos - spark_.pos);
            any = true;
            if (dd < nearest) {
                nearest = dd;
                to = pl.pos;
            }
        }
        const bool lost = any && nearest > d.comeBackWu;
        const bool found = !any || nearest < d.comeBackUntilWu;
        if ((lost && !spark_.comeBack) || (spark_.comeBack && found)) {
            spark_.comeBack = lost;
            spark_.comeTo = to;
            spark_.aheadWall[0] = spark_.aheadHome[0] = 0.0f;  // (the walls' way round chosen again, the homing's side first)
            sparkRebuild();
            if (role_ == NetRole::Host) logArena(14, static_cast<u8>(lost ? 1 : 0), 0, to, Vec2{}, static_cast<double>(spark_.step0));
            Event ev;
            ev.type = EventType::SparkBack;
            ev.a = spark_.comeBack ? 1 : 0;
            ev.x = spark_.pos.x;
            ev.y = spark_.pos.y;
            emit(ev);
        } else if (spark_.comeBack) {
            spark_.comeTo = to;
        }
    } else if (spark_.comeBack) {
        // (a client bends toward the nearest ship as it sees them; the host's word turns it on and off, its sync puts it right)
        float nearest = 1e30f;
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (!pl.present || (!pl.alive && !pl.knocked)) continue;
            const float dd = lengthSq(pl.pos - spark_.pos);
            if (dd < nearest) {
                nearest = dd;
                spark_.comeTo = pl.pos;
            }
        }
    }
    // The line moves on a sixteenth at a time; each sixteenth passed lays its gates.
    while (m1_ >= songStepTick(spark_.step0 + 1u)) {
        for (u32 k = 0; k < 16; ++k) {
            spark_.ahead[k] = spark_.ahead[k + 1u];
            spark_.aheadHeading[k] = spark_.aheadHeading[k + 1u];
            spark_.aheadFlown[k] = spark_.aheadFlown[k + 1u];
            spark_.aheadWall[k] = spark_.aheadWall[k + 1u];
            spark_.aheadHome[k] = spark_.aheadHome[k + 1u];
        }
        ++spark_.step0;
        sparkPoint(16);
        if (role_ == NetRole::Host && spark_.step0 % 16u == 0) {
            // (the line a bar ahead, for the clients: the director's message carries it once a bar)
            sparkSync_.sparkStep = static_cast<u16>(spark_.step0 + 16u < 0xFFFFu ? spark_.step0 + 16u : 0xFFFEu);
            sparkSync_.sparkX = spark_.ahead[16].x;
            sparkSync_.sparkY = spark_.ahead[16].y;
            sparkSync_.sparkHeading = spark_.aheadHeading[16];
            sparkSync_.sparkFlown = static_cast<float>(spark_.aheadFlown[16]);
            sparkSync_.sparkBack = spark_.comeBack ? 1 : 0;
        }
        u32 total = 0;
        for (u32 j = 0; j < director_.formCount; ++j) total += director_.formBars[j];
        spark_.spiral = total && spark_.step0 / 16u + d.finaleBars >= total;
        layGates(spark_.step0, spark_.ahead[0], spark_.aheadHeading[0]);
    }
    const double u = clampf(static_cast<float>((m1_ - songStepTick(spark_.step0)) / kSixteenth), 0.0f, 1.0f);
    spark_.prevPos = spark_.pos;
    spark_.pos = spark_.ahead[0] + (spark_.ahead[1] - spark_.ahead[0]) * static_cast<float>(u);
    spark_.heading = angleOf(spark_.ahead[1] - spark_.ahead[0]);
    // What the line has left behind (the host's, once a beat): a body further than 2,000 wu from every ship dissolves (no Shards, no
    // score), as the set's stray fodder does; the line moves on faster than its hunters can follow.
    if (role_ != NetRole::Client && crossed(m0_, m1_, kTicksPerBeat)) {
        Enemies& e = enemies_;
        for (u32 i = 0; i < e.count; ++i) {
            if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping || (e.traits[i] & kEliteBit)) continue;
            bool near = false;
            for (u32 p = 0; p < playerCount_ && !near; ++p)
                near = players_[p].present && lengthSq(players_[p].pos - Vec2{e.x[i], e.y[i]}) < 2000.0f * 2000.0f;
            if (near || lengthSq(spark_.pos - Vec2{e.x[i], e.y[i]}) < 2000.0f * 2000.0f) continue;
            e.flags[i] = static_cast<u8>(e.flags[i] | kFlagDead);
            Event ev;
            ev.type = EventType::Kill;
            ev.species = e.species[i];
            ev.b = 0xFF;
            ev.c = 1;
            ev.x = e.x[i];
            ev.y = e.y[i];
            emit(ev);
            if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
        }
    }
    // Each device's own players thread their gates; a gate past its life is a miss for each whose gate it was and who did not
    // thread it (versus: a gate nobody took).
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].present && players_[p].alive && isLocal(p)) threadGates(p);
    const double life = static_cast<double>(d.lifeBars) * kTicksPerBar, gone = life + static_cast<double>(d.fadeBeats) * kTicksPerBeat;
    for (SongGate& g : songGates_) {
        if (g.born < 0.0) continue;
        if (!g.missed && m1_ - g.born > life) {
            g.missed = true;
            if (g.kind != 2)  // (a gold gate passed by is no miss)
                for (u32 p = 0; p < playerCount_; ++p) {
                    if (!players_[p].present || !isLocal(p) || ((g.threaded >> p) & 1u)) continue;
                    if (g.string != 0xFF && g.string != p) continue;
                    if (setup_.versus && g.owner != 0xFF) continue;
                    songMiss(p);
                }
        }
        if (m1_ - g.born > gone) g.born = -1.0;
    }
}

void World::songFormation(u16 species, u32 count, double landTick, bool crowd) {
    // Rule 14: the crowd comes along the line: formations land 600 to 1,200 wu ahead of the Spark, as rings round coming gates,
    // walls across the line and escorts beside it (spawnFormation keeps every body 260 wu from a ship).
    const ModesData::Songline& d = species_->modes.songline;
    const u32 first = enemies_.count;
    const float want = rng_.range(d.aheadWu[0], d.aheadWu[1]);
    // The point of the line that far ahead (its end if the bar ahead is shorter: the rest straight on from there).
    Vec2 at = spark_.ahead[16];
    float head = spark_.aheadHeading[16];
    float gone = 0.0f;
    for (u32 k = 1; k <= 16; ++k) {
        const float seg = length(spark_.ahead[k] - spark_.ahead[k - 1u]);
        if (gone + seg >= want) {
            const float u = seg > 1e-3f ? (want - gone) / seg : 0.0f;
            at = spark_.ahead[k - 1u] + (spark_.ahead[k] - spark_.ahead[k - 1u]) * u;
            head = spark_.aheadHeading[k];
            gone = want;
            break;
        }
        gone += seg;
    }
    if (gone < want) at = at + dirOf(head) * (want - gone);
    const Vec2 dir = dirOf(head), side = perpL(dir);
    const float r = rng_.uniform();
    for (u32 k = 0; k < count; ++k) {
        Vec2 p{};
        if (r < 0.4f) {
            const float a = kTau * static_cast<float>(k) / static_cast<float>(count > 0 ? count : 1u) + rng_.range(0.0f, 0.3f);
            p = at + dirOf(a) * (120.0f + 8.0f * static_cast<float>(count));  // a ring round the coming gates
        } else if (r < 0.7f) {
            const float span = std::fmin(40.0f * static_cast<float>(count), 520.0f);
            p = at + side * (-0.5f * span + span * (static_cast<float>(k) + 0.5f) / static_cast<float>(count));  // a wall across the line
        } else {
            const float s = (k & 1u) ? 1.0f : -1.0f;
            p = at + side * (s * 260.0f) - dir * (60.0f * static_cast<float>(k / 2u));  // escorts on both sides
        }
        p = Vec2{clampf(p.x, -halfW() + 60.0f, halfW() - 60.0f), clampf(p.y, -halfH() + 60.0f, halfH() - 60.0f)};
        spawnFormation(species, 1, p, landTick);
    }
    if (crowd)
        for (u32 i = first; i < enemies_.count; ++i) enemies_.flags[i] |= kFlagArriving;
}

}  // namespace rf::sim
