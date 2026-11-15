#include "net/protocol.h"

#include <cmath>
#include <cstring>

#include "core/half.h"
#include "sim/species.h"
#include "sim/upgrades.h"

namespace rf::net {

using namespace rf::sim;

namespace {

constexpr u32 kHeader = 3;  // type, payload length
constexpr float kPosQ = 0.15f;  // wu per step of a correction's position
constexpr float kVelQ = 0.1f;   // wu/s per step of a velocity
constexpr float kKnockQ = 5.0f;
constexpr float kUntilQ = 0.25f;  // musical ticks per step of a correction's state end (+-8,192 ticks)
constexpr float kWorldLimit = 5600.0f;  // positions beyond this are malformed (the arena is +-4800 x +-2700)
constexpr float kDecisionLimit = 1.0e7f;  // a boss decision's values (a place, a pace, a damage, a bar)

float halfToFloat(u16 h) {
    const u32 sign = static_cast<u32>(h & 0x8000u) << 16;
    const u32 exp = (h >> 10) & 0x1Fu;
    u32 man = h & 0x3FFu;
    u32 bits;
    if (exp == 0) {
        if (man == 0) {
            bits = sign;
        } else {
            int e = -1;
            do {
                ++e;
                man <<= 1;
            } while (!(man & 0x400u));
            bits = sign | (static_cast<u32>(127 - 15 - e) << 23) | ((man & 0x3FFu) << 13);
        }
    } else if (exp == 31) {
        bits = sign | 0x7F800000u | (man << 13);
    } else {
        bits = sign | ((exp + 127 - 15) << 23) | (man << 13);
    }
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
u16 angleQ(float a) {
    const float t = a / kTau;
    return static_cast<u16>(static_cast<i32>(std::lround((t - std::floor(t)) * 65536.0f)) & 0xFFFF);
}
float angleF(u16 q) { return static_cast<float>(q) * (kTau / 65536.0f); }
i16 q16(float v, float step) {
    if (!(v == v)) return 0;  // NaN
    const float s = std::nearbyint(v / step);
    return static_cast<i16>(s > 32767.0f ? 32767.0f : (s < -32768.0f ? -32768.0f : s));
}
i8 q8(float v, float step) {
    const float s = std::nearbyint(v / step);
    return static_cast<i8>(s > 127.0f ? 127.0f : (s < -128.0f ? -128.0f : s));
}
bool finite(float v) { return std::isfinite(v); }
bool finite(double v) { return std::isfinite(v); }
bool inWorld(float x, float y) { return finite(x) && finite(y) && std::fabs(x) <= kWorldLimit && std::fabs(y) <= kWorldLimit; }
bool killerOk(u8 k) { return k < kMaxPlayers || k == 0xFF; }

class Reader {
public:
    Reader(const u8* p, u32 n) : p_(p), n_(n) {}
    bool bad() const { return bad_; }
    u32 left() const { return n_ - at_; }
    bool need(u32 k) {
        if (bad_ || n_ - at_ < k) bad_ = true;
        return !bad_;
    }
    u8 u8v() { return need(1) ? p_[at_++] : 0; }
    u16 u16v() {
        if (!need(2)) return 0;
        const u16 v = static_cast<u16>(p_[at_] | (p_[at_ + 1] << 8));
        at_ += 2;
        return v;
    }
    u32 u32v() {
        if (!need(4)) return 0;
        u32 v = 0;
        for (u32 k = 0; k < 4; ++k) v |= static_cast<u32>(p_[at_ + k]) << (8 * k);
        at_ += 4;
        return v;
    }
    u64 u64v() {
        const u64 lo = u32v();
        return lo | (static_cast<u64>(u32v()) << 32);
    }
    i64 i64v() { return static_cast<i64>(u64v()); }
    i16 i16v() { return static_cast<i16>(u16v()); }
    float f32v() {
        const u32 b = u32v();
        float f;
        std::memcpy(&f, &b, sizeof f);
        return f;
    }
    double f64v() {
        const u64 b = u64v();
        double d;
        std::memcpy(&d, &b, sizeof d);
        return d;
    }
    void fail() { bad_ = true; }

private:
    const u8* p_;
    u32 n_, at_ = 0;
    bool bad_ = false;
};

void readPose(Reader& r, PoseState& s) {
    s.x = static_cast<float>(r.i16v()) * kPosQ;
    s.y = static_cast<float>(r.i16v()) * kPosQ;
    s.vx = static_cast<float>(r.i16v()) * kVelQ * 10.0f;  // 1 wu/s steps (a ship moves at most 1,833 wu/s in a Dash)
    s.vy = static_cast<float>(r.i16v()) * kVelQ * 10.0f;
    s.aim = angleF(r.u16v());
    s.heading = angleF(r.u16v());
    s.fire = r.u8v();
    s.flags = r.u8v();
    if (!inWorld(s.x, s.y) || s.fire > 2 || s.flags > 7) r.fail();
}

bool readPoseMsg(Reader& r, MsgPose& m) {
    m.player = r.u8v();
    m.stampUs = r.i64v();
    readPose(r, m.cur);
    m.prevAgeMs = r.i16v();
    readPose(r, m.prev);
    if (m.player >= kMaxPlayers) r.fail();
    return !r.bad();
}

// One message's payload, validated; handed to `sink` when it is not null.
bool parseMessage(Msg type, Reader& r, PacketSink* sink) {
    switch (type) {
        case Msg::Hello: {
            MsgHello m;
            m.version = r.u32v();
            m.content = r.u64v();
            for (char& c : m.name) {
                const u8 b = r.u8v();
                c = (b >= 32 && b < 127) ? static_cast<char>(b) : 0;
            }
            m.name[15] = 0;
            if (r.bad()) return false;
            if (sink) sink->onHello(m);
            return true;
        }
        case Msg::Welcome: {
            MsgWelcome m;
            m.version = r.u32v();
            m.content = r.u64v();
            m.seat = r.u8v();
            m.seats = r.u8v();
            if (r.bad() || m.seats < 1 || m.seats > kMaxPlayers || m.seat >= m.seats) return false;
            if (sink) sink->onWelcome(m);
            return true;
        }
        case Msg::Refuse:
        case Msg::Bye: {
            const u8 reason = r.u8v();
            if (r.bad()) return false;
            if (sink) type == Msg::Refuse ? sink->onRefuse(reason) : sink->onBye(reason);
            return true;
        }
        case Msg::Ping: {
            const i64 t0 = r.i64v();
            if (r.bad()) return false;
            if (sink) sink->onPing(t0);
            return true;
        }
        case Msg::Pong: {
            const i64 t0 = r.i64v(), t1 = r.i64v(), t2 = r.i64v();
            if (r.bad()) return false;
            if (sink) sink->onPong(t0, t1, t2);
            return true;
        }
        case Msg::RunStart: {
            MsgRunStart m;
            m.within = r.u16v();
            m.run = r.u16v();
            m.bar = r.f64v();
            m.seed = r.u64v();
            m.players = r.u8v();
            m.runOffset = r.f64v();
            m.mask = r.u8v();
            if (r.bad() || !finite(m.bar) || !finite(m.runOffset) || m.players < 1 || m.players > kMaxPlayers || m.mask == 0 || m.mask > 0xF)
                return false;
            if (sink) sink->onRunStart(m);
            return true;
        }
        case Msg::Spawns: {
            // Play test 2 (the faster game doubled the crowd's churn): each item is a byte of flags (bit 0 the serial is
            // the last one's + 1, bit 1 the same species, bit 2 the same group, bit 3 the same landing; bits 4-7 the
            // enemy's flags), then only what differs (serial u32, species u8, group u16), always x, y and heading, and
            // the landing when it differs. The first item says everything.
            const u16 run = r.u16v();
            const double base = r.f64v();
            const u32 n = r.u8v();
            if (r.bad() || !finite(base) || n == 0) return false;
            NetSpawn prev;
            i16 landQ = 0;
            for (u32 k = 0; k < n; ++k) {
                const u8 h = r.u8v();
                if (k == 0 && (h & 0x0Fu)) return false;
                NetSpawn s;
                s.serial = (h & 1u) ? prev.serial + 1u : r.u32v();
                s.species = (h & 2u) ? prev.species : static_cast<u16>(r.u8v());
                s.group = (h & 4u) ? prev.group : r.u16v();
                s.x = static_cast<float>(r.i16v()) * kPosQ;
                s.y = static_cast<float>(r.i16v()) * kPosQ;
                s.heading = angleF(r.u16v());
                if (!(h & 8u)) landQ = r.i16v();
                // M5: the enemy's flags as two bits: 4 crowd fodder arriving, 5 called by a mini-boss.
                s.flags = static_cast<u8>(((h & 0x10u) ? kFlagArriving : 0u) | ((h & 0x20u) ? kFlagCalled : 0u));
                s.land = base + static_cast<double>(static_cast<float>(landQ) * kUntilQ);
                if (r.bad() || s.serial == 0 || s.species >= SpeciesTable::kCount || !inWorld(s.x, s.y)) return false;
                if (sink) sink->onSpawn(run, s);
                prev = s;
            }
            return r.left() == 0;
        }
        case Msg::Deaths: {
            // Play test 2: each item a varint (serial delta << 5 | kind << 3 | killer: 0-3 a player, 4 none), in serial
            // order a byte or two (any order still round-trips).
            const u16 run = r.u16v();
            const u32 n = r.u16v();
            if (r.bad() || n == 0) return false;
            u32 prev = 0;
            for (u32 k = 0; k < n; ++k) {
                u64 v = 0;
                u32 bytes = 0;
                u8 b = 0;
                do {
                    b = r.u8v();
                    if (r.bad() || bytes == 6) return false;  // at most 6 bytes (42 bits)
                    v |= static_cast<u64>(b & 0x7Fu) << (7 * bytes);
                    ++bytes;
                } while (b & 0x80u);
                if ((v >> 5) > 0xFFFFFFFFull) return false;
                NetDeath d;
                d.serial = prev + static_cast<u32>(v >> 5);
                d.kind = static_cast<u8>((v >> 3) & 3u);
                const u32 killer = static_cast<u32>(v & 7u);
                if (killer > 4) return false;
                d.killer = killer == 4 ? 0xFF : static_cast<u8>(killer);
                if (d.serial == 0 || !killerOk(d.killer) || d.kind > kDeathShards) return false;
                prev = d.serial;
                if (sink) sink->onDeath(run, d);
            }
            return r.left() == 0;
        }
        case Msg::Claims: {
            // Each item: a varint (serial delta << 3 | kind << 1 | damage follows), then the damage (u16, 1/64ths)
            // when it differs from the last hit's. Hits must carry a damage, kills and dissolves never do.
            const u16 run = r.u16v();
            const u32 n = r.u16v();
            if (r.bad() || n == 0) return false;
            u32 prev = 0;
            float damage = -1.0f;
            for (u32 k = 0; k < n; ++k) {
                u64 v = 0;
                u32 bytes = 0;
                u8 b = 0;
                do {
                    b = r.u8v();
                    if (r.bad() || bytes == 5) return false;  // at most 5 bytes (35 bits)
                    v |= static_cast<u64>(b & 0x7Fu) << (7 * bytes);
                    ++bytes;
                } while (b & 0x80u);
                if ((v >> 3) > 0xFFFFFFFFull) return false;
                NetClaim c;
                c.serial = prev + static_cast<u32>(v >> 3);
                c.kind = static_cast<u8>((v >> 1) & 3u);
                if (c.serial == 0 || c.kind > kClaimDissolve) return false;
                if (v & 1u) {
                    const u16 q = r.u16v();
                    if (r.bad() || q > 0x3FFFu || c.kind != kClaimHit) return false;
                    damage = static_cast<float>(q) * (1.0f / 64.0f);
                }
                if (c.kind == kClaimHit && damage < 0.0f) return false;
                c.damage = c.kind == kClaimHit ? damage : 0.0f;
                prev = c.serial;
                if (sink) sink->onClaim(run, c);
            }
            return true;
        }
        case Msg::BossClaims: {
            const u16 run = r.u16v();
            const u32 n = r.u8v();
            if (r.bad() || n * kBossClaimBytes != r.left()) return false;
            for (u32 k = 0; k < n; ++k) {
                NetBossClaim c;
                c.part = r.u8v();
                c.pair = r.u8v();
                c.damage = r.f32v();
                // (M5 part 5: a hit on an arena body is a claim on part kArenaPartBase + its slot)
                const bool body = c.part >= sim::kArenaPartBase && c.part < sim::kArenaPartBase + 16 && c.pair == 0;
                if (r.bad() || (!body && (c.part < 1 || c.part > 3 || c.pair >= sim::kBossParts)) || !finite(c.damage) || c.damage < 0.0f || c.damage > 1e6f)
                    return false;
                if (sink) sink->onBossClaim(run, c);
            }
            return true;
        }
        case Msg::Boss: {
            MsgBoss m;
            m.run = r.u16v();
            m.kind = r.u8v();
            m.pair = r.u8v();
            m.killer = r.u8v();
            m.boss = r.u8v();
            m.x = r.f32v();
            m.y = r.f32v();
            m.x2 = r.f32v();
            m.y2 = r.f32v();
            m.tick = r.f64v();
            m.changeBar = r.u32v();
            // M5 part 5: the host's arena decisions (pair kArenaOpBase + the operation, its a in oss, its b in killer).
            const bool arena = m.kind == 1 && m.pair >= sim::kArenaOpBase && m.pair < sim::kArenaOpBase + 16;
            const bool geom = m.kind == 1 && m.pair >= sim::kBossGeomBase && m.pair < sim::kBossGeomBase + sim::kGeomCount;
            const bool partOk = m.pair < sim::kBossParts || (m.pair >= sim::kBossStepBase && m.pair < sim::kBossStepBase + 8) || geom || arena;
            // A decision's two values are not always a place (M5 part 7's pace carries a damage, which passed the arena's bounds
            // and took its whole packet with it: spawns and verdicts lost, the census off): finite and bounded for those.
            const auto valueOk = [](float v) { return finite(v) && std::fabs(v) <= kDecisionLimit; };
            const bool placesOk = geom ? valueOk(m.x) && valueOk(m.y) && valueOk(m.x2) && valueOk(m.y2) : inWorld(m.x, m.y) && inWorld(m.x2, m.y2);
            if (r.bad() || m.kind > 2 || !partOk || (!arena && (m.boss >= sim::kBossKinds || !killerOk(m.killer))) || (arena && m.boss >= 32) ||
                !placesOk || !finite(m.tick))
                return false;
            if (sink) sink->onBoss(m);
            return true;
        }
        case Msg::BoostDrop: {
            MsgBoostDrop m;
            m.run = r.u16v();
            m.x = r.f32v();
            m.y = r.f32v();
            m.type = r.u8v();
            if (r.bad() || !inWorld(m.x, m.y) || (m.type >= kBoostCount && !sim::isExtraLifeDrop(m.type))) return false;  // (play test 3: an Extra Life)
            if (sink) sink->onBoostDrop(m);
            return true;
        }
        case Msg::Director: {
            MsgDirector m;
            m.run = r.u16v();
            m.d.startTick = r.f64v();
            m.d.trackBar = r.u32v();
            m.d.changeBar = r.u32v();
            m.d.track = r.u8v();
            m.d.bossSpawned = r.u8v() != 0;
            m.d.assist = r.f32v();
            m.d.midHold = r.u8v();
            m.d.midHolding = r.u8v() != 0;
            m.d.miniSpawned = r.u8v() != 0;
            m.d.lastMini = r.u8v();
            if (r.bad() || !finite(m.d.startTick) || m.d.track < 1 || !finite(m.d.assist) || m.d.assist < 0.1f || m.d.assist > 10.0f)
                return false;
            if (sink) sink->onDirector(m);
            return true;
        }
        case Msg::PlayerEvent: {
            MsgPlayerEvent m;
            m.run = r.u16v();
            m.player = r.u8v();
            m.kind = r.u8v();
            m.b = r.u8v();
            for (u8& o : m.opts) o = r.u8v();
            m.x = r.f32v();
            m.y = r.f32v();
            m.x2 = r.f32v();
            m.y2 = r.f32v();
            m.tick = r.f64v();
            if (r.bad() || m.player >= kMaxPlayers || m.kind >= kPeCount || !inWorld(m.x, m.y) || !inWorld(m.x2, m.y2) || !finite(m.tick))
                return false;
            // The small value by kind (cells, an upgrade, a Boost, hits left, a flag).
            u32 bMax = 255;
            switch (m.kind) {
                case kPeHit: bMax = kMaxCells; break;
                case kPeTrioOpen: bMax = sim::kMaxOrbs; break;
                case kPeTrioTake: bMax = 255; break;  // an upgrade or a Boost orb (play test 2), checked below
                case kPeBoostTake: bMax = m.b == sim::kPickupExtraLife ? 255u : kBoostCount - 1; break;  // (play test 3: an Extra Life)
                case kPeHalo: bMax = 3; break;
                case kPeWell: bMax = sim::kRiffMaxLevel; break;
                case kPeDash:
                case kPeRevive: bMax = 1; break;
                default: bMax = 0; break;
            }
            if (m.b > bMax) return false;
            if (m.kind == kPeTrioTake && !sim::validOrb(m.b)) return false;
            if (m.kind == kPeTrioOpen)
                for (u32 k = 0; k < m.b; ++k)
                    if (!sim::validOrb(m.opts[k])) return false;
            if (sink) sink->onPlayerEvent(m);
            return true;
        }
        case Msg::Census: {
            MsgCensus m;
            m.run = r.u16v();
            m.count = r.u32v();
            m.digest = r.u64v();
            if (r.bad() || m.count > kMaxEnemies) return false;
            if (sink) sink->onCensus(m);
            return true;
        }
        case Msg::Pause: {
            MsgPause m;
            m.kind = r.u8v();
            m.runTick = r.f64v();
            m.songTick = r.f64v();
            m.runOffset = r.f64v();
            m.away = r.u8v();
            if (r.bad() || m.kind > kAwayMask || m.away > 0xF || !finite(m.runTick) || !finite(m.songTick) || !finite(m.runOffset)) return false;
            if (sink) sink->onPause(m);
            return true;
        }
        case Msg::Seats: {
            const u8 mask = r.u8v();
            if (r.bad() || mask > 0xF) return false;
            if (sink) sink->onSeats(mask);
            return true;
        }
        case Msg::Ready: {
            const u8 ready = r.u8v();
            if (r.bad() || ready > 1) return false;
            if (sink) sink->onReady(ready);
            return true;
        }
        case Msg::Lobby: {
            MsgLobby m;
            m.seats = r.u8v();
            m.ready = r.u8v();
            m.code = r.u16v();
            m.state = r.u8v();
            const u8 reserved = r.u8v();
            for (u32 s = 0; s < 4; ++s) {
                bool ended = false;
                for (u32 k = 0; k < 16; ++k) {
                    const u8 c = r.u8v();
                    if (c == 0) {
                        ended = true;
                        continue;
                    }
                    if (ended || c < 0x20 || c > 0x7E) return false;  // printable, then zeros only
                    m.names[s][k] = static_cast<char>(c);
                }
                if (!ended) return false;
            }
            if (r.bad() || reserved != 0 || m.seats > 0xF || (m.ready & ~m.seats) != 0 || m.state > 1) return false;
            if (sink) sink->onLobby(m);
            return true;
        }
        case Msg::Bed: {
            MsgBed m;
            m.run = r.u16v();
            m.bar = r.u32v();
            m.level = r.u8v();
            if (r.bad() || m.bar > (1u << 24)) return false;
            if (sink) sink->onBed(m);
            return true;
        }
        case Msg::Pose: {
            MsgPose m;
            if (!readPoseMsg(r, m)) return false;
            if (sink) sink->onPose(m);
            return true;
        }
        case Msg::Players: {
            const u32 n = r.u8v();
            if (r.bad() || n > kMaxPlayers || n * kPoseBytes != r.left()) return false;
            MsgPose poses[kMaxPlayers];
            for (u32 k = 0; k < n; ++k)
                if (!readPoseMsg(r, poses[k])) return false;
            if (sink)
                for (u32 k = 0; k < n; ++k) sink->onPose(poses[k]);
            return true;
        }
        case Msg::Corrections: {
            const u16 run = r.u16v();
            const double base = r.f64v();
            const u32 n = r.u8v();
            if (r.bad() || !finite(base) || n * kCorrectionBytes != r.left()) return false;
            if (sink) sink->onCorrections(run, base);
            for (u32 k = 0; k < n; ++k) {
                NetEnemyState s;
                s.serial = r.u32v();
                s.x = static_cast<float>(r.i16v()) * kPosQ;
                s.y = static_cast<float>(r.i16v()) * kPosQ;
                s.vx = static_cast<float>(r.i16v()) * kVelQ;
                s.vy = static_cast<float>(r.i16v()) * kVelQ;
                s.kx = static_cast<float>(static_cast<i8>(r.u8v())) * kKnockQ;
                s.ky = static_cast<float>(static_cast<i8>(r.u8v())) * kKnockQ;
                s.heading = angleF(r.u16v());
                s.a = halfToFloat(r.u16v());
                s.b = halfToFloat(r.u16v());
                s.hp = halfToFloat(r.u16v());
                s.until = base + static_cast<double>(r.i16v()) * kUntilQ;
                s.rng = r.u32v();
                const u8 packed = r.u8v();
                s.state = packed & 15u;  // M5: the called bodies' states need four bits
                s.flags = static_cast<u8>(((packed & 16u) ? kFlagEngaged : 0) | ((packed & 32u) ? kFlagArriving : 0) | ((packed & 64u) ? kFlagCalled : 0));
                s.frozen = (packed & 128u) != 0;  // M5 part 3: Freeze Frame
                s.target = r.u8v();
                s.step = r.u8v();  // M5 part 2
                s.far = 0;
                if (r.bad() || s.serial == 0 || !finite(s.a) || !finite(s.b) || !finite(s.hp) || s.state >= kStateCount ||
                    !(s.target < kMaxPlayers || s.target == 0xFF) || s.rng == 0)
                    return false;
                if (sink) sink->onCorrection(s);
            }
            return true;
        }
        case Msg::Motions: {
            const u16 run = r.u16v();
            const double base = r.f64v();
            const u32 n = r.u8v();
            if (r.bad() || !finite(base) || n == 0 || n * 8u > r.left()) return false;
            if (sink) sink->onMotions(run, base);
            u32 prev = 0;
            for (u32 k = 0; k < n; ++k) {
                u32 step = 0;
                for (u32 shift = 0;; shift += 7) {
                    const u8 b = r.u8v();
                    if (r.bad() || shift > 28) return false;
                    step |= static_cast<u32>(b & 0x7Fu) << shift;
                    if (!(b & 0x80u)) break;
                }
                NetMotion m;
                if (step == 0 || step > 0xFFFFFFFFu - prev) return false;  // strictly increasing, no wrap
                m.serial = prev + step;
                prev = m.serial;
                m.x = static_cast<float>(r.i16v()) * kPosQ;
                m.y = static_cast<float>(r.i16v()) * kPosQ;
                m.heading = angleF(r.u16v());
                const u8 bits = r.u8v();
                const u8 t = bits & 7u;
                if (bits & 0xC0u || (t >= kMaxPlayers && t != 7u)) return false;
                m.target = t < kMaxPlayers ? t : 0xFF;
                m.engaged = (bits & 16u) != 0;
                m.frozen = (bits & 32u) != 0;
                if (bits & 8u) {
                    m.kx = static_cast<float>(static_cast<i8>(r.u8v())) * kKnockQ;
                    m.ky = static_cast<float>(static_cast<i8>(r.u8v())) * kKnockQ;
                }
                if (r.bad() || !inWorld(m.x, m.y)) return false;
                if (sink) sink->onMotion(m);
            }
            return true;
        }
        case Msg::Stats: {
            const u32 n = r.u8v();
            if (r.bad() || n < 1 || n > kMaxPlayers || n * kStatsBytes != r.left()) return false;
            MsgStats st[kMaxPlayers];
            for (u32 k = 0; k < n; ++k) {
                MsgStats& m = st[k];
                m.player = r.u8v();
                m.cells = r.u8v();
                m.novas = r.u8v();
                m.chain = r.u32v();
                m.kills = r.u32v();
                m.score = r.u64v();
                // (2026-11-15: the Nova check allowed 6, but Nova Stock's three ranks hold 7; a player with seven dropped every Stats)
                if (r.bad() || m.player >= kMaxPlayers || m.cells > kMaxCells || m.novas > sim::kNovaCapacityMax || m.chain == 0) return false;
            }
            if (sink)
                for (u32 k = 0; k < n; ++k) sink->onStats(st[k]);
            return true;
        }
        case Msg::Attention: {
            const u16 run = r.u16v();
            const u32 n = r.u8v();
            if (r.bad() || n < 1 || n > kMaxAttention || n * 4 != r.left()) return false;
            u32 serials[kMaxAttention];
            for (u32 k = 0; k < n; ++k) {
                serials[k] = r.u32v();
                if (r.bad() || serials[k] == 0) return false;
            }
            if (sink) sink->onAttention(run, serials, n);
            return true;
        }
        case Msg::Heartbeat: {
            MsgHeartbeat m;
            m.run = r.u16v();
            m.hostUs = r.i64v();
            m.song = r.f64v();
            m.tps = r.f32v();
            m.nextBar = r.f64v();
            m.tpsNext = r.f32v();
            m.runOffset = r.f64v();
            m.state = r.u8v();
            m.bossOn = r.u8v();
            m.bossParts = r.u8v();
            if (m.bossParts > sim::kBossParts) return false;
            bool partsOk = true;
            for (u32 k = 0; k < m.bossParts; ++k) {
                m.bossPart[k] = r.f32v();
                partsOk = partsOk && finite(m.bossPart[k]);
            }
            m.bossCore = r.f32v();
            m.bossX = r.f32v();
            m.bossY = r.f32v();
            const auto tpsOk = [](float t) { return finite(t) && t > 1.0f && t < 2000.0f; };
            if (r.bad() || !finite(m.song) || !tpsOk(m.tps) || !finite(m.nextBar) || !tpsOk(m.tpsNext) || !finite(m.runOffset) || m.state > 2 ||
                !partsOk || !finite(m.bossCore) || !inWorld(m.bossX, m.bossY))
                return false;
            if (sink) sink->onHeartbeat(m);
            return true;
        }
    }
    return false;  // an unknown type
}

bool walk(const u8* data, u32 size, PacketSink* sink) {
    u32 at = 0;
    while (at < size) {
        if (size - at < kHeader) return false;
        const Msg type = static_cast<Msg>(data[at]);
        const u32 len = static_cast<u32>(data[at + 1] | (data[at + 2] << 8));
        at += kHeader;
        if (len > size - at) return false;
        Reader r(data + at, len);
        if (!parseMessage(type, r, sink) || r.bad() || r.left() != 0) {
            g_lastBadMessage = static_cast<u8>(type);  // (diagnostics: the soak names what a refused packet held)
            return false;
        }
        at += len;
    }
    return true;
}

}  // namespace

bool parsePacket(const u8* data, u32 size, PacketSink& sink) {
    if (!data || size == 0 || size > kMaxPacketBytes) return false;
    if (!walk(data, size, nullptr)) return false;  // all of it well formed first
    return walk(data, size, &sink);
}

// ---------------------------------------------------------------------------------------------- writing

bool PacketWriter::begin(Msg type, u32 payload) {
    if (size_ + kHeader + payload > kMaxPacketBytes || payload > 0xFFFF) return false;
    u8v(static_cast<u8>(type));
    u16v(static_cast<u16>(payload));
    return true;
}
void PacketWriter::u16v(u16 v) {
    buf_[size_++] = static_cast<u8>(v);
    buf_[size_++] = static_cast<u8>(v >> 8);
}
void PacketWriter::u32v(u32 v) {
    for (u32 k = 0; k < 4; ++k) buf_[size_++] = static_cast<u8>(v >> (8 * k));
}
void PacketWriter::u64v(u64 v) {
    u32v(static_cast<u32>(v));
    u32v(static_cast<u32>(v >> 32));
}
void PacketWriter::f32v(float v) {
    u32 b;
    std::memcpy(&b, &v, sizeof b);
    u32v(b);
}
void PacketWriter::f64v(double v) {
    u64 b;
    std::memcpy(&b, &v, sizeof b);
    u64v(b);
}
void PacketWriter::pose(const PoseState& s) {
    u16v(static_cast<u16>(q16(s.x, kPosQ)));
    u16v(static_cast<u16>(q16(s.y, kPosQ)));
    u16v(static_cast<u16>(q16(s.vx, kVelQ * 10.0f)));
    u16v(static_cast<u16>(q16(s.vy, kVelQ * 10.0f)));
    u16v(angleQ(s.aim));
    u16v(angleQ(s.heading));
    u8v(s.fire);
    u8v(s.flags);
}
void PacketWriter::poseMsg(const MsgPose& m) {
    u8v(m.player);
    u64v(static_cast<u64>(m.stampUs));
    pose(m.cur);
    u16v(static_cast<u16>(m.prevAgeMs));
    pose(m.prev);
}

bool PacketWriter::putHello(const MsgHello& m) {
    if (!begin(Msg::Hello, 28)) return false;
    u32v(m.version);
    u64v(m.content);
    for (char c : m.name) u8v(static_cast<u8>(c));
    return true;
}
bool PacketWriter::putWelcome(const MsgWelcome& m) {
    if (!begin(Msg::Welcome, 14)) return false;
    u32v(m.version);
    u64v(m.content);
    u8v(m.seat);
    u8v(m.seats);
    return true;
}
bool PacketWriter::putRefuse(u8 reason) {
    if (!begin(Msg::Refuse, 1)) return false;
    u8v(reason);
    return true;
}
bool PacketWriter::putBye(u8 reason) {
    if (!begin(Msg::Bye, 1)) return false;
    u8v(reason);
    return true;
}
bool PacketWriter::putPing(i64 t0) {
    if (!begin(Msg::Ping, 8)) return false;
    u64v(static_cast<u64>(t0));
    return true;
}
bool PacketWriter::putPong(i64 t0, i64 t1, i64 t2) {
    if (!begin(Msg::Pong, 24)) return false;
    u64v(static_cast<u64>(t0));
    u64v(static_cast<u64>(t1));
    u64v(static_cast<u64>(t2));
    return true;
}
bool PacketWriter::putRunStart(const MsgRunStart& m) {
    if (!begin(Msg::RunStart, 30)) return false;
    u16v(m.within);
    u16v(m.run);
    f64v(m.bar);
    u64v(m.seed);
    u8v(m.players);
    f64v(m.runOffset);
    u8v(m.mask);
    return true;
}
u32 PacketWriter::putSpawns(u16 run, double base, const NetSpawn* items, u32 n) {
    // Play test 2: only what differs from the item before (see the parser); a formation of the crowd costs 7 bytes an
    // enemy (it was 17).
    const auto landQ = [&](const NetSpawn& s) { return q16(static_cast<float>(s.land - base), kUntilQ); };
    const auto head = [&](u32 k) -> u8 {
        u8 h = static_cast<u8>(((items[k].flags & kFlagArriving) ? 0x10u : 0u) | ((items[k].flags & kFlagCalled) ? 0x20u : 0u));
        if (k == 0) return h;
        const NetSpawn &s = items[k], &p = items[k - 1];
        if (s.serial == p.serial + 1u) h |= 1u;
        if (s.species == p.species) h |= 2u;
        if (s.group == p.group) h |= 4u;
        if (landQ(s) == landQ(p)) h |= 8u;
        return h;
    };
    const auto itemSize = [](u8 h) { return 1u + ((h & 1u) ? 0u : 4u) + ((h & 2u) ? 0u : 1u) + ((h & 4u) ? 0u : 2u) + 6u + ((h & 8u) ? 0u : 2u); };
    if (room() < kHeader + 11 + kSpawnBytes) return 0;
    const u32 budget = room() - kHeader - 11;
    u32 size = 0, fit = 0;
    for (; fit < n && fit < 255u; ++fit) {
        const u32 sz = itemSize(head(fit));
        if (size + sz > budget) break;
        size += sz;
    }
    if (!fit || !begin(Msg::Spawns, 11 + size)) return 0;
    u16v(run);
    f64v(base);
    u8v(static_cast<u8>(fit));
    for (u32 k = 0; k < fit; ++k) {
        const NetSpawn& s = items[k];
        const u8 h = head(k);
        u8v(h);
        if (!(h & 1u)) u32v(s.serial);
        if (!(h & 2u)) u8v(static_cast<u8>(s.species < 255u ? s.species : 255u));
        if (!(h & 4u)) u16v(s.group);
        u16v(static_cast<u16>(q16(s.x, kPosQ)));
        u16v(static_cast<u16>(q16(s.y, kPosQ)));
        u16v(angleQ(s.heading));
        if (!(h & 8u)) u16v(static_cast<u16>(landQ(s)));
    }
    return fit;
}
u32 PacketWriter::putDeaths(u16 run, const NetDeath* items, u32 n) {
    // Play test 2: a varint an item (serial delta << 5 | kind << 3 | killer), a byte or two in serial order.
    const auto code = [](const NetDeath& d, u32 prev) {
        const u64 killer = d.killer < kMaxPlayers ? d.killer : (d.killer == 0xFF ? 4u : 7u);
        return (static_cast<u64>(d.serial - prev) << 5) | (static_cast<u64>(d.kind & 3u) << 3) | killer;
    };
    const auto varSize = [](u64 v) {
        u32 b = 1;
        while (v >= 0x80u) {
            v >>= 7;
            ++b;
        }
        return b;
    };
    if (room() < kHeader + 4 + 1) return 0;
    const u32 budget = room() - kHeader - 4;
    u32 size = 0, fit = 0, prev = 0;
    for (; fit < n && fit < 65535u; ++fit) {
        const u32 sz = varSize(code(items[fit], prev));
        if (size + sz > budget) break;
        size += sz;
        prev = items[fit].serial;
    }
    if (!fit || !begin(Msg::Deaths, 4 + size)) return 0;
    u16v(run);
    u16v(static_cast<u16>(fit));
    prev = 0;
    for (u32 k = 0; k < fit; ++k) {
        u64 v = code(items[k], prev);
        while (v >= 0x80u) {
            u8v(static_cast<u8>(v | 0x80u));
            v >>= 7;
        }
        u8v(static_cast<u8>(v));
        prev = items[k].serial;
    }
    return fit;
}
u32 PacketWriter::putClaims(u16 run, const NetClaim* items, u32 n) {
    // The most frequent message (a Pulse ring claims a crowd at once): in serial order each item costs a byte or two
    // (any order still round-trips). Damage in 1/64ths: every damage the slice deals is a multiple of 1/64.
    const auto quant = [](float dmg) -> u16 {
        const float d = dmg * 64.0f;
        return static_cast<u16>(d <= 0.0f || !(d == d) ? 0u : (d >= 16383.0f ? 16383u : static_cast<u32>(d + 0.5f)));
    };
    const auto varSize = [](u64 v) {
        u32 b = 1;
        while (v >= 0x80u) {
            v >>= 7;
            ++b;
        }
        return b;
    };
    if (room() < kHeader + 4 + 1) return 0;
    const u32 budget = room() - kHeader - 4;
    u32 size = 0, fit = 0, prev = 0;
    i32 lastQ = -1;
    for (; fit < n && fit < 65535u; ++fit) {
        const NetClaim& c = items[fit];
        const bool hit = (c.kind & 3u) == kClaimHit;
        const u16 q = quant(c.damage);
        const bool dmg = hit && static_cast<i32>(q) != lastQ;
        const u64 v = (static_cast<u64>(c.serial - prev) << 3) | (static_cast<u64>(c.kind & 3u) << 1) | (dmg ? 1u : 0u);
        const u32 sz = varSize(v) + (dmg ? 2u : 0u);
        if (size + sz > budget) break;
        size += sz;
        prev = c.serial;
        if (dmg) lastQ = q;
    }
    if (!fit || !begin(Msg::Claims, 4 + size)) return 0;
    u16v(run);
    u16v(static_cast<u16>(fit));
    prev = 0;
    lastQ = -1;
    for (u32 k = 0; k < fit; ++k) {
        const NetClaim& c = items[k];
        const bool hit = (c.kind & 3u) == kClaimHit;
        const u16 q = quant(c.damage);
        const bool dmg = hit && static_cast<i32>(q) != lastQ;
        u64 v = (static_cast<u64>(c.serial - prev) << 3) | (static_cast<u64>(c.kind & 3u) << 1) | (dmg ? 1u : 0u);
        while (v >= 0x80u) {
            u8v(static_cast<u8>(v | 0x80u));
            v >>= 7;
        }
        u8v(static_cast<u8>(v));
        if (dmg) {
            u16v(q);
            lastQ = q;
        }
        prev = c.serial;
    }
    return fit;
}
u32 PacketWriter::putBossClaims(u16 run, const NetBossClaim* items, u32 n) {
    if (room() < kHeader + 3 + kBossClaimBytes) return 0;
    u32 fit = (room() - kHeader - 3) / kBossClaimBytes;
    if (fit > 255) fit = 255;
    if (n > fit) n = fit;
    if (!n || !begin(Msg::BossClaims, 3 + n * kBossClaimBytes)) return 0;
    u16v(run);
    u8v(static_cast<u8>(n));
    for (u32 k = 0; k < n; ++k) {
        u8v(items[k].part);
        u8v(items[k].pair);
        f32v(items[k].damage);
    }
    return n;
}
bool PacketWriter::putBoss(const MsgBoss& m) {
    if (!begin(Msg::Boss, 34)) return false;
    u16v(m.run);
    u8v(m.kind);
    u8v(m.pair);
    u8v(m.killer);
    u8v(m.boss);
    f32v(m.x);
    f32v(m.y);
    f32v(m.x2);
    f32v(m.y2);
    f64v(m.tick);
    u32v(m.changeBar);
    return true;
}
bool PacketWriter::putBoostDrop(const MsgBoostDrop& m) {
    if (!begin(Msg::BoostDrop, 11)) return false;
    u16v(m.run);
    f32v(m.x);
    f32v(m.y);
    u8v(m.type);
    return true;
}
bool PacketWriter::putDirector(const MsgDirector& m) {
    if (!begin(Msg::Director, 28)) return false;
    u16v(m.run);
    f64v(m.d.startTick);
    u32v(m.d.trackBar);
    u32v(m.d.changeBar);
    u8v(m.d.track);
    u8v(m.d.bossSpawned ? 1 : 0);
    f32v(m.d.assist);
    u8v(m.d.midHold);
    u8v(m.d.midHolding ? 1 : 0);
    u8v(m.d.miniSpawned ? 1 : 0);
    u8v(m.d.lastMini);
    return true;
}
bool PacketWriter::putPlayerEvent(const MsgPlayerEvent& m) {
    if (!begin(Msg::PlayerEvent, 29 + sim::kMaxOrbs)) return false;
    u16v(m.run);
    u8v(m.player);
    u8v(m.kind);
    u8v(m.b);
    for (u8 o : m.opts) u8v(o);
    f32v(m.x);
    f32v(m.y);
    f32v(m.x2);
    f32v(m.y2);
    f64v(m.tick);
    return true;
}
bool PacketWriter::putCensus(const MsgCensus& m) {
    if (!begin(Msg::Census, 14)) return false;
    u16v(m.run);
    u32v(m.count);
    u64v(m.digest);
    return true;
}
bool PacketWriter::putPause(const MsgPause& m) {
    if (!begin(Msg::Pause, 26)) return false;
    u8v(m.kind);
    f64v(m.runTick);
    f64v(m.songTick);
    f64v(m.runOffset);
    u8v(m.away);
    return true;
}
bool PacketWriter::putSeats(u8 mask) {
    if (!begin(Msg::Seats, 1)) return false;
    u8v(mask);
    return true;
}
bool PacketWriter::putReady(u8 ready) {
    if (!begin(Msg::Ready, 1)) return false;
    u8v(ready);
    return true;
}
bool PacketWriter::putLobby(const MsgLobby& m) {
    if (!begin(Msg::Lobby, 6 + 64)) return false;
    u8v(m.seats);
    u8v(m.ready);
    u16v(m.code);
    u8v(m.state);
    u8v(0);
    for (u32 s = 0; s < 4; ++s) {
        bool ended = false;
        for (u32 k = 0; k < 16; ++k) {
            char c = ended || k == 15 ? '\0' : m.names[s][k];
            if (c < 0x20 || c > 0x7E) {
                c = '\0';
                ended = true;
            }
            u8v(static_cast<u8>(c));
        }
    }
    return true;
}
bool PacketWriter::putBed(const MsgBed& m) {
    if (!begin(Msg::Bed, 7)) return false;
    u16v(m.run);
    u32v(m.bar);
    u8v(m.level);
    return true;
}
bool PacketWriter::putPose(const MsgPose& m) {
    if (!begin(Msg::Pose, kPoseBytes)) return false;
    poseMsg(m);
    return true;
}
bool PacketWriter::putPlayers(const MsgPose* poses, u32 n) {
    if (n > kMaxPlayers || !begin(Msg::Players, 1 + n * kPoseBytes)) return false;
    u8v(static_cast<u8>(n));
    for (u32 k = 0; k < n; ++k) poseMsg(poses[k]);
    return true;
}
u32 PacketWriter::putCorrections(u16 run, double base, const NetEnemyState* items, u32 n) {
    if (room() < kHeader + 11 + kCorrectionBytes) return 0;
    u32 fit = (room() - kHeader - 11) / kCorrectionBytes;
    if (fit > 255) fit = 255;
    if (n > fit) n = fit;
    if (!n || !begin(Msg::Corrections, 11 + n * kCorrectionBytes)) return 0;
    u16v(run);
    f64v(base);
    u8v(static_cast<u8>(n));
    for (u32 k = 0; k < n; ++k) {
        const NetEnemyState& s = items[k];
        u32v(s.serial);
        u16v(static_cast<u16>(q16(s.x, kPosQ)));
        u16v(static_cast<u16>(q16(s.y, kPosQ)));
        u16v(static_cast<u16>(q16(s.vx, kVelQ)));
        u16v(static_cast<u16>(q16(s.vy, kVelQ)));
        u8v(static_cast<u8>(q8(s.kx, kKnockQ)));
        u8v(static_cast<u8>(q8(s.ky, kKnockQ)));
        u16v(angleQ(s.heading));
        u16v(floatToHalf(s.a));
        u16v(floatToHalf(s.b));
        u16v(floatToHalf(s.hp));
        // The state's end matters only in timed states (a few beats either side); far ends are clamped.
        u16v(static_cast<u16>(q16(static_cast<float>(s.until - base), kUntilQ)));
        u32v(s.rng);
        u8v(static_cast<u8>((s.state & 15u) | ((s.flags & kFlagEngaged) ? 16u : 0u) | ((s.flags & kFlagArriving) ? 32u : 0u) |
                            ((s.flags & kFlagCalled) ? 64u : 0u) | (s.frozen ? 128u : 0u)));
        u8v(s.target);
        u8v(s.step);
    }
    return n;
}
u32 PacketWriter::putMotions(u16 run, double base, const NetMotion* items, u32 n) {
    // Item: varint (serial - previous serial, the first from 0), x i16, y i16, heading u16, bits u8 (0-2 the target, 7 none; 3
    // knockback follows as two i8; 4 engaged; 5 frozen), [kx i8, ky i8]. Serials strictly increasing.
    const auto varSize = [](u32 v) {
        u32 b = 1;
        while (v >= 0x80u) {
            v >>= 7;
            ++b;
        }
        return b;
    };
    const auto knocked = [](const NetMotion& m) { return q8(m.kx, kKnockQ) != 0 || q8(m.ky, kKnockQ) != 0; };
    if (room() < kHeader + 11 + kMotionBytesMax) return 0;
    const u32 budget = room() - kHeader - 11;
    u32 size = 0, fit = 0, prev = 0;
    for (; fit < n && fit < 255u; ++fit) {
        const NetMotion& m = items[fit];
        if (m.serial <= prev) break;  // (the caller sorts them)
        const u32 sz = varSize(m.serial - prev) + 7u + (knocked(m) ? 2u : 0u);
        if (size + sz > budget) break;
        size += sz;
        prev = m.serial;
    }
    if (!fit || !begin(Msg::Motions, 11 + size)) return 0;
    u16v(run);
    f64v(base);
    u8v(static_cast<u8>(fit));
    prev = 0;
    for (u32 k = 0; k < fit; ++k) {
        const NetMotion& m = items[k];
        u32 v = m.serial - prev;
        while (v >= 0x80u) {
            u8v(static_cast<u8>(v | 0x80u));
            v >>= 7;
        }
        u8v(static_cast<u8>(v));
        prev = m.serial;
        u16v(static_cast<u16>(q16(m.x, kPosQ)));
        u16v(static_cast<u16>(q16(m.y, kPosQ)));
        u16v(angleQ(m.heading));
        const bool kb = knocked(m);
        u8v(static_cast<u8>((m.target < kMaxPlayers ? m.target : 7u) | (kb ? 8u : 0u) | (m.engaged ? 16u : 0u) | (m.frozen ? 32u : 0u)));
        if (kb) {
            u8v(static_cast<u8>(q8(m.kx, kKnockQ)));
            u8v(static_cast<u8>(q8(m.ky, kKnockQ)));
        }
    }
    return fit;
}
bool PacketWriter::putStats(const MsgStats* st, u32 n) {
    if (n < 1 || n > kMaxPlayers || !begin(Msg::Stats, 1 + n * kStatsBytes)) return false;
    u8v(static_cast<u8>(n));
    for (u32 k = 0; k < n; ++k) {
        u8v(st[k].player);
        u8v(st[k].cells);
        u8v(st[k].novas);
        u32v(st[k].chain);
        u32v(st[k].kills);
        u64v(st[k].score);
    }
    return true;
}

bool PacketWriter::putAttention(u16 run, const u32* serials, u32 n) {
    if (n < 1 || n > kMaxAttention || !begin(Msg::Attention, 3 + n * 4)) return false;
    u16v(run);
    u8v(static_cast<u8>(n));
    for (u32 k = 0; k < n; ++k) u32v(serials[k]);
    return true;
}

bool PacketWriter::putHeartbeat(const MsgHeartbeat& m) {
    const u32 parts = m.bossParts < sim::kBossParts ? m.bossParts : sim::kBossParts;
    if (!begin(Msg::Heartbeat, 57 + 4 * parts)) return false;
    u16v(m.run);
    u64v(static_cast<u64>(m.hostUs));
    f64v(m.song);
    f32v(m.tps);
    f64v(m.nextBar);
    f32v(m.tpsNext);
    f64v(m.runOffset);
    u8v(m.state);
    u8v(m.bossOn);
    u8v(static_cast<u8>(parts));
    for (u32 k = 0; k < parts; ++k) f32v(m.bossPart[k]);
    f32v(m.bossCore);
    f32v(m.bossX);
    f32v(m.bossY);
    return true;
}

}  // namespace rf::net
