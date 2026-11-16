// M8 part 4 (design/modes/tracers.md; docs/MODES_PLAN.md 2.3): TRACERS on the Stage. The ship leaves a tracer, a line that lives a
// few beats; where the line meets itself a ring locks, everything inside is caught and pops on the next eighth line, and the floor's
// tiles inside light in the ring's owner's colour; a floor nine tenths lit is a full house: it comes down and the next begins. There
// is no gun. Every function here runs only in TRACERS (World::tick calls updateTracers); the numbers are modes.json's `tracers`.

#include <algorithm>
#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
float cross2(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
// Where segments p0-p1 and q0-q1 cross (true), and the parameter along p.
bool segCross(Vec2 p0, Vec2 p1, Vec2 q0, Vec2 q1, float& t) {
    const Vec2 r = p1 - p0, s = q1 - q0;
    const float den = cross2(r, s);
    if (std::fabs(den) < 1e-6f) return false;
    const Vec2 qp = q0 - p0;
    t = cross2(qp, s) / den;
    const float u = cross2(qp, r) / den;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}
// Even-odd: a point inside a ring flown either way round is inside (tracers.md "What to check": both ways light the same tiles).
bool insidePoly(const Vec2* v, u32 n, Vec2 p) {
    bool in = false;
    for (u32 i = 0, j = n - 1; i < n; j = i++)
        if (((v[i].y > p.y) != (v[j].y > p.y)) && (p.x < (v[j].x - v[i].x) * (p.y - v[i].y) / (v[j].y - v[i].y) + v[i].x)) in = !in;
    return in;
}
i64 sixteenthOf(double tick) { return static_cast<i64>(std::floor(tick / (kTicksPerBeat / 4.0))); }
}  // namespace

Vec2 World::tileCentre(u32 t) const {
    const float tw = species_ ? species_->modes.stage.tileWu : 40.0f;
    const u32 x = t % TracerFloor::kTilesX, y = t / TracerFloor::kTilesX;
    return Vec2{-halfW() + tw * (static_cast<float>(x) + 0.5f), -halfH() + tw * (static_cast<float>(y) + 0.5f)};
}

void World::resetTracers() {
    for (TracerLine& t : tracers_) t = TracerLine{};
    for (TracerRing& r : rings_) r = TracerRing{};
    floor_ = TracerFloor{};
    for (u8& o : floor_.owner) o = TracerFloor::kDark;
    for (u16& b : bestRing_) b = 0;
    // The elites begin with the form's second Groove (tracers.md rule 14).
    tracersEliteFrom_ = 0;
    u32 grooves = 0, at = 0;
    for (u32 k = 0; k < director_.formCount; ++k) {
        if (static_cast<FormKind>(director_.formKind[k]) == FormKind::Groove && ++grooves == 2) {
            tracersEliteFrom_ = at;
            break;
        }
        at += director_.formBars[k];
    }
}

double World::tracerLifeTicks(u32 p) const {
    // Rule 4: its life in beats, a beat more a Shard level to the most; LONG doubles it (rule 15); the versus underdog's is 4 beats
    // longer (tracers.md "Versus: turf").
    const ModesData::Tracers& d = species_->modes.tracers;
    const PlayerState& pl = players_[p < kMaxPlayers ? p : 0];
    float beats = std::fmin(d.lifeBeats + d.lifeBeatsPerLevel * static_cast<float>(pl.level), d.lifeBeatsMax);
    if (gel(p, kGelLong)) beats *= d.longScale;
    if (setup_.versus && director_.underdog == p) beats += 4.0f;
    return static_cast<double>(beats) * kTicksPerBeat;
}

void World::breakTracer(u32 p) {
    // Rule 3: a hit (or the knock-out, or a versus spin-out) puts the whole tracer out.
    TracerLine& t = tracers_[p];
    t.count = 0;
    t.head = 0;
    t.laying = false;
}

void World::updateTracers() {
    if (!species_) return;
    for (u32 p = 0; p < playerCount_; ++p) layTracer(p);
    // ECHO (rule 15): a ring popped under the gel pops again a beat later, half as large again about its centre, catching what
    // has walked in.
    const ModesData::Tracers& d = species_->modes.tracers;
    for (TracerRing& r : rings_) {
        if (!r.active || !r.popped || r.echo || !gel(r.owner, kGelEcho)) continue;
        const double at = r.popTick + static_cast<double>(d.echoBeats) * kTicksPerBeat;
        if (m0_ >= at || m1_ < at) continue;
        Vec2 poly[TracerRing::kCorners];
        for (u32 k = 0; k < r.n; ++k) {
            const Vec2 v = r.centre + (r.pt[k] - r.centre) * d.echoScale;
            poly[k] = Vec2{clampf(v.x, -halfW(), halfW()), clampf(v.y, -halfH(), halfH())};
        }
        lockRing(r.owner, r.owner2, poly, r.n, true, at);
    }
    // Rule 8: a locked ring pops on its eighth line; a popped ring is kept two beats for the picture, then its slot is free.
    for (TracerRing& r : rings_) {
        if (!r.active) continue;
        if (!r.popped && m1_ >= r.popTick) popRing(r);
        else if (r.popped && m1_ > r.popTick + 2.0 * kTicksPerBeat) r.active = false;
    }
    houseStep();
}

void World::layTracer(u32 p) {
    // Rules 1 and 2: laid while the ship moves faster than minSpeedWu (a Dash straight through), a point every pointEveryWu; each
    // point lives tracerLifeTicks and goes out from the tail. A sixteenth heard while it is laid is a node (the lead's note).
    const ModesData::Tracers& d = species_->modes.tracers;
    PlayerState& pl = players_[p];
    TracerLine& t = tracers_[p];
    if (!pl.present || !pl.alive) {
        breakTracer(p);
        return;
    }
    const double life = tracerLifeTicks(p);
    while (t.count && m1_ - t.born[t.head] > life) {
        t.head = static_cast<u16>((t.head + 1u) % TracerLine::kMax);
        --t.count;
    }
    if (!t.count) t.head = 0;
    t.laying = pl.dazzledUntil <= m1_ && (pl.dashing || length(pl.vel) > d.minSpeedWu);
    if (!t.laying) return;
    const bool node = sixteenthOf(m0_) != sixteenthOf(m1_);
    if (t.count) {
        const u32 newest = (t.head + t.count - 1u) % TracerLine::kMax;
        if (lengthSq(pl.pos - t.pt[newest]) < d.pointEveryWu * d.pointEveryWu) {
            if (node) t.node[newest] = 1;
            return;
        }
    }
    if (t.count >= d.maxPoints) {
        t.head = static_cast<u16>((t.head + 1u) % TracerLine::kMax);
        --t.count;
    }
    const u32 slot = (t.head + t.count) % TracerLine::kMax;
    t.pt[slot] = pl.pos;
    t.born[slot] = m1_;
    t.node[slot] = node ? 1 : 0;
    ++t.count;
    // A ring is its owner's to close (each device its own players'; tracers.md "Caps and the wire").
    if (isLocal(p) && t.count > d.skipNewest + 1u) closeRing(p);
}

bool World::closingHint(u32 p, Vec2& at, float& dist) const {
    // Rule 5's hint: the nearest place the tracer could close on, from hintWu away (the older points; not the newest six).
    if (!species_ || p >= kMaxPlayers) return false;
    const ModesData::Tracers& d = species_->modes.tracers;
    const TracerLine& t = tracers_[p];
    const Vec2 ship = players_[p].pos;
    float best = d.hintWu * d.hintWu;
    bool found = false;
    for (u32 k = 0; k + d.skipNewest < t.count; ++k) {
        const float dd = lengthSq(t.at(k) - ship);
        if (dd <= best) {
            best = dd;
            at = t.at(k);
            found = true;
        }
    }
    dist = found ? std::sqrt(best) : 0.0f;
    return found;
}

void World::closeRing(u32 p) {
    // Rule 5: the newest piece crossing an older one (the newest six points never count), or the ship within the snap of an older
    // point. Rule 6: the line from the meeting place round to the ship is the ring; a new tracer starts from the ship.
    const ModesData::Tracers& d = species_->modes.tracers;
    TracerLine& t = tracers_[p];
    const u32 n = t.count;
    const Vec2 a = t.at(n - 2u), b = t.at(n - 1u);
    i32 meet = -1;
    Vec2 cross{};
    for (u32 k = n - d.skipNewest - 1u; k-- > 0;) {  // (the newest crossing first: the loop just flown)
        float u = 0.0f;
        if (segCross(a, b, t.at(k), t.at(k + 1u), u)) {
            meet = static_cast<i32>(k + 1u);
            cross = a + (b - a) * u;
            break;
        }
    }
    bool snap = false;
    if (meet < 0) {
        float best = d.snapWu * d.snapWu;
        for (u32 k = 0; k + d.skipNewest < n; ++k) {
            const float dd = lengthSq(b - t.at(k));
            if (dd <= best) {
                best = dd;
                meet = static_cast<i32>(k);
                snap = true;
            }
        }
    }
    if (meet < 0) return;
    Vec2 poly[TracerLine::kMax + 1];
    u32 m = 0;
    if (!snap) poly[m++] = cross;
    for (u32 k = static_cast<u32>(meet); k < n; ++k) poly[m++] = t.at(k);
    // A ring smaller than minTiles fizzles: one dry tick (one a beat at most), and the line flies on as it was (a wiggle never
    // costs the ring being flown round it; M8 part 4's reading of rule 6, PROGRESS).
    if (ringTiles(poly, m) < d.minTiles) {
        if (m1_ - t.fizzleTick >= kTicksPerBeat) {
            t.fizzleTick = m1_;
            Event ev;
            ev.type = EventType::RingLock;
            ev.a = static_cast<u8>(p);
            ev.c = 1;
            ev.x = players_[p].pos.x;
            ev.y = players_[p].pos.y;
            ev.tick = m1_;
            emit(ev);
        }
        return;
    }
    // The tail behind the meeting place is cut loose; the new tracer starts from the ship.
    const Vec2 ship = players_[p].pos;
    breakTracer(p);
    t.pt[0] = ship;
    t.born[0] = m1_;
    t.node[0] = 0;
    t.count = 1;
    const double eighth = kTicksPerBeat / 2.0;
    const double pop = std::ceil(m1_ / eighth - 1e-9) * eighth;
    lockRing(static_cast<u8>(p), 0xFF, poly, m, false, pop > m1_ ? pop : m1_ + eighth);
}

u32 World::ringTiles(const Vec2* poly, u32 m) const {
    // The tiles a ring would light (its corners thinned as lockRing thins them), for the fizzle's test.
    const ModesData::Tracers& d = species_->modes.tracers;
    const u32 maxV = d.maxVertices < TracerRing::kCorners ? d.maxVertices : TracerRing::kCorners;
    Vec2 v[TracerRing::kCorners];
    u32 nv = 0;
    if (m <= maxV) {
        for (u32 k = 0; k < m; ++k) v[nv++] = poly[k];
    } else {
        for (u32 k = 0; k < maxV; ++k) v[nv++] = poly[(k * (m - 1u) + (maxV - 1u) / 2u) / (maxV - 1u)];
    }
    if (nv < 3) return 0;
    Vec2 lo = v[0], hi = v[0];
    for (u32 k = 1; k < nv; ++k) {
        lo = Vec2{std::fmin(lo.x, v[k].x), std::fmin(lo.y, v[k].y)};
        hi = Vec2{std::fmax(hi.x, v[k].x), std::fmax(hi.y, v[k].y)};
    }
    u32 tiles = 0;
    for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
        const Vec2 c = tileCentre(tl);
        if (c.x >= lo.x && c.x <= hi.x && c.y >= lo.y && c.y <= hi.y && insidePoly(v, nv, c)) ++tiles;
    }
    return tiles;
}

bool World::lockRing(u8 owner, u8 owner2, const Vec2* poly, u32 m, bool echo, double popTick) {
    const ModesData::Tracers& d = species_->modes.tracers;
    const u32 maxV = d.maxVertices < TracerRing::kCorners ? d.maxVertices : TracerRing::kCorners;
    // The ring as caught and lit: its line thinned to maxVertices corners, evenly along the points (tracers.md "Numbers").
    Vec2 v[TracerRing::kCorners];
    u32 nv = 0;
    if (m <= maxV) {
        for (u32 k = 0; k < m; ++k) v[nv++] = poly[k];
    } else {
        for (u32 k = 0; k < maxV; ++k) v[nv++] = poly[(k * (m - 1u) + (maxV - 1u) / 2u) / (maxV - 1u)];
    }
    Vec2 lo = nv ? v[0] : Vec2{}, hi = lo, sum{};
    for (u32 k = 0; k < nv; ++k) {
        lo = Vec2{std::fmin(lo.x, v[k].x), std::fmin(lo.y, v[k].y)};
        hi = Vec2{std::fmax(hi.x, v[k].x), std::fmax(hi.y, v[k].y)};
        sum = sum + v[k];
    }
    const Vec2 centre = nv ? sum * (1.0f / static_cast<float>(nv)) : Vec2{};
    // Its tiles (rule 9: every tile whose centre is inside).
    u16 inside[TracerFloor::kTiles];
    u32 tiles = 0;
    if (nv >= 3)
        for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
            const Vec2 c = tileCentre(tl);
            if (c.x < lo.x || c.x > hi.x || c.y < lo.y || c.y > hi.y || !insidePoly(v, nv, c)) continue;
            inside[tiles++] = static_cast<u16>(tl);
        }
    Event ev;
    ev.type = EventType::RingLock;
    ev.a = owner;
    ev.tick = popTick;
    // Rule 6: a ring smaller than minTiles fizzles (one dry tick; ECHO's second pop never does).
    TracerRing* slot = nullptr;
    if (nv >= 3 && (echo || tiles >= d.minTiles)) {
        u32 alive = 0;
        for (TracerRing& r : rings_)
            if (r.active) ++alive;
        for (TracerRing& r : rings_)
            if (!r.active && alive < d.maxAlive) {
                slot = &r;
                break;
            }
        if (!slot)  // (all in use: the oldest that has popped gives way)
            for (TracerRing& r : rings_)
                if (r.active && r.popped && (!slot || r.popTick < slot->popTick)) slot = &r;
    }
    if (!slot) {
        ev.c = 1;
        ev.x = players_[owner < kMaxPlayers ? owner : 0].pos.x;
        ev.y = players_[owner < kMaxPlayers ? owner : 0].pos.y;
        emit(ev);
        return false;
    }
    TracerRing& r = *slot;
    r = TracerRing{};
    r.active = true;
    r.echo = echo;
    r.owner = owner;
    r.owner2 = owner2;
    r.n = static_cast<u8>(nv);
    for (u32 k = 0; k < nv; ++k) r.pt[k] = v[k];
    r.centre = centre;
    for (u32 k = 0; k < nv; ++k) r.radius = std::fmax(r.radius, length(v[k] - centre));
    r.lockTick = m1_;
    r.popTick = popTick;
    r.tiles = static_cast<u16>(tiles);
    // Rule 7: every solid body whose centre is inside is caught: it stands still (as under Freeze Frame), takes the owner's colour
    // and hurts nobody. Bodies still warping in are not caught; one already caught pops with its own ring.
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count && r.caught < TracerRing::kCaught; ++i) {
        if ((e.flags[i] & (kFlagDead | kFlagCaught)) || e.state[i] == kWarping) continue;
        if (e.x[i] < lo.x || e.x[i] > hi.x || e.y[i] < lo.y || e.y[i] > hi.y || !insidePoly(v, nv, Vec2{e.x[i], e.y[i]})) continue;
        e.flags[i] = static_cast<u8>(e.flags[i] | kFlagCaught);
        e.frozenUntil[i] = popTick + 1.0;
        r.handle[r.caught++] = e.handle[i];
    }
    r.bonus = static_cast<u8>(d.bonusFor(r.caught) * (owner2 != 0xFF ? static_cast<u32>(d.duetBonus) : 1u));
    // Rule 9 (and versus's repainting): the tiles inside become the owner's (a duet's in a check of the two colours).
    TracerFloor& f = floor_;
    for (u32 k = 0; k < tiles; ++k) {
        const u32 tl = inside[k];
        const u32 x = tl % TracerFloor::kTilesX, y = tl / TracerFloor::kTilesX;
        const u8 who = (owner2 != 0xFF && ((x + y) & 1u)) ? owner2 : owner;
        if (f.owner[tl] == who) continue;
        if (f.owner[tl] == TracerFloor::kDark) ++f.lit;
        f.owner[tl] = who;
        f.order[tl] = ++f.serial;
    }
    f.lastRing = centre;
    if (owner < kMaxPlayers && r.caught > bestRing_[owner]) bestRing_[owner] = r.caught;
    // Rule 11: the floor nine tenths lit is a full house, on the next bar line (no houses in versus: the floor is the score).
    if (!setup_.versus && f.chaseTick < 0.0 && f.popTick < 0.0 && f.darkTick < 0.0 &&
        static_cast<float>(f.lit) >= d.fullHouseShare * static_cast<float>(TracerFloor::kTiles) - 0.5f)
        f.chaseTick = tickOfBar(barOf(m1_ + 1.0) + 1u);
    ev.b = static_cast<u8>(r.caught < 255 ? r.caught : 255);
    ev.f = static_cast<float>(tiles);
    ev.x = centre.x;
    ev.y = centre.y;
    ev.x2 = r.radius;
    emit(ev);
    return true;
}

void World::popRing(TracerRing& r) {
    // Rule 8: every caught body dies, paying the set's score times the Chain times the ring's bonus; an elite loses heavyShare of
    // its full health instead and is let go (four rings, or a Nova: rule 14).
    const ModesData::Tracers& d = species_->modes.tracers;
    r.popped = true;
    const float saved = rules_.scoreMultiplier;
    rules_.scoreMultiplier = saved * static_cast<float>(r.bonus);
    Enemies& e = enemies_;
    u32 popped = 0;
    for (u32 k = 0; k < r.caught; ++k) {
        const i32 i = indexOf(r.handle[k]);
        if (i < 0 || (e.flags[i] & kFlagDead)) continue;
        e.flags[i] = static_cast<u8>(e.flags[i] & ~kFlagCaught);
        if (e.traits[i] & kEliteBit) {
            e.hp[i] -= d.heavyShare * e.hpMax[i];
            e.frozenUntil[i] = m1_;
            if (e.hp[i] > 0.0f) continue;
        }
        killEnemy(static_cast<u32>(i), r.owner);
        ++popped;
    }
    rules_.scoreMultiplier = saved;
    Event ev;
    ev.type = EventType::RingPop;
    ev.a = r.owner;
    ev.b = static_cast<u8>(popped < 255 ? popped : 255);
    ev.c = r.bonus;
    ev.x = r.centre.x;
    ev.y = r.centre.y;
    ev.f = r.radius;
    ev.x2 = r.echo ? 1.0f : 0.0f;
    ev.tick = r.popTick;
    emit(ev);
}

void World::houseStep() {
    // Rule 11, a full house: on its bar line the tiles still dark light by themselves (the chase, two beats); then the whole crowd
    // pops (the set's score, no ring bonus) and the house pays housePayout x its number x the Chain to every player; on the bar
    // line after the chase's the floor goes dark and the next house begins (its crowd and budget houseCrowdStep higher).
    const ModesData::Tracers& d = species_->modes.tracers;
    TracerFloor& f = floor_;
    if (f.chaseTick >= 0.0 && m1_ >= f.chaseTick) {
        const double at = f.chaseTick;
        f.chaseTick = -1.0;
        for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl)
            if (f.owner[tl] == TracerFloor::kDark) {
                f.owner[tl] = TracerFloor::kHouse;
                f.order[tl] = ++f.serial;
            }
        f.lit = static_cast<u16>(TracerFloor::kTiles);
        f.popTick = at + 2.0 * kTicksPerBeat;
        f.darkTick = at + kTicksPerBar;
        Event ev;
        ev.type = EventType::House;
        ev.a = static_cast<u8>(f.house < 255 ? f.house : 255);
        ev.b = 0;
        ev.x = f.lastRing.x;
        ev.y = f.lastRing.y;
        ev.tick = at;
        emit(ev);
    }
    if (f.popTick >= 0.0 && m1_ >= f.popTick) {
        const double at = f.popTick;
        f.popTick = -1.0;
        if (role_ != NetRole::Client) {
            Enemies& e = enemies_;
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
                e.flags[i] = static_cast<u8>(e.flags[i] & ~kFlagCaught);
                const i32 q = nearestPlayer(e.x[i], e.y[i]);
                killEnemy(i, q >= 0 ? static_cast<u8>(q) : static_cast<u8>(0xFF));
            }
        }
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.present || !isLocal(p)) continue;
            pl.score += static_cast<u64>(d.housePayout) * f.house * pl.chain;
        }
        ++f.houses;
        Event ev;
        ev.type = EventType::House;
        ev.a = static_cast<u8>(f.house < 255 ? f.house : 255);
        ev.b = 1;
        ev.tick = at;
        emit(ev);
    }
    if (f.darkTick >= 0.0 && m1_ >= f.darkTick) {
        const double at = f.darkTick;
        f.darkTick = -1.0;
        for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
            f.owner[tl] = TracerFloor::kDark;
            f.order[tl] = 0;
        }
        f.lit = 0;
        f.serial = 0;
        if (f.house < 0xFFFF) ++f.house;
        Event ev;
        ev.type = EventType::House;
        ev.a = static_cast<u8>(f.house < 255 ? f.house : 255);
        ev.b = 2;
        ev.tick = at;
        emit(ev);
    }
}

void World::tracersEnd() {
    // Rule 12: at the last downbeat each tile pays its owner endPerTile x the owner's Chain (the count; screens.md).
    const ModesData::Tracers& d = species_->modes.tracers;
    TracerFloor& f = floor_;
    if (f.chaseTick >= 0.0 || f.popTick >= 0.0) {
        // (a house filled in the run's last bar is a house: it pays as it would have)
        for (u32 p = 0; p < playerCount_; ++p)
            if (players_[p].present && isLocal(p)) players_[p].score += static_cast<u64>(d.housePayout) * f.house * players_[p].chain;
        ++f.houses;
        f.chaseTick = f.popTick = f.darkTick = -1.0;
        Event ev;
        ev.type = EventType::House;
        ev.a = static_cast<u8>(f.house < 255 ? f.house : 255);
        ev.b = 1;
        ev.tick = tickOfBar(phrase_.bar);
        emit(ev);
    }
    u32 lit[kMaxPlayers] = {};
    for (u8 o : f.owner)
        if (o < kMaxPlayers) ++lit[o];
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.present) continue;
        if (isLocal(p)) pl.score += static_cast<u64>(d.endPerTile) * lit[p] * pl.chain;
        Event ev;
        ev.type = EventType::FloorCount;
        ev.a = static_cast<u8>(p);
        ev.f = static_cast<float>(lit[p]);
        ev.tick = tickOfBar(phrase_.bar);
        emit(ev);
    }
}

void World::tracersKnock(u32 p) {
    // Rule 17: a knock-out costs the lights: the oldest knockShare of the player's own lit tiles go dark.
    const ModesData::Tracers& d = species_->modes.tracers;
    TracerFloor& f = floor_;
    u16 orders[TracerFloor::kTiles];
    u32 n = 0;
    for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl)
        if (f.owner[tl] == p) orders[n++] = f.order[tl];
    const u32 lose = static_cast<u32>(std::floor(static_cast<float>(n) * d.knockShare));
    if (!lose) return;
    std::nth_element(orders, orders + (lose - 1u), orders + n);
    const u16 upTo = orders[lose - 1u];
    u32 gone = 0;
    for (u32 tl = 0; tl < TracerFloor::kTiles && gone < lose; ++tl)
        if (f.owner[tl] == p && f.order[tl] <= upTo) {
            f.owner[tl] = TracerFloor::kDark;
            --f.lit;
            ++gone;
        }
    Event ev;
    ev.type = EventType::TilesLost;
    ev.a = static_cast<u8>(p);
    ev.f = static_cast<float>(gone);
    emit(ev);
}

void World::takeGel(u32 p, u8 gel, Vec2 at) {
    const ModesData::Tracers& d = species_->modes.tracers;
    PlayerState& pl = players_[p];
    if (gel >= kGels) return;
    pl.gelUntil[gel] = m1_ + static_cast<double>(d.gelBars[gel]) * kTicksPerBar;
    ++pl.boostsTaken;
    Event ev;
    ev.type = EventType::GelTake;
    ev.a = static_cast<u8>(p);
    ev.b = gel;
    ev.x = at.x;
    ev.y = at.y;
    emit(ev);
}

Vec2 World::stageSpot(float clearWu) {
    // A place on the Stage at least clearWu from every ship standing (README.md 4, Spawning); the arena's stream.
    Vec2 best{};
    float bestD = -1.0f;
    for (u32 tries = 0; tries < 12; ++tries) {
        const Vec2 q{randRange(arenaRng_, -halfW() + 80.0f, halfW() - 80.0f), randRange(arenaRng_, -halfH() + 80.0f, halfH() - 80.0f)};
        float nearest = 1e9f;
        for (u32 p = 0; p < playerCount_; ++p)
            if (players_[p].alive) nearest = std::fmin(nearest, length(q - players_[p].pos));
        if (nearest > bestD) {
            bestD = nearest;
            best = q;
        }
        if (nearest >= clearWu) break;
    }
    return best;
}

bool World::tracersBody(u16 species) const {
    // Rule 13: contact only: variant A of every family, and no Sentries, Tethers or Weavers (nothing fires or lays a lethal line).
    const Species& s = species_->get(species);
    return s.variant == kVariantA && s.family != Family::Sentry && s.family != Family::Tether && s.family != Family::Weaver && !s.shoots();
}

float World::tracersCrowdScale() const {
    const ModesData::Tracers& d = species_->modes.tracers;
    return d.crowdScale * std::pow(1.0f + d.houseCrowdStep, static_cast<float>(floor_.house > 1 ? floor_.house - 1u : 0u));
}

u8 World::boostPick() {
    // The next Boost orb: THE SET's draw from all sixteen (its stream as it always was), or TRACERS's list (rule 15).
    if (setup_.mode == Mode::Tracers && species_ && species_->modes.tracers.boostCount) {
        const ModesData::Tracers& d = species_->modes.tracers;
        const u8 o = d.boosts[upgradeRng_.below(d.boostCount)];
        return (o & kGelOrb) ? static_cast<u8>(kPickupGel + (o & 0x7Fu)) : o;
    }
    return static_cast<u8>(upgradeRng_.below(kBoostCount));
}

}  // namespace rf::sim
