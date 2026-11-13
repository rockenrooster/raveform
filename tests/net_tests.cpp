// Unit tests for the co-op network (M4): message round trips, the packet parsing fuzz (TECH_SPEC 8.3: every
// length, count and id validated; nothing allocated from a size field), the wire and its condition simulator,
// clock and song sync, and a short loopback session (a host and a client, census and run start). Run by
// scripts/test.ps1. Exit code: the number of failures (0 = pass).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>

#include "core/alloc_counter.h"
#include "net/link.h"
#include "net/lobby.h"
#include "net/peer.h"
#include "net/protocol.h"
#include "net/sync.h"
#include "net/wire.h"
#include "sim/species.h"

using namespace rf;
using namespace rf::net;
using namespace rf::sim;

namespace {

int g_checks = 0, g_failures = 0;
const char* g_test = "";

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::printf("  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond); \
        }                                                                                \
    } while (0)

u32 g_rng = 12345;
u32 rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

// Counts what a parse hands on.
struct Count : PacketSink {
    u32 messages = 0, spawns = 0, deaths = 0, claims = 0, corrections = 0, poses = 0, attention = 0;
    NetClaim claimList[512];
    u32 attentionList[kMaxAttention] = {};
    NetSpawn lastSpawn;
    NetEnemyState lastCorr;
    MsgPose lastPose;
    MsgHeartbeat lastBeat;
    MsgPlayerEvent lastEvent;
    void onHello(const MsgHello&) override { ++messages; }
    void onWelcome(const MsgWelcome&) override { ++messages; }
    void onPing(i64) override { ++messages; }
    void onPong(i64, i64, i64) override { ++messages; }
    void onRunStart(const MsgRunStart&) override { ++messages; }
    void onSpawn(u16, const NetSpawn& s) override {
        ++spawns;
        lastSpawn = s;
    }
    void onDeath(u16, const NetDeath& d) override {
        if (deaths < 512) deathList[deaths] = d;
        ++deaths;
    }
    NetDeath deathList[512];
    void onClaim(u16, const NetClaim& c) override {
        if (claims < 512) claimList[claims] = c;
        ++claims;
    }
    void onAttention(u16, const u32* serials, u32 n) override {
        for (u32 k = 0; k < n && k < kMaxAttention; ++k) attentionList[k] = serials[k];
        attention += n;
    }
    void onPlayerEvent(const MsgPlayerEvent& m) override {
        ++messages;
        lastEvent = m;
    }
    void onCorrection(const NetEnemyState& s) override {
        ++corrections;
        lastCorr = s;
    }
    void onPose(const MsgPose& m) override {
        ++poses;
        lastPose = m;
    }
    void onHeartbeat(const MsgHeartbeat& m) override {
        ++messages;
        lastBeat = m;
    }
    void onCensus(const MsgCensus&) override { ++messages; }
};

// A packet with one of every kind of message (lists of a few items).
void fullPacket(PacketWriter& w, u32 variant) {
    w.reset();
    MsgHello h;
    h.content = 0x1122334455667788ull;
    std::snprintf(h.name, sizeof h.name, "TESTER");
    w.putHello(h);
    w.putPing(123456789);
    MsgRunStart rs;
    rs.run = 2;
    rs.bar = 3840.0;
    rs.seed = 99;
    rs.players = 4;
    w.putRunStart(rs);
    NetSpawn sp[3];
    for (u32 k = 0; k < 3; ++k) {
        sp[k].serial = 100 + k + variant;
        sp[k].species = static_cast<u16>(k * 7);
        sp[k].x = 120.5f * k;
        sp[k].y = -64.25f;
        sp[k].heading = 1.0f;
        sp[k].land = 4032.0;
        sp[k].group = 3;
        sp[k].flags = kFlagArriving;
    }
    w.putSpawns(2, 3840.0, sp, 3);
    NetDeath d[2] = {{77, 1, kDeathKill}, {78, 0xFF, kDeathDissolve}};
    w.putDeaths(2, d, 2);
    NetClaim c[2] = {{77, 1.25f, kClaimHit}, {79, 0.0f, kClaimKill}};
    w.putClaims(2, c, 2);
    MsgPlayerEvent ev;
    ev.run = 2;
    ev.player = 1;
    ev.kind = kPeTrioOpen;
    ev.b = 3;
    ev.opts[0] = kSplit;
    ev.opts[1] = kOrbitals;
    ev.opts[2] = kMagnet;
    ev.x = 10.0f;
    ev.tick = 4000.0;
    w.putPlayerEvent(ev);
    MsgCensus cs;
    cs.run = 2;
    cs.count = 300;
    cs.digest = 0xABCDEFull;
    w.putCensus(cs);
    const u32 att[3] = {500 + variant, 501, 77};
    w.putAttention(2, att, 3);
}

void testRoundTrip() {
    g_test = "round trip";
    PacketWriter w;
    fullPacket(w, 0);
    Count c;
    CHECK(parsePacket(w.data(), w.size(), c));
    CHECK(c.messages == 5);  // hello, ping, run start, player event, census
    CHECK(c.spawns == 3);
    CHECK(c.deaths == 2);
    CHECK(c.claims == 2);
    CHECK(c.lastSpawn.serial == 102 && c.lastSpawn.species == 14 && std::fabs(c.lastSpawn.x - 241.0f) < 0.08f);
    CHECK(std::fabs(c.lastSpawn.land - 4032.0) < 1e-3 && c.lastSpawn.group == 3 && c.lastSpawn.flags == kFlagArriving);
    CHECK(c.lastEvent.kind == kPeTrioOpen && c.lastEvent.opts[1] == kOrbitals && c.lastEvent.tick == 4000.0);
    CHECK(c.attention == 3 && c.attentionList[0] == 500 && c.attentionList[2] == 77);
    CHECK(c.claimList[0].serial == 77 && c.claimList[0].damage == 1.25f && c.claimList[0].kind == kClaimHit);
    CHECK(c.claimList[1].serial == 79 && c.claimList[1].damage == 0.0f && c.claimList[1].kind == kClaimKill);

    // Play test 2: a formation of the crowd costs 7 bytes an enemy (the first says everything), deaths in serial order a
    // byte or two; both round-trip exactly as before.
    {
        NetSpawn crowd[120];
        for (u32 k = 0; k < 120; ++k) {
            crowd[k] = NetSpawn{};
            crowd[k].serial = 5000 + k;
            crowd[k].species = 3;
            crowd[k].x = -800.0f + 13.0f * static_cast<float>(k);
            crowd[k].y = 200.0f - 3.0f * static_cast<float>(k);
            crowd[k].heading = 0.01f * static_cast<float>(k);
            crowd[k].land = 9000.0;
            crowd[k].flags = kFlagArriving;
        }
        crowd[60].species = 7;  // another species and a school in the middle: those items say so
        crowd[60].group = 12;
        PacketWriter sw;
        CHECK(sw.putSpawns(4, 8800.0, crowd, 120) == 120);
        CHECK(sw.size() <= 3 + 11 + 17 + 119 * 7 + 2 * 3);
        Count sc;
        CHECK(parsePacket(sw.data(), sw.size(), sc));
        CHECK(sc.spawns == 120 && sc.lastSpawn.serial == 5119 && sc.lastSpawn.species == 3 && sc.lastSpawn.group == 0);
        CHECK(std::fabs(sc.lastSpawn.x - crowd[119].x) < 0.08f && std::fabs(sc.lastSpawn.land - 9000.0) < 0.2 && sc.lastSpawn.flags == kFlagArriving);
        NetDeath dd[300];
        for (u32 k = 0; k < 300; ++k) dd[k] = NetDeath{7000 + 3 * k, static_cast<u8>(k % 5 == 4 ? 0xFF : k % 4), static_cast<u8>(k % 3)};
        PacketWriter dw;
        CHECK(dw.putDeaths(4, dd, 300) == 300);
        CHECK(dw.size() < 3 + 4 + 300 * 2);
        Count dc;
        CHECK(parsePacket(dw.data(), dw.size(), dc));
        bool same = dc.deaths == 300;
        for (u32 k = 0; k < 300 && same; ++k)
            same = dc.deathList[k].serial == dd[k].serial && dc.deathList[k].killer == dd[k].killer && dc.deathList[k].kind == dd[k].kind;
        CHECK(same);
    }
    // Claims: a crowd hit by one ring costs a byte or two an enemy in serial order; any order still round-trips.
    {
        NetClaim many[400];
        for (u32 k = 0; k < 400; ++k) {
            many[k].serial = 100000 + k * 3;
            many[k].damage = k % 50 == 7 ? 2.5f : 0.75f;
            many[k].kind = k % 97 == 5 ? kClaimKill : kClaimHit;
        }
        PacketWriter cw;
        CHECK(cw.putClaims(9, many, 400) == 400);
        CHECK(cw.size() < 400 * 2);
        Count cc;
        CHECK(parsePacket(cw.data(), cw.size(), cc));
        bool same = cc.claims == 400;
        for (u32 k = 0; k < 400 && same; ++k)
            same = cc.claimList[k].serial == many[k].serial && cc.claimList[k].kind == many[k].kind &&
                   cc.claimList[k].damage == (many[k].kind == kClaimHit ? many[k].damage : 0.0f);
        CHECK(same);
        // Reversed (every delta wraps around) and a repeated serial (delta 0).
        NetClaim rev[6] = {{900, 1.0f, kClaimHit}, {500, 1.0f, kClaimHit}, {500, 3.0f, kClaimHit},
                           {4000000000u, 0.0f, kClaimDissolve}, {1, 255.0f, kClaimHit}, {2, 0.015625f, kClaimHit}};
        cw.reset();
        CHECK(cw.putClaims(9, rev, 6) == 6);
        Count cr;
        CHECK(parsePacket(cw.data(), cw.size(), cr));
        same = cr.claims == 6;
        for (u32 k = 0; k < 6 && same; ++k)
            same = cr.claimList[k].serial == rev[k].serial && cr.claimList[k].kind == rev[k].kind && cr.claimList[k].damage == rev[k].damage;
        CHECK(same);
        // A packet's worth: as many as fit, the rest for the next packet.
        NetClaim spread[700];
        for (u32 k = 0; k < 700; ++k) spread[k] = NetClaim{1u + k * 100003u, 0.5f + static_cast<float>(k % 7), kClaimHit};
        cw.reset();
        const u32 put = cw.putClaims(9, spread, 700);
        CHECK(put > 100 && put < 700 && cw.size() <= kMaxPacketBytes);
        Count cs2;
        CHECK(parsePacket(cw.data(), cw.size(), cs2) && cs2.claims == put);
    }

    // Corrections: quantized within their steps.
    NetEnemyState s;
    s.serial = 4242;
    s.x = -4321.7f;
    s.y = 2650.3f;
    s.vx = 133.33f;
    s.vy = -620.0f;
    s.kx = 300.0f;
    s.ky = -42.0f;
    s.heading = -2.5f;
    s.a = 0.73f;
    s.b = -0.5f;
    s.hp = 37.25f;
    s.until = 10032.25;
    s.rng = 0xDEADBEEF;
    s.state = kTelegraph;
    s.flags = kFlagEngaged;
    s.target = 2;
    s.far = 1;
    s.step = 7;  // M5 part 2: a family's own counter
            s.frozen = true;  // M5 part 3: Freeze Frame's bit
    w.reset();
    CHECK(w.putCorrections(2, 10000.0, &s, 1) == 1);
    Count c2;
    CHECK(parsePacket(w.data(), w.size(), c2));
    CHECK(c2.corrections == 1);
    const NetEnemyState& r = c2.lastCorr;
    CHECK(r.serial == 4242 && std::fabs(r.x - s.x) <= 0.08f && std::fabs(r.y - s.y) <= 0.08f);
    CHECK(std::fabs(r.vx - s.vx) <= 0.06f && std::fabs(r.vy - s.vy) <= 0.06f && std::fabs(r.kx - s.kx) <= 2.6f);
    CHECK(std::fabs(std::remainder(r.heading - s.heading, kTau)) < 1e-3f);
    CHECK(std::fabs(r.a - s.a) < 1e-3f && r.b == -0.5f && r.hp == 37.25f && std::fabs(r.until - s.until) < 0.13);
    CHECK(r.rng == s.rng && r.state == kTelegraph && r.flags == kFlagEngaged && r.target == 2 && r.step == 7 && r.frozen);

    // A full corrections packet stays inside 1,200 bytes.
    NetEnemyState many[64];
    for (u32 k = 0; k < 64; ++k) {
        many[k] = s;
        many[k].serial = k + 1;
    }
    w.reset();
    const u32 put = w.putCorrections(2, 10000.0, many, 64);
    CHECK(put == 38 && w.size() <= kMaxPacketBytes);  // M5: 31 bytes each (39 at 30)
    if (put != 38) std::printf("  corrections in a packet: %u (%u bytes)\n", put, static_cast<u32>(w.size()));

    // M4 net follow-up: motion corrections (serials as varint steps, knockback only when there is some), after full ones
    // in the same packet; a serial out of order ends what is written.
    {
        NetMotion mo[3];
        mo[0].serial = 4242;
        mo[0].x = -1234.5f;
        mo[0].y = 678.25f;
        mo[0].heading = 2.5f;
        mo[0].target = 1;
        mo[0].engaged = true;
        mo[1] = mo[0];
        mo[1].serial = 4243;
        mo[1].kx = -40.0f;
        mo[1].ky = 25.0f;
        mo[1].target = 0xFF;
        mo[1].frozen = true;
        mo[2] = mo[0];
        mo[2].serial = 70000;
        w.reset();
        CHECK(w.putCorrections(2, 10000.0, &s, 1) == 1);
        const u32 before = w.size();
        CHECK(w.putMotions(2, 10000.0, mo, 3) == 3);
        CHECK(w.size() - before == 3 + 11 + (2 + 7) + (1 + 9) + (3 + 7));  // header, then steps of 2, 1 and 3 bytes
        struct MotionSink : Count {
            NetMotion got[4];
            u32 n = 0;
            u16 run = 0;
            double music = 0.0;
            void onMotions(u16 r, double m) override {
                run = r;
                music = m;
            }
            void onMotion(const NetMotion& m) override {
                if (n < 4) got[n] = m;
                ++n;
            }
        } ms;
        CHECK(parsePacket(w.data(), w.size(), ms));
        CHECK(ms.corrections == 1 && ms.n == 3 && ms.run == 2 && ms.music == 10000.0);
        CHECK(ms.got[0].serial == 4242 && ms.got[1].serial == 4243 && ms.got[2].serial == 70000);
        CHECK(std::fabs(ms.got[0].x - mo[0].x) <= 0.08f && std::fabs(ms.got[0].y - mo[0].y) <= 0.08f);
        CHECK(std::fabs(std::remainder(ms.got[0].heading - 2.5f, kTau)) < 1e-3f && ms.got[0].target == 1 && ms.got[0].engaged);
        CHECK(ms.got[0].kx == 0.0f && !ms.got[0].frozen);
        CHECK(ms.got[1].kx == -40.0f && ms.got[1].ky == 25.0f && ms.got[1].target == 0xFF && ms.got[1].frozen && ms.got[1].engaged);
        mo[2].serial = 10;  // out of order: not written
        w.reset();
        CHECK(w.putMotions(2, 10000.0, mo, 3) == 2);
        NetMotion lots[200];
        for (u32 k = 0; k < 200; ++k) {
            lots[k] = mo[0];
            lots[k].serial = 1000 + 3 * k;
        }
        w.reset();
        const u32 fit = w.putMotions(2, 10000.0, lots, 200);
        CHECK(fit > 100 && w.size() <= kMaxPacketBytes);  // about 8 bytes each
    }

    // Poses and the heartbeat.
    MsgPose p;
    p.player = 3;
    p.stampUs = 5000000;
    p.cur.x = 100.0f;
    p.cur.y = -50.0f;
    p.cur.vx = 520.0f;
    p.cur.aim = 1.0f;
    p.cur.fire = 2;
    p.cur.flags = 2;
    p.prevAgeMs = 33;
    MsgPose all[2] = {p, p};
    all[1].player = 1;
    w.reset();
    CHECK(w.putPlayers(all, 2));
    MsgHeartbeat hb;
    hb.run = 2;
    hb.hostUs = 7000000;
    hb.song = 1234.5;
    hb.tps = 192.0f;
    hb.nextBar = 1536.0;
    hb.tpsNext = 163.2f;
    CHECK(w.putHeartbeat(hb));
    Count c3;
    CHECK(parsePacket(w.data(), w.size(), c3));
    CHECK(c3.poses == 2 && c3.lastPose.player == 1);
    CHECK(std::fabs(c3.lastPose.cur.vx - 520.0f) <= 0.5f && c3.lastPose.cur.fire == 2 && c3.lastPose.prevAgeMs == 33);
    CHECK(std::fabs(c3.lastPose.cur.x - 100.0f) <= 0.08f && std::fabs(c3.lastPose.cur.y + 50.0f) <= 0.08f);
    CHECK(c3.lastBeat.song == 1234.5 && c3.lastBeat.tpsNext == 163.2f);
}

void testFuzz() {
    g_test = "fuzz";
    // TECH_SPEC 8.3: malformed packets are dropped whole, nothing is allocated, nothing crashes.
    PacketWriter w;
    Count c;
    const u64 a0 = heapAllocCountThisThread();
    u32 accepted = 0, rejected = 0;
    u8 buf[kMaxPacketBytes];
    // 1. Random bytes.
    for (u32 it = 0; it < 200000; ++it) {
        const u32 n = 1 + rnd() % kMaxPacketBytes;
        for (u32 k = 0; k < n; ++k) buf[k] = static_cast<u8>(rnd());
        if (rnd() % 4 == 0) buf[0] = static_cast<u8>(16 + rnd() % 40);  // a known type, often
        (parsePacket(buf, n, c) ? accepted : rejected)++;
    }
    // 2. Valid packets, mutated: a byte changed, truncated, extended, a length or count field broken.
    u32 mutatedAccepted = 0;
    for (u32 it = 0; it < 200000; ++it) {
        fullPacket(w, it);
        u32 n = w.size();
        std::memcpy(buf, w.data(), n);
        switch (rnd() % 5) {
            case 0: buf[rnd() % n] = static_cast<u8>(rnd()); break;
            case 1: n = 1 + rnd() % n; break;
            case 2:
                if (n < kMaxPacketBytes) buf[n++] = static_cast<u8>(rnd());
                break;
            case 3: buf[1 + 3 * (rnd() % 4)] ^= static_cast<u8>(1 + rnd() % 255); break;
            default: buf[(rnd() % n) | 1] ^= 0x80; break;
        }
        if (parsePacket(buf, n, c)) ++mutatedAccepted;
    }
    // 3. Lists whose count disagrees with their length, ids that are zero, enums out of range.
    NetDeath d{0, 1, kDeathKill};
    w.reset();
    w.putDeaths(1, &d, 1);
    CHECK(!parsePacket(w.data(), w.size(), c));  // serial 0
    d = NetDeath{5, 9, kDeathKill};
    w.reset();
    w.putDeaths(1, &d, 1);
    CHECK(!parsePacket(w.data(), w.size(), c));  // killer 9
    d = NetDeath{5, 1, 7};
    w.reset();
    w.putDeaths(1, &d, 1);
    CHECK(!parsePacket(w.data(), w.size(), c));  // kind 7
    NetSpawn s;
    s.serial = 1;
    s.species = 4000;
    w.reset();
    w.putSpawns(1, 0.0, &s, 1);
    CHECK(!parsePacket(w.data(), w.size(), c));  // no such species
    MsgBoss bm;
    bm.x = 1e9f;
    w.reset();
    w.putBoss(bm);
    CHECK(!parsePacket(w.data(), w.size(), c));  // outside the world
    bm.x = std::nanf("");
    w.reset();
    w.putBoss(bm);
    CHECK(!parsePacket(w.data(), w.size(), c));  // not a number
    // Claims: no items, a varint longer than 5 bytes, a hit with no damage yet, a kill with a damage, serial 0.
    const auto raw = [&](const u8* bytes, u32 n) {
        w.reset();
        std::memcpy(w.raw(), bytes, n);
        w.setSize(n);
        return parsePacket(w.data(), w.size(), c);
    };
    const u8 claimsOk[] = {19, 7, 0, 1, 0, 1, 0, 0x29, 0x40, 0x00};  // run 1, one hit on serial 5, damage 1.0
    CHECK(raw(claimsOk, sizeof claimsOk));
    const u8 claimsNone[] = {19, 4, 0, 1, 0, 0, 0};
    CHECK(!raw(claimsNone, sizeof claimsNone));
    const u8 claimsLong[] = {19, 10, 0, 1, 0, 1, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01};
    CHECK(!raw(claimsLong, sizeof claimsLong));
    const u8 claimsNoDamage[] = {19, 5, 0, 1, 0, 1, 0, 0x28};
    CHECK(!raw(claimsNoDamage, sizeof claimsNoDamage));
    const u8 claimsKillDamage[] = {19, 7, 0, 1, 0, 1, 0, 0x2B, 0x40, 0x00};
    CHECK(!raw(claimsKillDamage, sizeof claimsKillDamage));
    const u8 claimsZero[] = {19, 7, 0, 1, 0, 1, 0, 0x01, 0x40, 0x00};
    CHECK(!raw(claimsZero, sizeof claimsZero));
    const u8 claimsKind3[] = {19, 5, 0, 1, 0, 1, 0, 0x2E};
    CHECK(!raw(claimsKind3, sizeof claimsKind3));
    const u8 claimsShort[] = {19, 7, 0, 1, 0, 2, 0, 0x29, 0x40, 0x00};  // says two items, holds one
    CHECK(!raw(claimsShort, sizeof claimsShort));
    // Attention: 1-8 nonzero serials, exactly.
    const u8 attOk[] = {53, 7, 0, 1, 0, 1, 9, 0, 0, 0};
    CHECK(raw(attOk, sizeof attOk));
    const u8 attNone[] = {53, 3, 0, 1, 0, 0};
    CHECK(!raw(attNone, sizeof attNone));
    const u8 attZero[] = {53, 7, 0, 1, 0, 1, 0, 0, 0, 0};
    CHECK(!raw(attZero, sizeof attZero));
    const u8 attNine[] = {53, 39, 0, 1, 0, 9, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    CHECK(!raw(attNine, sizeof attNine));
    const u8 attLong[] = {53, 8, 0, 1, 0, 1, 9, 0, 0, 0, 0};
    CHECK(!raw(attLong, sizeof attLong));
    // A count that claims more items than the payload holds.
    s = NetSpawn{};
    s.serial = 9;
    w.reset();
    w.putSpawns(1, 0.0, &s, 1);
    std::memcpy(buf, w.data(), w.size());
    buf[3 + 10] = 200;  // count byte after run (2) and base (8)
    CHECK(!parsePacket(buf, w.size(), c));
    // A message length beyond the packet.
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    CHECK(!parsePacket(buf, w.size(), c));
    // Empty and oversized packets.
    CHECK(!parsePacket(buf, 0, c));
    CHECK(!parsePacket(buf, kMaxPacketBytes + 1, c));
    const u64 allocs = heapAllocCountThisThread() - a0;
    CHECK(allocs == 0);
    std::printf("  fuzz: random packets %u accepted, %u rejected; mutated valid packets %u of 200000 still well formed; %llu allocations\n",
                accepted, rejected, mutatedAccepted, static_cast<unsigned long long>(allocs));
    CHECK(rejected > 190000);
}

void testWire() {
    g_test = "wire";
    // Two loopback hosts, unreliable packets under 5% loss and 80 ms jitter on a simulated clock.
    CHECK(netInit());
    wireUseSimClock(true);
    i64 t = 5000000000ll;
    wireSetSimTimeUs(t);
    NetSim sim;
    sim.loss = 0.05f;
    sim.jitterMs = 80.0f;
    sim.seed = 3;
    Link host, client;
    CHECK(host.listen("127.0.0.1", 47900, 4, WireKind::Loopback, sim));
    CHECK(client.connect("127.0.0.1", 47900, WireKind::Loopback, sim, kProtocolVersion));
    bool hostSaw = false, clientSaw = false;
    u32 got = 0, outOfOrder = 0, last = 0, sent = 0;
    Link::Event ev;
    for (u32 step = 0; step < 60000; ++step) {
        t += 1000;
        wireSetSimTimeUs(t);
        wirePump(host.socket());
        wirePump(client.socket());
        while (host.poll(ev)) {
            if (ev.type == Link::Event::Connect) hostSaw = ev.info == kProtocolVersion;
            if (ev.type == Link::Event::Receive && ev.size == 4) {
                u32 v;
                std::memcpy(&v, ev.data, 4);
                ++got;
                if (v < last) ++outOfOrder;
                last = v;
            }
        }
        while (client.poll(ev))
            if (ev.type == Link::Event::Connect) clientSaw = true;
        if (clientSaw && step % 16 == 0 && sent < 2000) {
            client.send(0, kChControl, &sent, 4, false);
            ++sent;
        }
        client.flush();
        host.flush();
    }
    CHECK(hostSaw && clientSaw);
    const WireStats cs = client.wire();
    std::printf("  wire: %u of %u unreliable packets arrived (%u out of order), lost %u, largest datagram %u\n",
                got, sent, outOfOrder, cs.lost, cs.maxDatagram);
    CHECK(cs.lost > 0);
    CHECK(got > sent * 90 / 100 && got < sent);  // 5% lost on the wire; unsequenced: the rest arrive
    CHECK(outOfOrder > 0);                       // ... in the order the jitter gives them
    CHECK(cs.maxDatagram <= 1392);
    host.close();
    client.close();
    wireUseSimClock(false);
    netShutdown();
    // parseNetSim.
    NetSim p;
    CHECK(parseNetSim("loss=0.05,jitter=80,delay=10,dup=0.01,reorder=0.02", p));
    CHECK(p.loss == 0.05f && p.jitterMs == 80.0f && p.delayMs == 10.0f && p.duplicate == 0.01f && p.reorder == 0.02f);
    CHECK(!parseNetSim("loss=2", p));
    CHECK(!parseNetSim("bogus=1", p));
}

void testClock() {
    g_test = "clock";
    // Samples with a known offset and random (asymmetric) delays: the minimum-RTT one is close; the steered
    // clock never moves faster than 1%.
    ClockSync c;
    const i64 offset = 1234567;  // host = local + offset
    i64 local = 1000000000;
    for (u32 k = 0; k < 40; ++k) {
        const i64 up = 2000 + static_cast<i64>(rnd() % 80000), down = 2000 + static_cast<i64>(rnd() % 80000);
        const i64 t0 = local, t1 = t0 + up + offset, t2 = t1 + 300, t3 = t2 - offset + down;
        c.addSample(t0, t1, t2, t3);
        local += 250000;
    }
    const i64 err = c.estimateUs() - offset;
    std::printf("  clock: estimate off by %.2f ms with 0-80 ms jitter each way (best RTT %.1f ms)\n", static_cast<double>(err) * 1e-3,
                static_cast<double>(c.rttUs()) * 1e-3);
    CHECK(std::llabs(err) < 20000);
    // Steering: a 50 ms step in the estimate is followed at <= 1%.
    c.snap();
    i64 l = 2000000000;
    double h0 = static_cast<double>(c.hostNowUs(l));
    c.addSample(l, l + offset + 50000 + 1000, l + offset + 50000 + 1000, l + 2000);  // a lower-RTT sample, 50 ms later
    bool within = true;
    for (u32 k = 0; k < 600; ++k) {
        l += 16667;
        const double h = static_cast<double>(c.hostNowUs(l));
        const double rate = (h - h0) / 16667.0;
        within = within && rate <= 1.0101 && rate >= 0.9899;
        h0 = h;
    }
    CHECK(within);
    CHECK(std::fabs(c.steeredUs() - static_cast<double>(c.estimateUs())) < 1000.0);

    // Songs: an anchor before a tempo change at the bar line.
    g_test = "song";
    SongAnchor a;
    a.hostUs = 1000000;
    a.song = 300.0;
    a.tps = 192.0;
    a.nextBar = 384.0;
    a.tpsNext = 163.2;
    SongModel m;
    m.set(a);
    CHECK(std::fabs(m.songAt(1000000) - 300.0) < 1e-9);
    const i64 atBar = 1000000 + static_cast<i64>((84.0 / 192.0) * 1e6);
    CHECK(std::fabs(m.songAt(atBar) - 384.0) < 0.01);
    CHECK(std::fabs(m.songAt(atBar + 1000000) - (384.0 + 163.2)) < 0.01);
    SongClock sc;
    sc.reset(0, 0.0, 120.0f);
    sc.advanceTo(3000000, [](void*, double bar) { return bar >= 384.0 ? 102.0f : 120.0f; }, nullptr);
    CHECK(std::fabs(sc.song() - (384.0 + 1.0 * 102.0 * 96.0 / 60.0)) < 0.01);
}

void testSession(const SpeciesTable& species) {
    g_test = "session";
    // A host and one client on the loopback network: the client is seated, the run starts on a bar line, the
    // enemy sets agree (census), nothing disconnects; 40 simulated seconds.
    wireUseSimClock(true);
    i64 t = 7000000000ll;
    wireSetSimTimeUs(t);
    auto* peers = new (std::nothrow) NetPeer[2];
    PeerConfig h, c;
    h.session.role = NetRole::Host;
    h.session.seats = 2;
    h.session.wire = WireKind::Loopback;
    h.session.port = 47901;
    std::snprintf(h.session.bindIp, sizeof h.session.bindIp, "127.0.0.1");
    c = h;
    c.session.role = NetRole::Client;
    std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
    c.clockOffsetUs = 500000;
    c.botSeed = 9;
    CHECK(peers[0].start(h, &species));
    CHECK(peers[1].start(c, &species));
    for (u32 step = 0; step < 40000; ++step) {
        t += 1000;
        wireSetSimTimeUs(t);
        wirePump(peers[0].session().link().socket());
        wirePump(peers[1].session().link().socket());
        if (step % 17 == 0) peers[0].frame(t);
        if (step % 17 == 8) peers[1].frame(t);
    }
    const Session& hs = peers[0].session();
    const Session& cs = peers[1].session();
    CHECK(hs.running() && cs.running());
    CHECK(cs.seat() == 1);
    CHECK(hs.stats().disconnects == 0 && cs.stats().disconnects == 0);
    const WorldCounters& wc = peers[1].world().counters();
    std::printf("  session: host %u enemies, client %u; census %u checks, %u mismatches; client predicted %u kills (%u confirmed); %u "
                "corrections\n",
                peers[0].world().enemies().count, peers[1].world().enemies().count, wc.censusChecks, wc.censusMismatches, wc.predicted,
                wc.confirmed, cs.stats().corrections);
    CHECK(wc.censusChecks >= 10 && wc.censusMismatches == 0);
    CHECK(peers[0].world().enemies().count > 10);
    peers[0].stop();
    peers[1].stop();
    delete[] peers;
    wireUseSimClock(false);
}

// ---------------------------------------------------------------------------------------------- M4 stage B

struct LobbySink : PacketSink {
    u32 lobbies = 0, beds = 0, refusals = 0;
    u8 lastRefuse = 0;
    MsgLobby lastLobby;
    MsgBed lastBed;
    void onLobby(const MsgLobby& m) override {
        ++lobbies;
        lastLobby = m;
    }
    void onBed(const MsgBed& m) override {
        ++beds;
        lastBed = m;
    }
    void onRefuse(u8 r) override {
        ++refusals;
        lastRefuse = r;
    }
};

void testLobby() {
    // TECH_SPEC 8.3: the beacon and the lobby's messages are parsed like every packet (exact lengths, every field
    // checked, nothing allocated); room codes both ways, and the /16 a code can reach.
    g_test = "beacon";
    const u64 a0 = heapAllocCountThisThread();
    BeaconInfo b;
    b.version = kProtocolVersion;
    b.port = kGamePort;
    b.code = 0xC0DE;
    b.players = 2;
    b.seats = 4;
    b.state = kBeaconLobby;
    std::snprintf(b.name, sizeof b.name, "MOTO");
    u8 buf[64];
    CHECK(beaconWrite(b, buf, sizeof buf));
    BeaconInfo r;
    CHECK(beaconParse(buf, kBeaconBytes, r));
    CHECK(r.version == kProtocolVersion && r.port == kGamePort && r.code == 0xC0DE && r.players == 2 && r.seats == 4 && r.compatible);
    CHECK(std::strcmp(r.name, "MOTO") == 0);
    CHECK(!beaconWrite(b, buf, kBeaconBytes - 1));
    // Exact length, magic, players <= seats <= 4, a known state, reserved zero, a printable name then zeros.
    u8 bad[64];
    const auto broken = [&](u32 at, u8 v, u32 size) {
        std::memcpy(bad, buf, kBeaconBytes);
        if (at < kBeaconBytes) bad[at] = v;
        BeaconInfo x;
        return beaconParse(bad, size, x);
    };
    CHECK(!broken(99, 0, kBeaconBytes - 1));
    CHECK(!broken(99, 0, kBeaconBytes + 1));
    CHECK(!broken(0, 'X', kBeaconBytes));       // magic
    CHECK(!broken(12, 5, kBeaconBytes));        // players > seats
    CHECK(!broken(13, 0, kBeaconBytes));        // no seats
    CHECK(!broken(13, 5, kBeaconBytes));        // five seats
    CHECK(!broken(14, 2, kBeaconBytes));        // state
    CHECK(!broken(15, 1, kBeaconBytes));        // reserved
    {
        BeaconInfo noPort = b;
        noPort.port = 0;  // no game port
        u8 p[kBeaconBytes];
        CHECK(beaconWrite(noPort, p, sizeof p) && !beaconParse(p, kBeaconBytes, r));
    }
    CHECK(!broken(17, 0x07, kBeaconBytes));     // a control character in the name
    CHECK(!broken(21, 'A', kBeaconBytes));      // a character after the name's end
    CHECK(!broken(31, 'A', kBeaconBytes));      // a name without its end
    {
        // Another version is heard, and marked as such (the join screen shows it, greyed).
        BeaconInfo other = b;
        other.version = kProtocolVersion + 1;
        CHECK(beaconWrite(other, buf, sizeof buf));
        CHECK(beaconParse(buf, kBeaconBytes, r) && !r.compatible);
    }
    // Fuzz: random datagrams of every length, and mutated valid beacons.
    u32 accepted = 0;
    for (u32 it = 0; it < 100000; ++it) {
        const u32 n = rnd() % 64;
        for (u32 k = 0; k < n; ++k) buf[k] = static_cast<u8>(rnd());
        if (rnd() % 2) std::memcpy(buf, "RFBC", 4);
        accepted += beaconParse(buf, n, r) ? 1u : 0u;
    }
    u32 mutated = 0;
    for (u32 it = 0; it < 100000; ++it) {
        CHECK(beaconWrite(b, buf, sizeof buf));
        buf[rnd() % kBeaconBytes] = static_cast<u8>(rnd());
        if (beaconParse(buf, kBeaconBytes, r)) {
            ++mutated;
            CHECK(r.players <= r.seats && r.seats >= 1 && r.seats <= 4 && r.state <= kBeaconRunning);
        }
    }
    std::printf("  beacon: random datagrams accepted %u of 100000; mutated beacons still well formed %u of 100000\n", accepted, mutated);
    CHECK(accepted < 50);

    // The lobby's messages: round trips, and what a parse refuses.
    g_test = "lobby messages";
    PacketWriter w;
    LobbySink sink;
    MsgLobby lm;
    lm.seats = 0x7;
    lm.ready = 0x5;
    lm.code = 0x1234;
    lm.state = 0;
    std::snprintf(lm.names[0], 16, "HOST");
    std::snprintf(lm.names[1], 16, "PLAYER TWO");
    MsgBed bed;
    bed.run = 3;
    bed.bar = 140;
    bed.level = 200;
    w.reset();
    CHECK(w.putLobby(lm) && w.putBed(bed) && w.putReady(1));
    CHECK(parsePacket(w.data(), w.size(), sink));
    CHECK(sink.lobbies == 1 && sink.lastLobby.seats == 0x7 && sink.lastLobby.ready == 0x5 && sink.lastLobby.code == 0x1234);
    CHECK(std::strcmp(sink.lastLobby.names[1], "PLAYER TWO") == 0 && sink.lastLobby.names[2][0] == 0);
    CHECK(sink.beds == 1 && sink.lastBed.bar == 140 && sink.lastBed.level == 200 && sink.lastBed.run == 3);
    lm.ready = 0x8;  // ready but not seated
    w.reset();
    w.putLobby(lm);
    CHECK(!parsePacket(w.data(), w.size(), sink));
    lm.ready = 0x1;
    lm.state = 2;
    w.reset();
    w.putLobby(lm);
    CHECK(!parsePacket(w.data(), w.size(), sink));
    lm.state = 0;
    w.reset();
    w.putLobby(lm);
    {
        u8 p[kMaxPacketBytes];
        std::memcpy(p, w.data(), w.size());
        p[3 + 6 + 16 + 3] = 0x01;  // a control character in the second name
        CHECK(!parsePacket(p, w.size(), sink));
        std::memcpy(p, w.data(), w.size());
        p[3 + 6 + 15] = 'X';  // the first name without its end
        CHECK(!parsePacket(p, w.size(), sink));
    }
    w.reset();
    w.putReady(2);  // ready is 0 or 1
    CHECK(!parsePacket(w.data(), w.size(), sink));
    u32 lobbyAccepted = 0;
    for (u32 it = 0; it < 100000; ++it) {
        w.reset();
        w.putLobby(lm);
        u8 p[kMaxPacketBytes];
        u32 n = w.size();
        std::memcpy(p, w.data(), n);
        p[3 + rnd() % (n - 3)] = static_cast<u8>(rnd());
        if (rnd() % 3 == 0) n = 1 + rnd() % n;
        lobbyAccepted += parsePacket(p, n, sink) ? 1u : 0u;
    }
    std::printf("  lobby: mutated lobby messages still well formed %u of 100000\n", lobbyAccepted);
    CHECK(heapAllocCountThisThread() - a0 == 0);

    // Room codes: every 16-bit value both ways; lower case is read; the confusable letters are not in it.
    g_test = "room code";
    bool all = true;
    for (u32 v = 0; v < 65536; ++v) {
        char code[5];
        roomCodeEncode(static_cast<u16>(v), code);
        u16 back = 0;
        all = all && roomCodeDecode(code, back) && back == v && std::strlen(code) == 4;
    }
    CHECK(all);
    u16 low = 0;
    CHECK(roomCodeDecode("k7qm", low));
    u16 upper = 0;
    CHECK(roomCodeDecode("K7QM", upper) && upper == low);
    for (const char* badCode : {"K7Q", "K7QMM", "K7Q0", "O7QM", "17QM", "I7QM", "L7QM", "Z7QM", "57QM", "S7QM", "87QM", "B7QM", "U7QM", "V7QM", ""})
        CHECK(!roomCodeDecode(badCode, low));
    CHECK(!roomCodeDecode(nullptr, low));
    // The host's address from a code and the joiner's own: the joiner's /16 in front of the host's low 16 bits.
    const u32 host = (192u << 24) | (168u << 16) | (43u << 8) | 17u;   // 192.168.43.17 (a phone's hotspot)
    const u32 joiner = (192u << 24) | (168u << 16) | (43u << 8) | 200u;
    char code[5];
    roomCodeEncode(static_cast<u16>(host & 0xFFFFu), code);
    CHECK(roomCodeDecode(code, low) && roomCodeHost(low, joiner) == host);
    // Within the /16 the third byte may differ (a home network wider than /24) ...
    CHECK(roomCodeHost(low, (192u << 24) | (168u << 16) | (7u << 8) | 9u) == host);
    // ... beyond it the code cannot reach the host (the limit TECH_SPEC 8.3 accepts; the list or an address can).
    CHECK(roomCodeHost(low, (10u << 24) | (0u << 16) | (0u << 8) | 5u) != host);
}

void testRefusals(const SpeciesTable& species) {
    // A client with other game data, one with another protocol version, and one too many, all on the loopback
    // network: each is refused with its reason and the host's run is not disturbed.
    g_test = "refusals";
    wireUseSimClock(true);
    i64 t = 9000000000ll;
    wireSetSimTimeUs(t);
    auto* peers = new (std::nothrow) NetPeer[3];
    PeerConfig h, c;
    h.session.role = NetRole::Host;
    h.session.seats = 2;
    h.session.wire = WireKind::Loopback;
    h.session.port = 47902;
    std::snprintf(h.session.bindIp, sizeof h.session.bindIp, "127.0.0.1");
    c = h;
    c.session.role = NetRole::Client;
    std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
    PeerConfig other = c;
    other.contentSalt = 0x5A5A;
    CHECK(peers[0].start(h, &species));
    CHECK(peers[1].start(other, &species));
    Link raw;
    CHECK(raw.connect("127.0.0.1", 47902, WireKind::Loopback, NetSim{}, kProtocolVersion + 1));
    LobbySink rawSink;
    bool rawGone = false;
    for (u32 step = 0; step < 6000; ++step) {
        t += 1000;
        wireSetSimTimeUs(t);
        wirePump(peers[0].session().link().socket());
        wirePump(peers[1].session().link().socket());
        wirePump(raw.socket());
        if (step % 17 == 0) peers[0].frame(t);
        if (step % 17 == 8) peers[1].frame(t);
        Link::Event ev;
        while (raw.poll(ev)) {
            if (ev.type == Link::Event::Receive) parsePacket(ev.data, ev.size, rawSink);
            if (ev.type == Link::Event::Disconnect) rawGone = true;
        }
        raw.flush();
    }
    const char* contentWhy = peers[1].session().endReason();  // (a string literal: it outlives the session)
    CHECK(peers[1].session().state() == SessionState::Ended && std::strcmp(contentWhy, "other game data") == 0);
    CHECK(rawGone && (rawSink.refusals == 0 || rawSink.lastRefuse == kRefuseVersion));
    CHECK(peers[0].session().stats().refused >= 2);
    raw.close();
    // A seated client, then one too many: refused as full.
    peers[1].stop();
    CHECK(peers[1].start(c, &species));
    CHECK(peers[2].start(c, &species));
    for (u32 step = 0; step < 6000; ++step) {
        t += 1000;
        wireSetSimTimeUs(t);
        for (u32 k = 0; k < 3; ++k) wirePump(peers[k].session().link().socket());
        if (step % 17 == 0) peers[0].frame(t);
        if (step % 17 == 6) peers[1].frame(t);
        if (step % 17 == 12 && step > 300) peers[2].frame(t);
    }
    CHECK(peers[1].session().seat() == 1 && peers[1].session().state() != SessionState::Ended);
    const char* why = peers[2].session().endReason();
    CHECK(peers[2].session().state() == SessionState::Ended && (std::strcmp(why, "the run is full") == 0 || std::strcmp(why, "the run has started") == 0));
    std::printf("  refusals: other game data: '%s'; another version: refused %s; one too many: '%s'\n", contentWhy,
                rawGone ? "and disconnected" : "(not seen)", why);
    for (u32 k = 0; k < 3; ++k) peers[k].stop();
    delete[] peers;
    wireUseSimClock(false);
}

void testLeave(const SpeciesTable& species) {
    // GAME_DESIGN 10: a client that leaves opens its seat and the run goes on for the others; the host leaving ends
    // the run on every client (each knows why).
    g_test = "leave";
    wireUseSimClock(true);
    i64 t = 11000000000ll;
    wireSetSimTimeUs(t);
    auto* peers = new (std::nothrow) NetPeer[3];
    PeerConfig h, c;
    h.session.role = NetRole::Host;
    h.session.seats = 3;
    h.session.wire = WireKind::Loopback;
    h.session.port = 47903;
    std::snprintf(h.session.bindIp, sizeof h.session.bindIp, "127.0.0.1");
    c = h;
    c.session.role = NetRole::Client;
    std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
    CHECK(peers[0].start(h, &species));
    c.botSeed = 3;
    CHECK(peers[1].start(c, &species));
    c.botSeed = 4;
    CHECK(peers[2].start(c, &species));
    const auto run = [&](u32 steps, bool third) {
        for (u32 step = 0; step < steps; ++step) {
            t += 1000;
            wireSetSimTimeUs(t);
            for (u32 k = 0; k < 3; ++k)
                if ((k < 2 || third) && peers[k].session().state() != SessionState::Idle) wirePump(peers[k].session().link().socket());
            if (step % 17 == 0) peers[0].frame(t);
            if (step % 17 == 6) peers[1].frame(t);
            if (third && step % 17 == 12) peers[2].frame(t);
        }
    };
    run(20000, true);
    CHECK(peers[0].session().running() && peers[1].session().running() && peers[2].session().running());
    const u32 seat2 = peers[2].session().seat();
    CHECK(peers[0].world().player(seat2).present);
    // The client on seat 2 leaves.
    peers[2].stop();
    run(5000, false);
    CHECK(peers[0].session().running() && peers[1].session().running());
    CHECK((peers[0].session().lobbySeats() & (1u << seat2)) == 0);
    CHECK(!peers[0].world().player(seat2).present && !peers[1].world().player(seat2).present);
    CHECK(peers[0].world().player(0).present && peers[1].world().player(peers[1].session().seat()).present);
    // The host leaves: the client's run is over, and it knows why.
    peers[0].stop();
    run(3000, false);
    CHECK(peers[1].session().state() == SessionState::Ended);
    const char* why = peers[1].session().endReason();
    CHECK(std::strcmp(why, "the host left") == 0 || std::strcmp(why, "the host is gone") == 0);
    std::printf("  leave: a client left (its seat opened, the run went on); the host left: '%s'\n", why);
    for (u32 k = 0; k < 3; ++k) peers[k].stop();
    delete[] peers;
    wireUseSimClock(false);
}

void testDownRevive(const SpeciesTable& species) {
    // GAME_DESIGN 10, look.md 6: a hit at zero cells downs a ship into a beacon; a teammate in its ring for two
    // beats revives it with one cell, invulnerable for a bar (the fill empties twice as fast as it fills); every
    // beacon revives at a Break; all down at once ends the run (time at 50% for a beat).
    g_test = "down and revive";
    auto* w = new (std::nothrow) World;
    w->init(&species);
    w->spawner().mode = Spawner::Mode::Off;
    w->setNet(NetRole::Host, 0x3);  // both players this device's (as a host with its bots)
    w->resetRun(5, 0.0, 2);
    double music = 0.0;
    const double perTick = 120.0 * 96.0 / 60.0 / 60.0;
    Vec2 pos[2] = {{0.0f, 0.0f}, {300.0f, 0.0f}};
    u32 downs = 0, revives = 0, teamDeaths = 0;
    u8 lastReviveBy = 0xFF;
    const auto step = [&]() {
        TickInput in;
        in.music0 = music;
        in.music1 = music + perTick;
        in.bpm = 120.0f;
        for (u32 p = 0; p < 2; ++p) {
            in.players[p].present = true;
            in.players[p].pos = pos[p];
        }
        w->tick(in);
        music = in.music1;
        for (u32 k = 0; k < w->eventCount(); ++k) {
            const Event& e = w->events()[k];
            if (e.type == EventType::Down) ++downs;
            if (e.type == EventType::Revive) {
                ++revives;
                lastReviveBy = e.b;
            }
            if (e.type == EventType::Death && e.a == 0xFF) ++teamDeaths;
        }
    };
    const auto down = [&](u32 p) {
        PlayerState& pl = w->playerMut(p);
        pl.cells = 0;
        pl.novas = 0;  // (Auto Nova would fire one instead)
        pl.invulS = pl.novaInvulS = 0.0f;
        w->spawnBullet(pos[p], Vec2{}, 0.0f);
        step();
    };
    step();
    down(1);
    CHECK(downs == 1 && w->player(1).down && !w->player(1).alive && w->player(0).alive);
    CHECK(std::fabs(w->player(1).beacon.x - 300.0f) < 1.0f);
    // A teammate in the ring for two beats: revived with one cell, invulnerable for a bar.
    pos[0] = Vec2{300.0f, 60.0f};
    u32 ticks = 0;
    while (!revives && ticks < 200) {
        step();
        ++ticks;
    }
    const double beats = static_cast<double>(ticks) * perTick / kTicksPerBeat;
    CHECK(revives == 1 && lastReviveBy == 1 && w->player(1).alive && w->player(1).cells == 1);
    CHECK(beats > 1.9 && beats < 2.2);
    CHECK(std::fabs(w->player(1).invulS - static_cast<float>(kTicksPerBar) / w->ticksPerSecond()) < 0.1f);
    // The fill empties twice as fast as it fills: a beat in, half a beat out, and it is empty.
    pos[0] = Vec2{0.0f, 0.0f};
    for (u32 k = 0; k < 200; ++k) step();  // past the bar of invulnerability
    down(1);
    pos[0] = Vec2{300.0f, 60.0f};
    while (w->player(1).reviveTicks < kTicksPerBeat) step();
    pos[0] = Vec2{0.0f, 0.0f};
    for (double gone = 0.0; gone < 0.5 * kTicksPerBeat + perTick; gone += perTick) step();
    CHECK(w->player(1).down && w->player(1).reviveTicks < 1e-6);
    // At the Break every beacon revives on its first beat, without the fill.
    while (music < 20.0 * kTicksPerBar + 2.0 * perTick) step();
    CHECK(revives == 2 && lastReviveBy == 0 && w->player(1).alive);
    // All down at once: the run is over; time dips to 50% for a beat.
    for (u32 k = 0; k < 200; ++k) step();
    down(1);
    down(0);
    CHECK(teamDeaths == 1 && w->timeScale() == 0.5f);
    for (u32 k = 0; k < 40; ++k) step();
    CHECK(w->timeScale() != 0.5f);
    std::printf("  down and revive: revived by a teammate after %.2f beats, the fill emptied at twice the speed, the Break revived, "
                "all down ended the run\n",
                beats);
    delete w;
}

void testRelayClocks(const SpeciesTable& species) {
    g_test = "relay";
    // M4 net follow-up (docs/PROGRESS_m4_net.md): the host relays each client's pose to the others, whatever the clients'
    // own clocks read. Client 1's clock runs 6.5 days ahead of the host's (the PC's against the Moto's); before the fix its
    // lobby poses, stamped on its own clock, sorted as the newest for good and the host relayed only those, so clients 2
    // and 3 saw player 1 stand where it stood in the lobby for the whole run. A host and three clients, 40 simulated s.
    wireUseSimClock(true);
    i64 t = 9000000000ll;
    wireSetSimTimeUs(t);
    auto* peers = new (std::nothrow) NetPeer[4];
    static const i64 kOffsets[4] = {0, 560397120000ll, -777000, 3210};
    for (u32 k = 0; k < 4; ++k) {
        PeerConfig c;
        c.session.role = k == 0 ? NetRole::Host : NetRole::Client;
        c.session.seats = 4;
        c.session.wire = WireKind::Loopback;
        c.session.port = 47902;
        std::snprintf(c.session.bindIp, sizeof c.session.bindIp, "%s", k == 0 ? "127.0.0.1" : "");
        std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
        c.clockOffsetUs = kOffsets[k];
        c.botSeed = 31 + k;
        CHECK(peers[k].start(c, &species));
    }
    float worst = 0.0f;
    for (u32 step = 0; step < 40000; ++step) {
        t += 1000;
        wireSetSimTimeUs(t);
        for (u32 k = 0; k < 4; ++k) wirePump(peers[k].session().link().socket());
        for (u32 k = 0; k < 4; ++k)
            if (step % 17 == k * 4) peers[k].frame(t);
        if (step == 30000)
            for (u32 k = 1; k < 4; ++k) peers[k].session().resetDiag();
        if (step > 30000 && step % 500 == 0) {
            // Each client's copy of every other client's player against that player's own position (within the 66 ms
            // the copy is drawn behind and a ship's top speed).
            for (u32 k = 1; k < 4; ++k)
                for (u32 j = 1; j < 4; ++j) {
                    if (j == k || !peers[j].world().player(j).alive) continue;
                    const float d = length(peers[k].world().player(j).pos - peers[j].world().player(j).pos);
                    if (d > worst) worst = d;
                }
        }
    }
    u32 heard = 0;
    for (u32 k = 1; k < 4; ++k) {
        CHECK(peers[k].session().running());
        for (u32 j = 1; j < 4; ++j)
            if (j != k) {
                CHECK(peers[k].session().diag().poses[j] > 100);  // new poses of every other client arrived (30 Hz)
                heard += peers[k].session().diag().poses[j];  // (a relay that repeats the newest still carries the one before)
            }
    }
    CHECK(worst < 250.0f);
    std::printf("  relay: with one client's clock 6.5 days ahead, each client heard the others' new poses (%u in 10 s); the farthest "
                "copy of a teammate %.1f wu from the teammate itself\n",
                heard, static_cast<double>(worst));
    for (u32 k = 0; k < 4; ++k) peers[k].stop();
    delete[] peers;
    wireUseSimClock(false);
}

}  // namespace

int main() {
    auto* species = new (std::nothrow) SpeciesTable;
    if (!species || !species->load()) {
        std::printf("species table failed to load\n");
        return 1;
    }
    testRoundTrip();
    testFuzz();
    testWire();
    testClock();
    testSession(*species);
    testLobby();
    testRefusals(*species);
    testLeave(*species);
    testDownRevive(*species);
    testRelayClocks(*species);
    std::printf("[net tests] %d checks, %d failures\n", g_checks, g_failures);
    delete species;
    return g_failures;
}
