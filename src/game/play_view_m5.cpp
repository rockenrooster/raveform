// The view of M5 part 1: Longwave (design/bosses/longwave.md "Look"), The Choir (choir.md "Look"), the mini-bosses and
// the Pen (minibosses.md). Shapes come from the boss pages' json through the species table; cosmetic only, nothing here
// touches the simulation. Kaleidos stays in play_view_b.cpp.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 lerpv(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec2 rot(Vec2 p, float c, float s) { return {p.x * c - p.y * s, p.x * s + p.y * c}; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
u32 bitsIn(u32 m) {
    u32 n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}

// OKLCH -> linear sRGB, unclamped (the gamut test) and clamped.
Vec3 oklchRaw(float L, float hueDeg, float chroma) {
    const float h = hueDeg * kTau / 360.0f;
    const float a = chroma * std::cos(h), b = chroma * std::sin(h);
    const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return Vec3{4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
                -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s};
}
Vec3 oklch(float L, float hueDeg, float chroma) {
    const Vec3 c = oklchRaw(L, hueDeg, chroma);
    return Vec3{std::fmax(c.x, 0.0f), std::fmax(c.y, 0.0f), std::fmax(c.z, 0.0f)};
}
// The most chroma the sRGB gamut holds at this lightness and hue.
float gamutChroma(float L, float hueDeg) {
    float lo = 0.0f, hi = 0.4f;
    for (u32 k = 0; k < 18; ++k) {
        const float c = 0.5f * (lo + hi);
        const Vec3 v = oklchRaw(L, hueDeg, c);
        if (v.x < 0.0f || v.y < 0.0f || v.z < 0.0f || v.x > 1.0f || v.y > 1.0f || v.z > 1.0f) hi = c;
        else lo = c;
    }
    return lo;
}
// The OKLCH hue (degrees) of a linear sRGB colour.
float oklchHue(Vec3 c) {
    const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    const float a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    const float b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    const float h = std::atan2(b, a) * 360.0f / kTau;
    return h < 0.0f ? h + 360.0f : h;
}
// minibosses.md: the family's tier-5 body.
Family miniFamily(BossKind k) {
    switch (k) {
        case BossKind::AlphaHound: return Family::Hound;
        case BossKind::Hive: return Family::Shoal;
        case BossKind::BatteringRam: return Family::Ram;
        case BossKind::MothQueen: return Family::Moth;  // M5 part 4: batch 2
        case BossKind::BroodMother: return Family::Brood;
        case BossKind::BlinkStar: return Family::Blink;
        case BossKind::SentryArray: return Family::Sentry;
        case BossKind::MirrorMimic: return Family::Mimic;  // M5 part 6: batch 3
        case BossKind::TwinTether: return Family::Tether;
        case BossKind::WeaverPrime: return Family::Weaver;
        case BossKind::DeepWell: return Family::Well;
        default: return Family::Carom;
    }
}
}  // namespace

void PlayView::fightChevron(FrameDraw& d, const ViewInput& in, Vec2 at, Vec3 colour) {
    // look.md 6: off screen a boss (and a mini-boss) gets the double chevron, swelling on every beat.
    const float tps = std::fmax(in.bpm, 60.0f) * static_cast<float>(kTicksPerBeat) / 60.0f;
    const float sinceBeat = static_cast<float>(std::fmod(in.music, kTicksPerBeat)) / tps;
    const float beat = std::exp(-sinceBeat * 1000.0f / std::fmax(look_->kickDecayMs, 1.0f));
    edgeChevron(d, at, colour, true, 1.0f + look_->edge.bossBeatSwell * beat);
}

void PlayView::drawFight(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    drawPen(d, in);
    // The lines a fight has shown (a pack's charge, the Hive's spear): dashed in the danger colour until their moment.
    for (FightLine& l : fightLines_) {
        if (l.until < in.music || l.until - in.music > 3.0 * kTicksPerBeat) continue;
        telegraphLine(d, l.from, l.from + l.dir * l.length, 1.0f);
    }
    const BossState& s = w.boss();
    if (!s.active) {
        deathShown_ = 0;
        return;
    }
    switch (s.kind) {
        case BossKind::Kaleidos: drawBoss(d, in); break;
        case BossKind::Longwave: drawLongwave(d, in); break;
        case BossKind::Choir: drawChoir(d, in); break;
        case BossKind::EventHorizon: drawHorizon(d, in); break;
        case BossKind::Polyrhythm: drawPoly(d, in); break;
        case BossKind::Feedback: drawFeedback(d, in); break;
        case BossKind::Cadence: drawCadence(d, in); break;
        case BossKind::Arpeggiator: drawArpeggiator(d, in); break;
        case BossKind::Morgana: drawMorgana(d, in); break;
        case BossKind::Reprise: drawReprise(d, in); break;
        default: drawMini(d, in); break;
    }
}

// ---------------------------------------------------------------------------------------------- Longwave

void PlayView::drawLongwave(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const LongwaveData& L = w.species().longwave;
    const float a = in.alpha;
    const double t = in.music;
    const Vec3 W1 = whiteV();
    const Vec3 acc = oklch(L.accentL, L.accentHue, L.accentChroma * 0.16f);
    const Vec3 danger = palettes_->danger;
    const float hw = 0.6f * refPx_;
    const Vec2 head = lerpv(s.bodyPrev[0], s.body[0], a);
    if (!visible(head, 300.0f) && !s.dying) fightChevron(d, in, head, acc);
    // The death unravels the body tail-first, one segment per sixteenth (each a medium kill), then the head breaks.
    const u32 live = bitsIn(s.alive);
    u32 shown = live;
    if (s.dying) {
        const u32 gone = static_cast<u32>(std::floor((t - s.deathTick) / kTicksPerStep));
        shown = gone >= live ? 0u : live - gone;
        if (gone > live + 1) return;
    }
    const double wt = s.dying ? s.deathTick : t;  // the body freezes where it died
    const float total = static_cast<float>(L.segments);
    // The colour along the body: enemy hue 1 at the head, 2 in the middle, 3 at the tail.
    const auto bodyHue = [&](float k) {
        const float f = clamp01(k / total) * 2.0f;
        return f < 1.0f ? mix(palette_->hue[0], palette_->hue[1], f) : mix(palette_->hue[1], palette_->hue[2], f - 1.0f);
    };
    // The echoes (the wave's after-images, an eighth and a quarter of a wave behind) and the spine: polylines through the
    // body's places, kept on every tier.
    const float end = static_cast<float>(shown) + (shown == live ? 0.9f : 0.0f);
    for (u32 e = 0; e < 3; ++e) {
        const float shift = e < 2 ? L.echoBehind[e] : 0.0f;
        const float level = e < 2 ? L.echoIntensity[e] * 2.0f : 2.2f;
        Vec2 prev = w.longwavePoint(wt, 0.0f, shift);
        for (float k = 0.25f; k <= end + 1e-3f; k += 0.25f) {
            const Vec2 p = w.longwavePoint(wt, k, shift);
            if (visible(p, 60.0f) || visible(prev, 60.0f)) {
                if (e == 2) d.scene.line(toScene(prev), toScene(p), 0.5f * L.spineWidthPx * refPx_, bodyHue(k) * 1.4f, bodyHue(k) * 1.4f, 0);
                d.scene.line(toScene(prev), toScene(p), 0.5f * (e == 2 ? L.spineCoreWidthPx : L.spineWidthPx) * refPx_, acc * level, acc * level, 0);
            }
            prev = p;
        }
    }
    // The segments: lozenges drawn as elites (a white copy of the hull, glass), the crest white-hot, its neighbours warming
    // and cooling, a trough about to fire on a pool of the danger colour.
    u32 rank = 0;
    for (u32 k = 0; k < L.segments; ++k) {
        if (!(s.alive & (1u << k))) continue;
        ++rank;
        if (rank > shown) break;
        const Vec2 p = lerpv(s.bodyPrev[1 + k], s.body[1 + k], a);
        if (!visible(p, 120.0f)) continue;
        Vec2 tan;
        float wv = 0.0f;
        w.longwavePoint(wt, static_cast<float>(rank), 0.0f, &tan, &wv);
        const float c = tan.x, sn = tan.y;
        const bool crest = !s.dying && w.longwaveCrest(k, t) >= 0;
        const float heat = crest ? 1.0f : (wv > 0.0f ? wv * wv * wv : 0.0f);
        const Vec3 hue = bodyHue(static_cast<float>(rank)), partner = palette_->partner[rank * 3 / (L.segments + 1)];
        for (u32 j = 0; j < L.segLineCount; ++j) {
            const BossLine& ln = L.segLines[j];
            const Vec2 p0 = p + rot(Vec2{ln.x0, ln.y0}, c, sn), p1 = p + rot(Vec2{ln.x1, ln.y1}, c, sn);
            const Vec3 col = ln.layer == 2 ? acc : (ln.layer == 1 ? partner : hue);
            const float lvl = ln.layer == 0 ? 2.0f : 1.4f;
            d.scene.line(toScene(p0), toScene(p1), hw * (ln.layer == 0 ? 1.3f : 1.0f), col * lvl, col * lvl, 0);
            if (ln.layer == 0) {
                const float white = 0.9f + 1.7f * heat;  // the elite's white copy; the crest's at 2.6
                d.scene.line(toScene(p0), toScene(p1), hw * 0.6f, W1 * white, W1 * white, 0);
                // Glass: the hull's fan from the centre.
                const Vec3 gl = crest ? W1 * 0.18f : hue * 0.085f;
                d.glass.tri(toScene(p), gl, toScene(p0), gl, toScene(p1), gl);
            }
        }
        // The fins: one blade a flank; the one the segment swings toward opens as it crests.
        for (float side : {1.0f, -1.0f}) {
            const float open = side * wv > 0.0f ? std::fabs(wv) * std::fabs(wv) * std::fabs(wv) : 0.0f;
            const float reach = L.finClosed + (L.finCrest - L.finClosed) * open;
            const Vec2 b0 = p + rot(Vec2{-4.0f, 21.0f * side}, c, sn), b1 = p + rot(Vec2{-30.0f, (21.0f + reach) * side}, c, sn),
                       b2 = p + rot(Vec2{12.0f, 21.0f * side}, c, sn);
            const Vec3 fc = crest && side * wv > 0.0f ? W1 * 2.2f : acc * 1.6f;
            d.scene.line(toScene(b0), toScene(b1), hw, fc, fc, 0);
            d.scene.line(toScene(b1), toScene(b2), hw, fc, fc, 0);
        }
        if (crest) {
            flare(d.scene, p, angleOf(Vec2{-sn, c}), 70.0f, 2, W1 * 2.4f, 0.8f);
            flare(d.scene, p, angleOf(tan), 30.0f, 2, W1 * 1.6f, 0.6f);
            d.sparkles.glow(toScene(p), 14.0f * refPx_, W1 * 1.0f);
        }
        if (!s.dying && wv < -0.95f) d.pools.glow(toScene(p), 46.0f * pxPerWu_, danger * 0.32f);
    }
    // The tail: three blades in a fan at the end of the body.
    if (shown == live && !s.dying) {
        const Vec2 tp = lerpv(s.bodyPrev[kBossBodies - 1], s.body[kBossBodies - 1], a);
        const float ta = s.bodyAng[kBossBodies - 1];
        const float c = std::cos(ta), sn = std::sin(ta);
        for (u32 j = 0; j < 3; ++j) {
            const Vec2 p0 = tp + rot(Vec2{L.tailBlades[j][0], L.tailBlades[j][1]}, c, sn), p1 = tp + rot(Vec2{L.tailBlades[j][2], L.tailBlades[j][3]}, c, sn);
            const Vec3 col = bodyHue(total) * 1.8f;
            d.scene.line(toScene(p0), toScene(p1), hw * 1.2f, col, col * 0.6f, 0);
        }
    }
    // The head: the jaw (open on beat 1 and after a lunge; locked open in death), the crown, the eyes, the seed.
    if (visible(head, 160.0f) && (!s.dying || t - s.deathTick < (live + 1) * kTicksPerStep)) {
        const float ang = s.bodyAng[0];
        const float c = std::cos(ang), sn = std::sin(ang);
        const bool open = s.dying || w.longwaveJawOpen(t);
        const Vec3 hue = palette_->hue[0];
        for (u32 j = 0; j < L.jawCount; ++j) {
            const BossLine& ln = open ? L.jawOpen[j] : L.jawClosed[j];
            const Vec2 p0 = head + rot(Vec2{ln.x0, ln.y0}, c, sn), p1 = head + rot(Vec2{ln.x1, ln.y1}, c, sn);
            d.scene.line(toScene(p0), toScene(p1), hw * 1.4f, hue * 2.2f, hue * 2.2f, 0);
            d.scene.line(toScene(p0), toScene(p1), hw * 0.6f, W1 * 1.1f, W1 * 1.1f, 0);
        }
        for (u32 j = 0; j < L.crownCount; ++j) {
            const BossLine& ln = L.crown[j];
            d.scene.line(toScene(head + rot(Vec2{ln.x0, ln.y0}, c, sn)), toScene(head + rot(Vec2{ln.x1, ln.y1}, c, sn)), hw, acc * 1.8f, acc * 1.8f, 0);
        }
        for (const auto& e : L.eyes) d.sparkles.glow(toScene(head + rot(Vec2{e[0], e[1]}, c, sn)), 7.0f * refPx_, acc * 1.6f);
        const Vec2 seed = head + rot(Vec2{L.seedX, 0.0f}, c, sn);
        const Vec3 gold = palettes_->player[3];
        d.sparkles.glow(toScene(seed), (open ? 16.0f : 7.0f) * refPx_, open ? gold * 2.0f : gold * 0.6f);
        if (open && !s.dying) d.pools.glow(toScene(seed), 60.0f * pxPerWu_, gold * 0.22f);
    }
    // The lunge: a dashed line from the head through where the nearest player stood, for its telegraph.
    const LongwaveState& lw = s.lw;
    if (!s.dying && lw.lungeArmed && t >= lw.lungeTelegraph && t < lw.lungeTick) {
        const Vec2 from = head;
        const LongwaveData::Phase& P = L.phases[lw.phase < 4 ? lw.phase : 3];
        telegraphLine(d, from, from + lw.lungeDir * (P.lungeDashWu > 0.0f ? P.lungeDashWu + 300.0f : 1000.0f), 1.0f);
    }
}

// ---------------------------------------------------------------------------------------------- The Choir

void PlayView::drawChoir(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const ChoirData& C = w.species().choir;
    const float a = in.alpha;
    const double t = in.music;
    const Vec3 W1 = whiteV();
    const Vec3 danger = palettes_->danger;
    const Vec3 pearl = oklch(C.pearlL, C.pearlHue, C.pearlChroma * gamutChroma(C.pearlL, C.pearlHue));
    const Vec3 gold = palettes_->player[3];
    const float hw = 0.6f * refPx_;
    if (choirHueFor_ != palette_) {
        // Eight hues 45 degrees apart from the Track's first enemy hue (lightness 0.80, 96% of the gamut's chroma); a hue
        // within 25 degrees of the danger colour moves to the nearer edge of that band.
        choirHueFor_ = palette_;
        const float h0 = oklchHue(palette_->hue[0]), hd = oklchHue(danger);
        for (u32 k = 0; k < 8; ++k) {
            float h = std::fmod(h0 + 45.0f * static_cast<float>(k), 360.0f);
            float dd = h - hd;
            while (dd > 180.0f) dd -= 360.0f;
            while (dd < -180.0f) dd += 360.0f;
            if (std::fabs(dd) < 25.0f) h = hd + (dd >= 0.0f ? 25.0f : -25.0f);
            choirHue_[k] = oklch(0.80f, h, 0.96f * gamutChroma(0.80f, h));
        }
    }
    const Vec2 centre = lerpv(s.prevPos, s.pos, a);
    if (!visible(centre, 500.0f) && !s.dying) fightChevron(d, in, centre, choirHue_[0]);
    // The death: the hoods return as outlines in their own hues for a bar, every string white, then shatter clockwise one
    // every eighth.
    const double sinceDeath = s.dying ? t - s.deathTick : -1.0;
    const u32 shattered = s.dying && sinceDeath > kTicksPerBar ? static_cast<u32>((sinceDeath - kTicksPerBar) / (kTicksPerBeat / 2.0)) + 1u : 0u;
    const u32 drawn = s.dying ? 0xFFu : s.alive;
    const u8 nowSing = s.dying ? 0 : w.choirSingers(std::floor(t / 48.0) * 48.0);
    // The answer: the next Voice to sing in the coming beat (it draws breath).
    u8 next = 0;
    for (u32 e = 1; e <= 2 && !next && !s.dying; ++e) next = static_cast<u8>(w.choirSingers((std::floor(t / 48.0) + e) * 48.0) & ~nowSing);
    const auto voicePos = [&](u32 k) { return lerpv(s.bodyPrev[1 + k], s.body[1 + k], a); };
    // Neighbour strings (the octagon): pearl at rest, shaded between the two Voices' hues; the cage's dashes and burn.
    const u8 cage = s.dying ? 0 : w.choirCage(t);
    for (u32 k = 0; k < 8; ++k) {
        const u32 n = (k + 1) % 8;
        if (!(drawn & (1u << k)) || !(drawn & (1u << n))) continue;
        if (s.dying && (k < shattered || n < shattered)) continue;
        const Vec2 p0 = voicePos(k), p1 = voicePos(n);
        if (!visible(p0, 200.0f) && !visible(p1, 200.0f)) continue;
        if (cage == 1) {
            telegraphLine(d, p0, p1, 1.0f, w.bossData().phases[1].beamWidth);
            continue;
        }
        if (cage == 2) {
            lethalBeam(d, p0, p1, w.bossData().phases[1].beamWidth);
            continue;
        }
        const Vec2 mid = (p0 + p1) * 0.5f + perpOf(normalizeOr(p1 - p0, Vec2{1.0f, 0.0f})) * (C.neighbourShimmer * std::sin(static_cast<float>(t) * 0.05f + k));
        const Vec3 c0 = s.dying ? W1 * 1.6f : mix(pearl, choirHue_[k], 0.35f) * 1.2f, c1 = s.dying ? W1 * 1.6f : mix(pearl, choirHue_[n], 0.35f) * 1.2f;
        const float wpx = 0.5f * C.neighbourWidthPx * refPx_;
        d.scene.line(toScene(p0), toScene(mid), wpx, c0, mix(c0, c1, 0.5f), 0);
        d.scene.line(toScene(mid), toScene(p1), wpx, mix(c0, c1, 0.5f), c1, 0);
    }
    // The call string: a standing wave across the Round between the singer and the Voice that will answer it (three
    // antinodes, its two envelopes), the singer's hue to white to the answerer's; light only.
    if (nowSing && next && !s.dying) {
        u32 from = 0, to = 0;
        while (!(nowSing & (1u << from))) ++from;
        while (!(next & (1u << to))) ++to;
        const Vec2 p0 = voicePos(from), p1 = voicePos(to);
        const Vec2 n = perpOf(normalizeOr(p1 - p0, Vec2{1.0f, 0.0f}));
        const float decay = 1.0f - clamp01(static_cast<float>(std::fmod(t, 48.0) / (C.callDecayBeats * kTicksPerBeat)));
        const float amp = C.callAmplitude * decay * std::cos(static_cast<float>(t) * 0.12f);
        const u32 segs = 24;
        for (float sign : {1.0f, -1.0f}) {
            Vec2 prev = p0;
            for (u32 j = 1; j <= segs; ++j) {
                const float u = static_cast<float>(j) / segs;
                const Vec2 q = lerpv(p0, p1, u) + n * (sign * amp * std::sin(kPi * static_cast<float>(C.callAntinodes) * u));
                const Vec3 col = u < 0.5f ? mix(choirHue_[from], W1, u * 2.0f) : mix(W1, choirHue_[to], (u - 0.5f) * 2.0f);
                d.scene.line(toScene(prev), toScene(q), 0.5f * C.callWidthPx * refPx_, col * 1.4f, col * 1.4f, 0);
                prev = q;
            }
        }
    }
    // The Voices.
    for (u32 k = 0; k < 8; ++k) {
        if (!(drawn & (1u << k))) continue;
        if (s.dying && k < shattered) continue;
        const Vec2 p = voicePos(k);
        if (!visible(p, 220.0f)) continue;
        const float ang = s.bodyAng[1 + k];
        const float c = std::cos(ang), sn = std::sin(ang);
        const bool singing = !s.dying && w.choirSinging(k, t);
        const bool breath = !singing && (next & (1u << k));
        const Vec3 hue = choirHue_[k];
        // choir.md (94706dc): a silent hood is drawn at 0.6 of its level (it does not hurt); a singing one at full.
        const float outline = s.dying ? 1.6f : (singing || C.hoodHurts == ChoirData::HoodHurts::Always ? 1.0f : 0.6f);
        // The hood: a pointed polyline, closed by the lips.
        for (u32 j = 0; j + 1 < C.hoodCount; ++j) {
            const Vec2 p0 = p + rot(Vec2{C.hood[j][0], C.hood[j][1]}, c, sn), p1 = p + rot(Vec2{C.hood[j + 1][0], C.hood[j + 1][1]}, c, sn);
            d.scene.line(toScene(p0), toScene(p1), hw * 1.3f, hue * (2.0f * outline), hue * (2.0f * outline), 0);
            if (singing) d.scene.line(toScene(p0), toScene(p1), hw * 0.7f, W1 * 2.7f, W1 * 2.7f, 0);
        }
        if (!s.dying) {
            for (const auto& r : C.ribs)
                d.scene.line(toScene(p + rot(Vec2{r[0][0], r[0][1]}, c, sn)), toScene(p + rot(Vec2{r[1][0], r[1][1]}, c, sn)), hw * 0.8f, hue * 1.2f,
                             hue * 1.2f, 0);
            // The lips: open while it sings, a third open as it draws breath.
            const float open = singing ? 1.0f : (breath ? 0.33f : 0.0f);
            for (u32 j = 0; j < 2; ++j) {
                const Vec2 q0 = lerpv(Vec2{C.mouthClosed[j][0][0], C.mouthClosed[j][0][1]}, Vec2{C.mouthOpen[j][0][0], C.mouthOpen[j][0][1]}, open);
                const Vec2 q1 = lerpv(Vec2{C.mouthClosed[j][1][0], C.mouthClosed[j][1][1]}, Vec2{C.mouthOpen[j][1][0], C.mouthOpen[j][1][1]}, open);
                d.scene.line(toScene(p + rot(q0, c, sn)), toScene(p + rot(q1, c, sn)), hw * 1.2f, singing ? W1 * 2.4f : hue * 1.8f, singing ? W1 * 2.4f : hue * 1.8f, 0);
            }
            // The face (pearl) and its core (gold while it sings: the weak point).
            const Vec2 fc = p + rot(Vec2{C.faceX, 0.0f}, c, sn);
            for (u32 j = 0; j < C.faceSides; ++j) {
                const float a0 = kTau * static_cast<float>(j) / C.faceSides, a1 = kTau * static_cast<float>(j + 1) / C.faceSides;
                d.scene.line(toScene(fc + dirv(a0) * C.faceRadius), toScene(fc + dirv(a1) * C.faceRadius), hw * 0.8f, pearl * 1.4f, pearl * 1.4f, 0);
            }
            d.sparkles.glow(toScene(p + rot(Vec2{C.coreX, 0.0f}, c, sn)), (singing ? 14.0f : 6.0f) * refPx_, singing ? gold * 2.2f : pearl * 0.8f);
            if (breath) flare(d.scene, fc, 0.0f, 22.0f, 4, hue * 1.6f, 0.5f);
            // The halo behind the hood, brighter with its own note.
            const u32 n = 10;
            const float h0 = C.haloFromDeg * kTau / 360.0f, h1 = C.haloToDeg * kTau / 360.0f;
            for (u32 j = 0; j < n; ++j) {
                const float a0 = ang + h0 + (h1 - h0) * j / n, a1 = ang + h0 + (h1 - h0) * (j + 1) / n;
                const float lvl = singing ? 2.0f : 0.9f;
                d.scene.line(toScene(p + dirv(a0) * C.haloRadius), toScene(p + dirv(a1) * C.haloRadius), hw * 0.8f, pearl * lvl, pearl * lvl, 0);
            }
            if (singing) {
                // White-hot: glass, a two-arm flare across the mouth, three arcs of its hue leaving the mouth.
                const Vec2 mouth = p + rot(Vec2{C.mouthOpen[0][1][0], 0.0f}, c, sn);
                flare(d.scene, mouth, ang + 0.5f * kPi, 50.0f, 2, W1 * 2.2f, 0.7f);
                const float u = clamp01(static_cast<float>(std::fmod(t, kTicksPerBeat) / kTicksPerBeat));
                const float half = C.waveHalfDeg * kTau / 360.0f;
                for (float r : C.waveRadii) {
                    const float rr = r + 40.0f * u;
                    for (u32 j = 0; j < 6; ++j) {
                        const float a0 = ang - half + 2.0f * half * j / 6.0f, a1 = ang - half + 2.0f * half * (j + 1) / 6.0f;
                        d.scene.line(toScene(p + dirv(a0) * rr), toScene(p + dirv(a1) * rr), hw * 0.8f, hue * (1.6f * (1.0f - u)), hue * (1.6f * (1.0f - u)), 0);
                    }
                }
                const Vec2 hp0 = p + rot(Vec2{C.hood[0][0], C.hood[0][1]}, c, sn);
                for (u32 j = 1; j + 1 < C.hoodCount; ++j) {
                    const Vec3 gl = W1 * 0.16f;
                    d.glass.tri(toScene(hp0), gl, toScene(p + rot(Vec2{C.hood[j][0], C.hood[j][1]}, c, sn)), gl,
                                toScene(p + rot(Vec2{C.hood[j + 1][0], C.hood[j + 1][1]}, c, sn)), gl);
                }
            }
        }
        // The held note: a dashed line for a bar, then the beam from the open mouth sweeping 120 degrees.
        float beamAng = 0.0f;
        const u8 held = s.dying ? 0 : w.choirHeld(k, t, &beamAng);
        if (held) {
            const Vec2 dv = dirv(beamAng);
            float reach = 12000.0f;
            if (dv.x > 1e-4f) reach = std::fmin(reach, (kArenaHalfW - p.x) / dv.x);
            if (dv.x < -1e-4f) reach = std::fmin(reach, (-kArenaHalfW - p.x) / dv.x);
            if (dv.y > 1e-4f) reach = std::fmin(reach, (kArenaHalfH - p.y) / dv.y);
            if (dv.y < -1e-4f) reach = std::fmin(reach, (-kArenaHalfH - p.y) / dv.y);
            const Vec2 to = p + dv * reach;
            if (held == 1) {
                for (float u = 0.0f; u < reach; u += 60.0f) {
                    const Vec2 q0 = p + dv * u, q1 = p + dv * std::fmin(u + 30.0f, reach);
                    if (visible(q0, 40.0f) || visible(q1, 40.0f)) telegraphLine(d, q0, q1, 1.0f, w.bossData().phases[1].beamWidth);
                }
            } else {
                lethalBeam(d, p, to, w.bossData().phases[1].beamWidth);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- mini-bosses

void PlayView::drawMini(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const MiniData::Entry* me = w.species().minis.find(s.kind);
    if (!me) return;
    const float a = in.alpha;
    const double t = in.music;
    const Vec3 W1 = whiteV();
    const Vec3 danger = palettes_->danger;
    const Vec3 gold = palettes_->player[3];
    const Family fam = miniFamily(s.kind);
    const u16 sp = SpeciesTable::index(fam, 5, kVariantA);
    const Species& spec = w.species().get(sp);
    const Vec3 hue = palette_->hue[spec.hue], partner = palette_->partner[spec.hue];
    const int g = speciesGlyph_[sp];
    const MiniState& m = s.mini;
    const Vec2 body = lerpv(s.bodyPrev[0], s.body[0], a);
    if (!visible(body, 2.0f * me->radius) && !s.dying) fightChevron(d, in, body, hue);
    const float die = s.dying ? clamp01(static_cast<float>((t - s.deathTick) / (0.25 * kTicksPerBar))) : 0.0f;
    if (die >= 1.0f) return;
    const float life = 1.0f - die;
    const float enter = clamp01(static_cast<float>((t - s.enterTick) / (kWarpBeats * kTicksPerBeat)));
    // Its family's tier-5 body enlarged, one more ring of ornament, the white equipment marks.
    const auto drawBody = [&](Vec2 p, float radius, float ang, Vec3 h, Vec3 pt, float level, bool white) {
        if (!visible(p, 2.5f * radius)) return;
        const float spin = static_cast<float>(t / kTicksPerBar) * 0.5f;
        glyph(d.glyphs, g, false, p, ang, ang + spin, radius, level * 1.4f, 0.5f * look_->lines.enemyHullTier1.width * 1.3f, kGlyphEnemy, h, pt);
        glyph(d.glyphs, g, true, p, ang, ang, radius, level * (white ? 2.4f : 0.9f), 0.5f * look_->lines.enemyHullTier1.width * 0.7f, kGlyphWhite, W1, W1);
        glyph(d.glyphs, gRing64_, false, p, spin, spin, radius * 1.35f, level * 0.9f, 0.5f * look_->lines.enemyHullTier1.width, kGlyphFlat, h, pt);
        glyph(d.glyphs, gMarkC_, false, p, ang, ang, radius, level * 1.2f, 0.5f * look_->lines.variantMark.width, kGlyphWhite, W1, W1);
    };
    const float level = life * (0.3f + 0.7f * enter);
    switch (s.kind) {
        case BossKind::AlphaHound: {
            const bool winded = t < m.winded;
            drawBody(body, me->radius, s.bodyAng[0], winded ? mix(hue, W1, 0.4f) : hue, partner, level * (winded ? 0.7f : 1.0f), winded);
            if (winded) d.sparkles.glow(toScene(body), 20.0f * refPx_, gold * 1.6f);
            if (m.state == 1) telegraphLine(d, body, body + m.lockDir * 1000.0f, 1.0f);
            break;
        }
        case BossKind::Hive: {
            // The Queen in gold; after the half a white decoy flies with the other hive.
            const bool tell = t >= m.cueTick && t < m.cueTick + kTicksPerBeat;
            drawBody(body, me->radius * 1.6f, s.bodyAng[0], gold, gold, level * (tell ? 1.6f : 1.0f), tell);
            d.sparkles.glow(toScene(body), (tell ? 18.0f : 10.0f) * refPx_, gold * (tell ? 2.4f : 1.4f));
            if (m.hives > 1) {
                const Vec2 decoy = lerpv(s.bodyPrev[1], s.body[1], a);
                drawBody(decoy, me->radius * 1.6f, s.bodyAng[1], W1, W1, level, false);
            }
            break;
        }
        case BossKind::BatteringRam: {
            const bool stunned = m.state == 4;
            const float ang = s.bodyAng[0];
            drawBody(body, me->radius, ang, stunned ? mix(hue, W1, 0.3f) : hue, partner, level * (stunned ? 0.75f : 1.0f), stunned);
            // Its horns: the front 120 degrees sheds every shot (an arc in the boss's accent, the danger colour).
            if (!stunned) {
                const float half = 0.5f * prose::kRamFrontDeg * kTau / 360.0f;
                for (u32 j = 0; j < 8; ++j) {
                    const float a0 = ang - half + 2.0f * half * j / 8.0f, a1 = ang - half + 2.0f * half * (j + 1) / 8.0f;
                    d.scene.line(toScene(body + dirv(a0) * (me->radius * 1.15f)), toScene(body + dirv(a1) * (me->radius * 1.15f)), 0.9f * refPx_,
                                 W1 * 1.6f * level, W1 * 1.6f * level, 0);
                }
            } else {
                d.sparkles.glow(toScene(body), 22.0f * refPx_, gold * 1.8f);
            }
            if (m.state == 1) {
                // The lock: a danger line from its horns through the player to the Pen's wall.
                float reach = 1600.0f;
                const PenState& pen = w.pen();
                if (pen.up) {
                    const Vec2 dv = m.lockDir;
                    if (dv.x > 1e-4f) reach = std::fmin(reach, (pen.centre.x + pen.half - body.x) / dv.x);
                    if (dv.x < -1e-4f) reach = std::fmin(reach, (pen.centre.x - pen.half - body.x) / dv.x);
                    if (dv.y > 1e-4f) reach = std::fmin(reach, (pen.centre.y + pen.half - body.y) / dv.y);
                    if (dv.y < -1e-4f) reach = std::fmin(reach, (pen.centre.y - pen.half - body.y) / dv.y);
                }
                telegraphLine(d, body + m.lockDir * me->radius, body + m.lockDir * reach, 1.0f);
            }
            if (m.wake && (t < m.wakeUntil || m.state == 2)) {
                d.scene.line(toScene(m.wakeFrom), toScene(m.wakeTo), 0.5f * tuning::kRamWakeWu * pxPerWu_, danger * 2.2f, danger * 2.2f, 0);
                d.threatCores.line(toScreen(m.wakeFrom), toScreen(m.wakeTo), 0.35f * tuning::kRamWakeWu * pxPerWu_, danger, danger, 0);
            }
            break;
        }
        case BossKind::MothQueen: {
            // Open for her fourth bar: wings spread (two gold flares), core gold.
            const bool open = w.mothOpen(t);
            drawBody(body, me->radius, s.bodyAng[0], open ? mix(hue, W1, 0.3f) : hue, partner, level, open);
            if (open) {
                d.sparkles.glow(toScene(body), 18.0f * refPx_, gold * 2.2f);
                flare(d.scene, body, s.bodyAng[0] + 0.5f * kPi, 2.2f * me->radius, 1, gold * 1.8f, 0.8f);
            }
            break;
        }
        case BossKind::BroodMother: {
            // The dome (shut while a broodling lives: a thread of light from each to her); the eggs, white, pulsing on the beats.
            const bool shut = m.shut;
            drawBody(body, me->radius, s.bodyAng[0], hue, partner, level, !shut);
            const float beat = std::exp(-static_cast<float>(std::fmod(t, kTicksPerBeat)) / 24.0f);
            const Enemies& e = w.enemies();
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled)) continue;
                const Vec2 p{e.x[i], e.y[i]};
                if (e.state[i] == kEgg) {
                    if (!visible(p, 30.0f)) continue;
                    const u32 n = 10;
                    for (u32 j = 0; j < n; ++j)
                        d.scene.line(toScene(p + dirv(kTau * j / n) * 13.0f), toScene(p + dirv(kTau * (j + 1) / n) * 13.0f), 1.0f * refPx_,
                                     W1 * (1.4f + 1.2f * beat), W1 * (1.4f + 1.2f * beat), 0);
                } else if (e.state[i] != kWarping && (visible(p, 20.0f) || visible(body, 60.0f))) {
                    d.scene.line(toScene(p), toScene(body), 0.4f * refPx_, hue * 0.7f, partner * 0.4f, 0);  // its thread
                }
            }
            const float dome = me->radius * 1.3f;
            const u32 n = 36;
            for (u32 j = 0; j < n; ++j) {
                if (!shut && (j % 3) == 0) continue;  // broken
                d.scene.line(toScene(body + dirv(kTau * j / n) * dome), toScene(body + dirv(kTau * (j + 1) / n) * dome), 0.9f * refPx_,
                             shut ? partner * 1.4f : W1 * 0.5f, shut ? partner * 1.4f : W1 * 0.5f, 0);
            }
            if (!shut) d.sparkles.glow(toScene(body), 20.0f * refPx_, gold * 2.2f);
            break;
        }
        case BossKind::BlinkStar: {
            // The star: each edge left behind as a dashed danger line; all five lethal for a beat; dazed (gold) after. From
            // half health a white decoy at every point. The first point is marked from the first beat.
            static const u32 kOrder[5] = {0, 2, 4, 1, 3};
            const bool dazed = w.blinkDazed(t);
            drawBody(body, me->radius, s.bodyAng[0], dazed ? mix(hue, W1, 0.3f) : hue, partner, level * (dazed ? 0.8f : 1.0f), dazed);
            if (dazed) d.sparkles.glow(toScene(body), 20.0f * refPx_, gold * 2.2f);
            if (m.ringTick >= 0.0 && !s.dying) {
                const u32 edges = w.blinkEdges(t);
                const bool firing = w.blinkFiring(t);
                for (u32 k = 0; k < 5; ++k) {
                    const Vec2 a0 = w.blinkPoint(kOrder[k]), a1 = w.blinkPoint(kOrder[(k + 1) % 5]);
                    if (firing) {
                        lethalBeam(d, a0, a1, prose::kBsBeamWu);
                    } else if (k < edges) {
                        for (float u = 0.0f; u < 1.0f; u += 0.08f)
                            telegraphLine(d, lerpv(a0, a1, u), lerpv(a0, a1, std::fmin(u + 0.04f, 1.0f)), 1.0f, prose::kBsBeamWu);
                    }
                }
                const Vec2 first = w.blinkPoint(0);
                const u32 n = 16;
                for (u32 j = 0; j < n; ++j)
                    d.scene.line(toScene(first + dirv(kTau * j / n) * 24.0f), toScene(first + dirv(kTau * (j + 1) / n) * 24.0f), 0.8f * refPx_, W1 * 1.4f,
                                 W1 * 1.4f, 0);
                if (m.half)
                    for (u32 k = 0; k < 5; ++k) {
                        const Vec2 p = w.blinkPoint(k);
                        if (length(p - body) > 10.0f) drawBody(p, me->radius * 0.6f, s.bodyAng[0], W1, W1, level * 0.5f, false);
                    }
            }
            break;
        }
        case BossKind::SentryArray: {
            // Eight turrets in a fenced ring; domes join each to its neighbours; the turret under the hand has no dome (gold,
            // open); the hand: a white line from the centre.
            const i32 hand = w.sentryHand();
            const Vec2 c = m.ringAt;
            u32 order[8];
            u32 n = 0;
            for (u32 k = 0; k < 8; ++k)
                if (s.alive & (1u << k)) order[n++] = k;
            for (u32 j = 0; j < n && !s.dying; ++j) {
                const Vec2 p0 = lerpv(s.bodyPrev[1 + order[j]], s.body[1 + order[j]], a), p1 = lerpv(s.bodyPrev[1 + order[(j + 1) % n]], s.body[1 + order[(j + 1) % n]], a);
                if (n > 1) d.scene.line(toScene(p0), toScene(p1), 1.0f * refPx_, W1 * 1.2f, W1 * 1.2f, 0);
            }
            for (u32 j = 0; j < n; ++j) {
                const u32 k = order[j];
                const Vec2 p = lerpv(s.bodyPrev[1 + k], s.body[1 + k], a);
                const bool open = static_cast<i32>(k) == hand;
                drawBody(p, me->radius, s.bodyAng[1 + k], hue, partner, level, open);
                if (open) {
                    d.sparkles.glow(toScene(p), 16.0f * refPx_, gold * 2.2f);
                } else {
                    const u32 seg = 20;
                    for (u32 q = 0; q < seg; ++q)
                        d.scene.line(toScene(p + dirv(kTau * q / seg) * (me->radius * 1.3f)), toScene(p + dirv(kTau * (q + 1) / seg) * (me->radius * 1.3f)),
                                     0.7f * refPx_, W1 * 0.9f, W1 * 0.9f, 0);
                }
            }
            if (!s.dying && t >= s.fightTick) {
                const Vec2 tip = c + dirv(w.sentryHandAngle()) * (prose::kSaRingWu - me->radius * 1.4f);
                d.scene.line(toScene(c), toScene(tip), 1.2f * refPx_, W1 * 2.2f, W1 * 2.2f, 0);
                d.sparkles.glow(toScene(c), 8.0f * refPx_, W1 * 1.6f);
            }
            d.pools.glow(toScene(c), prose::kSaRingWu * 2.0f * pxPerWu_, hue * (0.05f * life));
            return;
        }
        // ---- M5 part 6: batch 3.
        case BossKind::MirrorMimic: {
            // Its image (and from half health the second, across the level line); gold while caught in its copy; the mirror point
            // a small white ring.
            const bool caught = w.mimicCopying(t);
            const Vec3 h = caught ? mix(hue, W1, 0.4f) : hue;
            drawBody(body, me->radius, s.bodyAng[0], h, partner, level, caught);
            if (m.half) drawBody(lerpv(s.bodyPrev[1], s.body[1], a), me->radius, s.bodyAng[1], h, partner, level, caught);
            if (caught) d.sparkles.glow(toScene(body), 20.0f * refPx_, gold * 2.2f);
            const u32 n = 20;
            for (u32 j = 0; j < n && !s.dying; ++j)
                d.scene.line(toScene(m.ringAt + dirv(kTau * j / n) * tuning::kMmPointWu), toScene(m.ringAt + dirv(kTau * (j + 1) / n) * tuning::kMmPointWu),
                             0.8f * refPx_, W1 * 1.8f, W1 * 1.8f, 0);
            break;
        }
        case BossKind::TwinTether: {
            // Two nodes and the lethal rope between them (gold, the rope dark, while slack); the circle it sweeps, faint.
            const bool slack = w.tetherSlack(t);
            for (u32 k = 0; k < 2; ++k)
                if (s.alive & (1u << k)) drawBody(lerpv(s.bodyPrev[1 + k], s.body[1 + k], a), me->radius, s.bodyAng[1 + k], slack ? mix(hue, W1, 0.4f) : hue, partner, level, slack);
            Vec2 b0, b1;
            if (!s.dying && w.tetherBeam(b0, b1)) {
                lethalBeam(d, b0, b1, tuning::kTtBeamWu);
            } else if (!s.dying && s.alive == 0x3u) {
                const Vec2 p0 = lerpv(s.bodyPrev[1], s.body[1], a), p1 = lerpv(s.bodyPrev[2], s.body[2], a);
                for (float u = 0.0f; u < 1.0f; u += 0.1f)
                    d.scene.line(toScene(lerpv(p0, p1, u)), toScene(lerpv(p0, p1, u + 0.05f)), 0.6f * refPx_, gold * 1.4f, gold * 1.4f, 0);
            }
            if (slack) d.sparkles.glow(toScene(body), 16.0f * refPx_, gold * 1.8f);
            if (s.alive == 0x3u && !s.dying) {
                const u32 n = 48;
                const float r = 0.5f * prose::kTtApart;
                for (u32 j = 0; j < n; ++j)
                    d.scene.line(toScene(body + dirv(kTau * j / n) * r), toScene(body + dirv(kTau * (j + 1) / n) * r), 0.4f * refPx_, hue * 0.35f, hue * 0.35f, 0);
            }
            break;
        }
        case BossKind::WeaverPrime: {
            // The Weaver (and from half health the second, drawing the other way), gold on its knot; its laps' lethal trail on the
            // priority layer, a lap drawn as it goes and gone two bars on; the last cage's laps bursting 40 wu wide as a new one
            // begins.
            for (u32 k = 0; k < (m.half ? 2u : 1u); ++k) {
                const bool knot = w.weaverKnot(k, t);
                drawBody(lerpv(s.bodyPrev[k], s.body[k], a), me->radius, s.bodyAng[k], knot ? mix(hue, W1, 0.4f) : hue, partner, level, knot);
                if (knot) d.sparkles.glow(toScene(lerpv(s.bodyPrev[k], s.body[k], a)), 18.0f * refPx_, gold * 2.0f);
            }
            const auto trail = [&](Vec2 c, float r, float a0, float dir, float u0, float u1, float widthWu, float lv) {
                const u32 seg = 20;
                for (u32 j = 0; j < seg; ++j) {
                    const float s0 = static_cast<float>(j) / seg, s1 = static_cast<float>(j + 1) / seg;
                    if (s1 <= u0 || s0 >= u1) continue;
                    const float q0 = std::fmax(s0, u0), q1 = std::fmin(s1, u1);
                    const Vec2 p0 = c + dirv(a0 + dir * kTau * q0) * r, p1 = c + dirv(a0 + dir * kTau * q1) * r;
                    if (!visible(p0, widthWu) && !visible(p1, widthWu)) continue;
                    if (shown(kMuteBullet)) d.scene.line(toScene(p0), toScene(p1), 0.5f * refPx_, danger * (1.4f * lv), danger * (1.4f * lv), 0);
                    if (shown(kMutePriority)) {
                        d.threatRims.line(toScreen(p0), toScreen(p1), 0.5f * widthWu * pxPerWu_ + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                        d.threatCores.line(toScreen(p0), toScreen(p1), 0.5f * widthWu * pxPerWu_, danger * lv, danger * lv, 0);
                    }
                }
            };
            for (u32 k = 0; k < (m.half ? 2u : 1u) && !s.dying; ++k)
                for (u32 lap = 0; lap < 4; ++lap) {
                    Vec2 c;
                    float r = 0.0f, a0 = 0.0f, dir = 1.0f, u0 = 0.0f, u1 = 0.0f;
                    if (w.weaverLap(k, lap, t, c, r, a0, dir, u0, u1)) trail(c, r, a0, dir, u0, u1, tuning::kWvTrailWu, 1.0f);
                }
            if (!s.dying && m.cageTick >= 0.0 && t >= m.cageTick && t < m.cageTick + tuning::kWvBurstBeats * kTicksPerBeat && lengthSq(m.prevCage) > 0.0f) {
                // The burst is a blast, drawn as an area (the lead's ruling, minibosses.md): two danger edge lines 40 wu apart and
                // a fill of at most 0.12 between them for its half beat, never a filled bar.
                const float lv = 1.0f - clamp01(static_cast<float>((t - m.cageTick) / (tuning::kWvBurstBeats * kTicksPerBeat)));
                const u32 seg = 40;
                for (u32 lap = 2; lap < 4; ++lap)
                    for (u32 k = 0; k < (m.half ? 2u : 1u); ++k) {
                        const float r = prose::kWvRadii[lap] - (k ? 25.0f : 0.0f);
                        const Vec2 c = m.prevCage;
                        for (u32 j = 0; j < seg; ++j) {
                            const float a0 = m.prevCageAng + kTau * static_cast<float>(j) / seg, a1 = m.prevCageAng + kTau * static_cast<float>(j + 1) / seg;
                            if (!visible(c + dirv(a0) * r, 60.0f)) continue;
                            for (float edge : {-0.5f, 0.5f}) {
                                const float re = r + edge * prose::kWvBurstWu;
                                const Vec2 p0 = c + dirv(a0) * re, p1 = c + dirv(a1) * re;
                                if (shown(kMuteBullet)) d.scene.line(toScene(p0), toScene(p1), 0.6f * refPx_, danger * (1.6f * lv), danger * (1.6f * lv), 0);
                                if (shown(kMutePriority)) {
                                    d.threatRims.line(toScreen(p0), toScreen(p1), 1.2f * refPx_ + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                                    d.threatCores.line(toScreen(p0), toScreen(p1), 1.2f * refPx_, danger * lv, danger * lv, 0);
                                }
                            }
                            // The fill: a quad between the edges at most 0.12.
                            const Vec2 q0 = c + dirv(a0) * (r - 0.5f * prose::kWvBurstWu), q1 = c + dirv(a1) * (r - 0.5f * prose::kWvBurstWu);
                            const Vec2 q2 = c + dirv(a1) * (r + 0.5f * prose::kWvBurstWu), q3 = c + dirv(a0) * (r + 0.5f * prose::kWvBurstWu);
                            const Vec3 fill = danger * (0.12f * lv);
                            d.glass.tri(toScene(q0), fill, toScene(q1), fill, toScene(q2), fill);
                            d.glass.tri(toScene(q0), fill, toScene(q2), fill, toScene(q3), fill);
                        }
                    }
            }
            break;
        }
        case BossKind::DeepWell: {
            // Four rings round it lit from the inside out as the bar's fill rises; gold for the bar it is choked (open: x 2); the
            // ground it draws ships from, faint.
            const bool choked = w.wellChoked(t);
            drawBody(body, me->radius, s.bodyAng[0], choked ? mix(hue, W1, 0.3f) : hue, partner, level, choked);
            const float st = w.wellStomach();
            const float fill = st > 0.0f ? clamp01(m.fill / st) : 0.0f;
            for (u32 q = 0; q < 4 && !s.dying; ++q) {
                const bool lit = choked || fill * 4.0f > static_cast<float>(q);
                const float r = me->radius * (1.35f + 0.3f * static_cast<float>(q));
                const Vec3 col = choked ? gold * 1.8f : (lit ? mix(hue, W1, 0.5f) * 1.8f : hue * 0.45f);
                const u32 n = 32;
                for (u32 j = 0; j < n; ++j)
                    d.scene.line(toScene(body + dirv(kTau * j / n) * r), toScene(body + dirv(kTau * (j + 1) / n) * r), 0.7f * refPx_, col * level, col * level, 0);
            }
            if (choked) d.sparkles.glow(toScene(body), 22.0f * refPx_, gold * 2.0f);
            const u32 n = 48;
            for (u32 j = 0; j < n && !s.dying; j += 2)
                d.scene.line(toScene(body + dirv(kTau * j / n) * prose::kDwPull), toScene(body + dirv(kTau * (j + 1) / n) * prose::kDwPull), 0.4f * refPx_,
                             hue * 0.3f, hue * 0.3f, 0);
            break;
        }
        default: {
            // Carom King: a size smaller at each split; gold for a beat after each bounce; its Princes on the half beats.
            const float r = prose::kKingRadii[m.splits < 4 ? m.splits : 3];
            const bool hot = t < m.hot[0];
            drawBody(body, r, s.bodyAng[0], hue, partner, level, hot);
            if (hot) d.sparkles.glow(toScene(body), 20.0f * refPx_, gold * 2.2f);
            for (u32 k = 0; k < 3; ++k) {
                if (!(s.alive & (1u << k))) continue;
                const Vec2 pp = lerpv(s.bodyPrev[1 + k], s.body[1 + k], a);
                drawBody(pp, prose::kPrinceRadius, s.bodyAng[1 + k], hue, partner, level * 0.85f, t < m.hot[1 + k]);
            }
            break;
        }
    }
    if (m.traits && !s.dying && s.kind != BossKind::SentryArray) {
        // M5 part 7: a mini-boss with elite traits (Tracks 9 and 10, the loop) wears the elite's white mark: a ring that swells
        // on the beat, a second in gold when Armored.
        const u32 n = 40;
        const float r0 = me->radius * (1.6f + 0.06f * beat_);
        for (u32 j = 0; j < n; ++j) {
            d.scene.line(toScene(body + dirv(kTau * j / n) * r0), toScene(body + dirv(kTau * (j + 1) / n) * r0), 0.7f * refPx_, W1 * (1.4f * life),
                         W1 * (1.4f * life), 0);
            if (m.traits & traitBit(kArmored))
                d.scene.line(toScene(body + dirv(kTau * j / n) * (r0 + 10.0f)), toScene(body + dirv(kTau * (j + 1) / n) * (r0 + 10.0f)), 0.6f * refPx_,
                             gold * (1.4f * life), gold * (1.4f * life), 0);
        }
    }
    d.pools.glow(toScene(body), me->radius * 2.4f * pxPerWu_, hue * (0.07f * life));
}

void PlayView::drawPen(FrameDraw& d, const ViewInput& in) {
    // minibosses.md: a square of white light walls, rising round the players' centre in one beat; it falls when the fight
    // ends.
    const World& w = *in.world;
    const PenState& pen = w.pen();
    float level = 0.0f;
    if (pen.up) {
        level = clamp01(static_cast<float>((in.music - pen.riseTick) / kTicksPerBeat));
    } else if (pen.fallTick >= 0.0) {
        level = 1.0f - clamp01(static_cast<float>((in.music - pen.fallTick) / kTicksPerBeat));
    }
    if (level <= 0.0f) return;
    const Vec2 c = pen.centre;
    const float h = pen.half;
    const Vec2 corner[4] = {c + Vec2{-h, -h}, c + Vec2{h, -h}, c + Vec2{h, h}, c + Vec2{-h, h}};
    const Vec3 W1 = whiteV();
    const float beat = std::exp(-static_cast<float>(std::fmod(in.music, kTicksPerBeat)) / 30.0f);
    for (u32 k = 0; k < 4; ++k) {
        const Vec2 p0 = corner[k], p1 = corner[(k + 1) % 4];
        // Drawn in pieces, so a wall far off the screen costs nothing.
        const u32 n = 14;
        for (u32 j = 0; j < n; ++j) {
            const Vec2 q0 = lerpv(p0, p1, static_cast<float>(j) / n), q1 = lerpv(p0, p1, static_cast<float>(j + 1) / n);
            if (!visible(q0, 60.0f) && !visible(q1, 60.0f)) continue;
            d.scene.line(toScene(q0), toScene(q1), 1.4f * refPx_, W1 * (1.6f + 0.8f * beat) * level, W1 * (1.6f + 0.8f * beat) * level, 0);
            d.scene.line(toScene(q0), toScene(q1), 4.0f * refPx_, palette_->hue[0] * 0.25f * level, palette_->hue[0] * 0.25f * level, 0);
        }
    }
}

// ---------------------------------------------------------------------------------------------- events

void PlayView::eventFight(const World& w, const Event& e, float t) {
    const Vec2 at{e.x, e.y};
    const BossState& s = w.boss();
    switch (e.type) {
        case EventType::Telegraph:
            if (e.species != kBossSpecies) break;
            // A pack's charge line (from the chevron's tip through the player) or the Hive's spear, until its moment.
            fightLines_[fightLineNext_] = FightLine{at, Vec2{e.x2, e.y2}, e.tick, 1100.0f};
            fightLineNext_ = (fightLineNext_ + 1) % kFightLines;
            break;
        case EventType::BossCue: {
            const BossCue cue = static_cast<BossCue>(e.a);
            if (cue >= BossCue::ElArrive) {  // M5 part 5: the arena's and the elites' moments
                eventArena(w, e, t);
                break;
            }
            // The lattice answers the fights: under each crest on its step, under each Voice as it sings.
            if (cue == BossCue::LwRun && s.kind == BossKind::Longwave) {
                for (u32 k = 0; k < kBossParts; ++k)
                    if ((s.alive & (1u << k)) && w.longwaveCrest(k, e.tick) >= 0) push(kImpBoss, s.body[1 + k], 140.0f, 110.0f, t);
            }
            if (cue == BossCue::ChVoice) push(kImpBoss, at, 120.0f, 110.0f, t);
            if (cue == BossCue::RmWall) {
                addShockRings(at, 1, 260.0f, palette_->hue[2], whiteV(), t, 0.4f);
                shakeAmp_ = std::fmax(shakeAmp_, 4.0f);
            }
            if (cue == BossCue::LwWhip) addShockRings(at, 1, 300.0f, palette_->hue[2], whiteV(), t, 0.4f);
            if (cue == BossCue::HvShape) push(kImpBoss, at, 200.0f, 300.0f, t);
            if (cue == BossCue::HzEat) {
                // Event Horizon eats: the body folds into the horizon (a small white implosion), its cell flashes.
                hzEatAt_ = t;
                SparkReq r{};
                r.at = at;
                r.dir = normalizeOr(s.pos - at, Vec2{1.0f, 0.0f});
                r.count = 10;
                r.kind = 3;
                r.a = palette_->hue[0];
                r.b = whiteV();
                r.t0 = t;
                sparkRequest(r);
            }
            if ((cue == BossCue::PrThree || cue == BossCue::PrFour) && s.kind == BossKind::Polyrhythm)
                push(kImpBoss, at, prose::kPrLatticeImpulse, 120.0f, t);  // the grid beats three against four
            if (cue == BossCue::HzFed) addShockRings(s.pos, 1, w.species().horizon.ringRadius[2] + 120.0f, palette_->hue[1], whiteV(), t, 0.35f);
            if (cue == BossCue::HzStarved)
                addPool(s.pos, w.species().horizon.photonRadius, w.species().horizon.ringRadius[1], palettes_->player[3], reduceFlashing_ ? 0.2f : 0.45f, 0.0f, 0.25f,
                        false, 1, false, false, t);
            break;
        }
        case EventType::BossPair: {
            // A segment shatters along the wave, a Voice's hood splits and falls outward, a Prince breaks: a large burst.
            SparkReq r{};
            r.at = at;
            r.dir = dirv(e.f);
            r.count = 60;
            r.kind = 1;
            r.a = s.kind == BossKind::Choir ? choirHue_[e.a % 8] : palette_->hue[e.a % 3];
            r.b = whiteV();
            r.t0 = t;
            sparkRequest(r);
            addPool(at, 30.0f, 160.0f, whiteV(), reduceFlashing_ ? 0.3f : 0.7f, 0.0f, 0.12f, false, 1, false, false, t);
            addShockRings(at, 1, 220.0f, r.a, whiteV(), t, 0.35f);
            push(kImpBoss, at, 260.0f, 180.0f, t);
            shakeAmp_ = std::fmax(shakeAmp_, 3.0f);
            break;
        }
        case EventType::BossDeath: {
            // The boss break (three rings, 300 sparks, the 30 px distortion ring) for the bosses; a mini-boss dies as a large
            // kill with its own rings. Event Horizon implodes first: its flash comes a bar and a beat later (drawHorizon).
            if (static_cast<BossKind>(e.a) == BossKind::EventHorizon || static_cast<BossKind>(e.a) == BossKind::Polyrhythm ||
                static_cast<BossKind>(e.a) == BossKind::Feedback)
                break;
            if (static_cast<BossKind>(e.a) == BossKind::Reprise) {
                // reprise.md: no burst; the core dims to a point (drawReprise) and one white ripple crosses the whole lattice.
                push(kImpBoss, at, 700.0f, 4200.0f, t);
                addShockRings(at, 1, 4200.0f, whiteV(), whiteV(), t, 1.6f);
                break;
            }
            const bool mini = isMiniBoss(static_cast<BossKind>(e.a));
            const Vec3 c = static_cast<BossKind>(e.a) == BossKind::Choir ? choirHue_[0] : palette_->hue[mini ? 2 : 1];
            addPool(at, 60.0f, mini ? 260.0f : 420.0f, whiteV(), reduceFlashing_ ? 0.45f : 0.9f, 0.0f, 0.12f, false, 1, false, false, t);
            addShockRings(at, mini ? 2 : 3, mini ? 700.0f : 1100.0f, c, whiteV(), t, 0.6f);
            SparkReq r{};
            r.at = at;
            r.count = static_cast<u16>(mini ? 160 : 300);
            r.kind = 1;
            r.a = c;
            r.b = whiteV();
            r.t0 = t;
            sparkRequest(r);
            addDistortion(at, mini ? 600.0f : 900.0f, 0.6f, mini ? 18.0f : 30.0f, 28.0f, t);
            shakeAmp_ = std::fmax(shakeAmp_, mini ? 5.0f : 6.0f);
            push(kImpBoss, at, 600.0f, 500.0f, t);
            break;
        }
        case EventType::BossGone:
            // It warps out: a ring contracting where it was.
            addShockRings(at, 1, 300.0f, palette_->hue[1], palette_->hue[2], t, 0.5f);
            break;
        case EventType::Impact: {
            SparkReq r{};
            r.at = at;
            r.dir = Vec2{-e.x2, -e.y2};
            r.count = static_cast<u16>(e.c == 1 ? 3 : 4);
            r.kind = 3;
            r.a = e.c == 1 ? whiteV() * 0.8f : (s.kind == BossKind::Choir ? choirHue_[e.a % 8] : palette_->hue[e.a % 3]);
            r.b = e.f > 1.5f ? whiteV() : r.a;
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        default: break;
    }
}

}  // namespace rf
