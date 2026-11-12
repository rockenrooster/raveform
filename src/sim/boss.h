#pragma once
// Kaleidos, the boss of Track 1 (design/bosses/kaleidos.md; numbers and shapes from kaleidos.json,
// embedded and parsed here, never retyped). Six arms in three mirrored pairs around a shuttered core.
// Everything it does is a function of song time (the wheel's angle, who rests, every bullet and beam);
// only health is state. The world runs it (sim/world_boss.cpp); the view draws it from the same data.

#include "core/types.h"
#include "sim/sim_types.h"

namespace rf::sim {

struct BossData {
    // Shapes (boss space, world units; arm-local segments [x0, y0, x1, y1, layer], x outward).
    struct Facet {
        float armRadius = 0, length = 0, halfWidth = 0;
        float seg[8][5] = {};
        u32 segs = 0;
    };
    Facet facets[4];
    Facet tip;
    float lensRadius = 66, lensHalfAngleDeg = 26, lensInner = 59, lensWidthPx = 3;
    float coreRingRadius[3] = {}, coreRingPhaseDeg[3] = {};
    u32 coreRingSides[3] = {};
    u32 irisShutters = 6;
    float irisClosed = 4, irisOpen = 13;
    float healthRingRadius = 52;
    float reflectScale[2] = {}, reflectTurnDeg[2] = {}, reflectIntensity[2] = {};
    float accentL = 0.83f, accentHue = 76.0f, accentChroma = 0.96f;
    // Numbers.
    float radius = 270, contactCore = 46;
    float pairHealth = 400, coreHealth = 600;
    float dmgFiring = 1.0f / 3.0f, dmgResting = 3, dmgLens = 0, dmgCoreExposed = 1, dmgCoreDownbeat = 3;
    float driftSpeed = 30, keepNear = 450, keepFar = 650;
    float bulletSpeed = 200, bulletLife = 5;
    struct Phase {
        u32 pairsAlive = 3;
        float turnBars = 8;
        float direction = 1;
        double fireStepTicks = 0;  // 0: the tips do not fire
        u32 fan = 0;
        float spreadDeg = 0;
        u32 coreBurst = 0;         // ring size (Spiral: on beat 1; Naked core: every beat)
        float beamTelegraphBeats = 0, beamFireBeats = 0, beamWidth = 0;
    };
    Phase phases[4];
    float bpm = 120;

    bool load();  // false (logged) when the data the fight needs is missing
};

// The fight's state. Phase = 3 - pairs alive (0 Bloom, 1 Refraction, 2 Spiral, 3 Naked core).
struct BossState {
    bool active = false;       // entered and not dead
    bool dying = false;        // the death plays out (no hits, no fire)
    u8 alive = 0;              // pairs A, B, C (bits 0..2)
    u8 phase = 0;
    double enterTick = 0;      // the Drop's downbeat: the entrance bar begins
    double fightTick = 0;      // a bar later: it fires and can be hurt
    double phaseTick = 0;      // when the current phase's turning began (a pair death stops it a beat)
    float angle0 = 0;          // wheel angle at phaseTick (radians)
    double deathTick = 0;
    float pairHp[3] = {}, coreHp = 0;
    float pairMax = 0, coreMax = 0;
    Vec2 pos, prevPos;
    float angle = 0, prevAngle = 0;
    u32 phaseBar0 = 0;         // fight bar on which the current phase began (Naked core: "from the ninth bar")
    u8 lastBeamBar = 0xFF;
    u32 hits = 0;              // shots that landed (diagnostics)
};

}  // namespace rf::sim
