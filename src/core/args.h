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
    kMutePool = 1u << 11,       // kill pools ("killpool")
    kMuteFx = 1u << 12,         // shock rings, embers, shatter, gravity wells, the Drop front
    kMuteWarp = 1u << 13,       // warp-in rings
    kMuteShard = 1u << 14,      // Shards
    kMutePlayer = 1u << 15,     // the player's scene light: ships, halo, trail, pool, shots, Riffs
    kMuteAfterglow = 1u << 16,  // composite effects:
    kMuteDistortion = 1u << 17,
    kMuteAberration = 1u << 18,
    kMuteVignette = 1u << 19,
    kMutePump = 1u << 20,       // the kick's exposure, bloom and enemy swell pumps
    kMuteSparkles = 1u << 21,   // every soft light under 16 ref px (the full-resolution sparkle batch)
    kMutePools = 1u << 22,      // every soft light of 16 ref px and up (the quarter-resolution pool batch)
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
    bool reduceFlashing = false;  // --reduce-flashing: beat amounts halved, no Drop burst (look.md §7)
    char tier[12] = {};        // --tier low|medium|high|ultra ("" = the device default)
    u32 cuts = 0;              // --cuts N: the first N cuts of design/look.md §10 on top of the tier
    char palette[16] = "reef"; // --palette <id> (design/palettes.json)
    u32 strip = 0;             // --strip N: also capture the N-1 frames before the --screenshot frame
    u32 page = 1;              // --page N: glyph_sheet page (1-3)
    int governor = -1;         // --governor on|off (-1: on, except in benchmarks)
    bool fixedStep = false;    // --fixed-step: advance scenes by 1/60 s per frame (implied by screenshots)
    u32 gpuBurn = 0;           // --gpu-burn N: profiling load that keeps the GPU at its highest clock
    u32 mute = 0;              // --mute <names>: MuteBits (analysis pictures)
    u32 cycles = 20;           // lifecycle_test cycles
    // M3 begin: the game (game/play.h): --scenario play (the default), bench_play, bench_stress
    bool bot = false;          // --bot: the bot plays (on screen; bench_play and bench_stress always)
    bool autoplay = false;     // --autoplay (M3 stage B): the game with its screens, the bot playing each run
    float recklessAfter = 0;   // --reckless-after S: with --autoplay, the bot turns careless after S s of a run (tests the loop)
    u64 seed = 1;              // --seed N: the run's random seed
    float fastForward = 0.0f;  // --fast-forward S: S seconds of the game before the first frame (pictures)
    bool noHits = false;       // --nohits: hits cost nothing (a measured run deep in the game, e.g. Track 3; C2)
    char picture[32] = {};     // --picture <name>: a scripted review scene (fire, kill_small, ..., sentry)
    bool paletteSet = false;   // --palette given (else the game uses its Track's palette)
    // M3 end
    // M4 begin: co-op from the command line (stage A: no lobby screens)
    u8 net = 0;                // 0 solo, 1 --host, 2 --join <ip>
    char joinIp[48] = {};      // --join <ip>: the host's address
    char bindIp[48] = {};      // --bind <ip>: the local address (host: listen on it; "" every address)
    u32 port = 47778;          // --port N (TECH_SPEC 8.3: 47778)
    u32 players = 2;           // --players N: the host's seats (itself included), 2-4
    char netsim[96] = {};      // --netsim loss=..,jitter=..,delay=..,dup=..,reorder=.. (testing)
    u32 loopbackBots = 0;      // --loopback-bots N: N bot clients in this process on the loopback network (host)
    bool netOverlay = false;   // --net-overlay: open on the overlay's network page
    u32 netCaptureBar = 0;     // --net-capture-bar B: with --screenshot, the picture as the run reaches bar B, then quit
                               // (every peer at the same musical moment)
    i32 netViewSeat = -1;      // --net-view-seat N: the camera on seat N's ship (pictures of one place on every peer)
    // M4 end
    // M2 audio begin
    AudioArgs audio;           // --band, --track, --sound, ... (audio/audio_args.h)
    // M2 audio end
};

// Returns false on a malformed or unknown argument (after logging it).
bool parseArgs(int argc, char** argv, AppArgs& out);

void logArgs(const AppArgs& a);

}  // namespace rf
