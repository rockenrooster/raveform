// Unit tests for the co-op network (M4): message round trips, the packet parsing fuzz (TECH_SPEC 8.3: every
// length, count and id validated; nothing allocated from a size field), the wire and its condition simulator,
// clock and song sync, and a short loopback session (a host and a client, census and run start). Run by
// scripts/test.ps1. Exit code: the number of failures (0 = pass).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "core/alloc_counter.h"
#include "net/link.h"
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
    void onDeath(u16, const NetDeath&) override { ++deaths; }
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
    CHECK(r.rng == s.rng && r.state == kTelegraph && r.flags == kFlagEngaged && r.target == 2);

    // A full corrections packet stays inside 1,200 bytes.
    NetEnemyState many[64];
    for (u32 k = 0; k < 64; ++k) {
        many[k] = s;
        many[k].serial = k + 1;
    }
    w.reset();
    const u32 put = w.putCorrections(2, 10000.0, many, 64);
    CHECK(put == 39 && w.size() <= kMaxPacketBytes);

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
    std::printf("[net tests] %d checks, %d failures\n", g_checks, g_failures);
    delete species;
    return g_failures;
}
