#pragma once
// Command line. On Android the string intent extra "args" is split on whitespace into argv by
// RaveformActivity.getArguments(), so both platforms share this parser.

#include "core/types.h"
// M2 audio begin
#include "audio/audio_args.h"
// M2 audio end

namespace rf {

// --mute <name,name,...>: analysis pictures without one kind of light. Muting only skips drawing (or
// turns a composite effect off); the simulation, the random sequence and the light budget are unchanged.
enum MuteBits : u32 {
    kMuteBackground = 1u << 0,  // gradient, nebula, star dust
    kMuteGrid = 1u << 1,        // the whole lattice
    kMuteHeat = 1u << 2,        // lattice heat above the rest level
    kMuteEnemy = 1u << 3,       // the horde: hulls, ornaments, marks, flares, streaks (not elites, not the boss)
    kMuteElite = 1u << 4,       // elites: hull, white copy, glass, flares (not their auras)
    kMuteBoss = 1u << 5,        // the boss stand-in's glyphs (not their auras)
    kMuteAura = 1u << 6,        // elite and boss auras
    kMuteBullet = 1u << 7,      // the scene light of enemy bullets, beams and trails
    kMutePriority = 1u << 8,    // the priority layer: threat and ship rims and cores
    kMuteSparks = 1u << 9,      // sparks
    kMuteFlash = 1u << 10,      // kill flashes and the boss flash
    kMutePool = 1u << 11,       // kill pools
    kMuteFx = 1u << 12,         // shock rings, embers, shatter, gravity wells, the Drop front
    kMuteWarp = 1u << 13,       // warp-in rings
    kMuteShard = 1u << 14,      // Shards
    kMutePlayer = 1u << 15,     // the player's scene light: ships, halo, trail, pool, shots, Riffs
    kMuteAfterglow = 1u << 16,  // composite effects:
    kMuteDistortion = 1u << 17,
    kMuteAberration = 1u << 18,
    kMuteVignette = 1u << 19,
    kMutePump = 1u << 20,       // the kick's exposure, bloom and enemy swell pumps
};

struct AppArgs {
    char scenario[32] = {};    // "" = interactive; bench_idle; lifecycle_test
    float seconds = 60.0f;     // measured duration for bench scenarios
    float warmupSeconds = 2.0f;
    u32 frames = 0;            // quit after N presented frames (0 = run until closed)
    char screenshot[260] = {}; // write an upright PNG of the last frame, then quit
    int width = 1600;          // desktop window size in pixels (--res WxH)
    int height = 720;
    int validation = -1;       // -1 = build default (on in Debug desktop builds)
    bool tone = false;         // hold the test tone on (audio soak)
    bool swappy = true;        // Android: present through Swappy (off = plain FIFO)
    float fps = 0.0f;          // Android: requested refresh rate (0 = panel maximum)
    bool fastCores = true;     // --affinity big|all: pin the main thread to the non-little cores (Android)
    bool overlay = true;
    char sceneFile[260] = {};  // --scene <file.rfscene>: replay a scene file (design/look.md §9)
    bool still = false;        // --still: every time-driven effect off (acceptance pictures)
    bool bloom = true;         // --bloom on|off
    char look[16] = "signal";  // --look signal|laser|haze (design/look_variants.json)
    char tier[12] = {};        // --tier low|medium|high|ultra ("" = the device default)
    char palette[16] = "reef"; // --palette <id> (design/palettes.json)
    u32 strip = 0;             // --strip N: also capture the N-1 frames before the --screenshot frame
    u32 page = 1;              // --page N: glyph_sheet page (1-3)
    int governor = -1;         // --governor on|off (-1: on, except in benchmarks)
    bool fixedStep = false;    // --fixed-step: advance scenes by 1/60 s per frame (implied by screenshots)
    u32 gpuBurn = 0;           // --gpu-burn N: profiling load that keeps the GPU at its highest clock
    u32 mute = 0;              // --mute <names>: MuteBits (analysis pictures)
    u32 cycles = 20;           // lifecycle_test cycles
    // M2 audio begin
    AudioArgs audio;           // --band, --track, --sound, ... (audio/audio_args.h)
    // M2 audio end
};

// Returns false on a malformed or unknown argument (after logging it).
bool parseArgs(int argc, char** argv, AppArgs& out);

void logArgs(const AppArgs& a);

}  // namespace rf
