#pragma once
// Command line. On Android the string intent extra "args" is split on whitespace into argv by
// RaveformActivity.getArguments(), so both platforms share this parser.

#include "core/types.h"

namespace rf {

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
    bool overlay = true;
    u32 cycles = 20;           // lifecycle_test cycles
};

// Returns false on a malformed or unknown argument (after logging it).
bool parseArgs(int argc, char** argv, AppArgs& out);

void logArgs(const AppArgs& a);

}  // namespace rf
