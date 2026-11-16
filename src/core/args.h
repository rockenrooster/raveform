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
    bool offscreen = false;    // --offscreen (desktop; the user's rule: PC test runs stay off screen): the window is placed
                               // off the visible desktop, never takes focus and has no taskbar button; it renders and
                               // captures as usual
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
    i32 uiScale = -1;          // --ui-scale 0-3 (M8 part 7): the HUD's size for this run (pictures: 90, 100, 115, 130%)
    float recklessAfter = 0;   // --reckless-after S: with --autoplay, the bot turns careless after S s of a run (tests the loop)
    u64 seed = 1;              // --seed N: the run's random seed
    float fastForward = 0.0f;  // --fast-forward S: S seconds of the game before the first frame (pictures)
    bool noHits = false;       // --nohits: hits cost nothing (a measured run deep in the game, e.g. Track 3; C2)
    bool loadoutMax = false;   // M5 --loadout max: every Mod at its cap, the heaviest Riffs at 5, Chorus and Fractal held (benchmarks)
    bool vivid = false;        // M6 --vivid: COLOUR VIVID (Display-P3 where offered) for this run, not saved (device checks)
    char picture[32] = {};     // --picture <name>: a scripted review scene (fire, kill_small, ..., sentry)
    char fight[24] = {};       // --fight <id>[:<phase>] (M5): the run starts in the fight's home Track and plays on to it
    u8 fightPhase = 0;         // ... with its parts taken down to that phase (0 its first; a mini-boss: 1 half health)
    bool fightHold = false;    // --fight <id>:<phase>:hold (benchmarks): its health comes back, so it stays in that phase
    u8 jumpTrack = 0, jumpCycle = 0;  // --drop <track>.<cycle> (M5): the run starts in that Track, fast-forwarded to that cycle's Drop
    bool holdDrop = false;             // --drop-hold (M7, the thermal check): that Drop is held for the whole run
    bool paletteSet = false;   // --palette given (else the game uses its Track's palette)
    // M8 (docs/MODES_PLAN.md 2.10): the run's mode; parsed into sim::RunSetup by the game (sim::parseModeArg).
    char mode[40] = {};        // --mode <id>[:<track>[:<moment>[:hold]]] (set, one, headliners, blast, tracers, songline, jam)
    bool versus = false;       // --versus: a versus run (with --host, --coop host or the headless tools)
    bool noHelp = false;       // --no-help: the underdog's help off
    char coopMode[40] = {};    // --coop-mode <id>[:<track>][:vs]: the room's mode, as the host's MODE pill would set it
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
    // M4 stage B: the co-op screens driven from the command line (tests, pictures, device runs).
    u8 coop = 0;               // --coop host|join[:<code>|:<ip>|:list]: 1 host a room at once, 2 open the join screen
    char coopTarget[48] = {};  //   the room code, the address, or "list" (the first room heard) to join at once
    u32 coopStart = 0;         // --coop-start [N]: the host starts once N players (default 2) are seated and ready, and
                               //   goes again 3 s after all are down
    char coopName[16] = {};    // --coop-name NAME: the name on the seat
    u8 coopPermission = 0;     // --coop-permission-screen [refused]: show the local network's screen (pictures); 2: as refused
    bool coopDemoRooms = false;  // --coop-demo-rooms: the join list also shows three made-up rooms (the layout's picture)
    u32 contentSalt = 0;       // --net-content-salt N: another content hash (the mismatch message's test)
    char coopShot[16] = {};    // --coop-shot <moment>: with --screenshot, the picture at that co-op moment, then quit
    float coopDownAt = -1.0f;  // --coop-down-at S: this device's ship goes down S s into the run (tests, pictures)
    float coopPauseAt = -1.0f; // --coop-pause-at S: this device pauses for everyone S s into the run, resumes 3 s later
    float coopQuitAt = -1.0f;  // --coop-quit-at S: this device leaves the room S s into the run (the host: the run ends for all)
    bool coopBackLobby = false;  // --coop-back-lobby (M8): the host taps the podium's LOBBY (tests, pictures)
    // M4 end
    // M2 audio begin
    AudioArgs audio;           // --band, --track, --sound, ... (audio/audio_args.h)
    // M2 audio end
};

// Returns false on a malformed or unknown argument (after logging it).
bool parseArgs(int argc, char** argv, AppArgs& out);

void logArgs(const AppArgs& a);

}  // namespace rf
