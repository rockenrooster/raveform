#pragma once
// Command-line switches of the band (M2), parsed by core/args.cpp through parseAudioArg().
//   --band off|play|staged|full|stress|metronome  what the band plays (default: off, except in the audio
//                                           scenes); full / stress: the load tests (src/audio/band.cpp)
//   --track 1|2|track01|track02             the Track (default 1)
//   --sound club|arcade|velvet              the sound variant (default club, variants.json "reference")
//   --intensity X                           director intensity 0..1 (play)
//   --section groove|build|drop|break       start in this section (play); --hold keeps it
//   --fire                                  the local player holds fire from the start
//   --haptics on|off                        beat haptics (default on in the audio scenes, off elsewhere)
//   --sync-offset MS                        sync calibration override for this run (not saved)

#include "core/types.h"

namespace rf {

struct AudioArgs {
    char band[12] = {};
    char track[12] = {};
    char sound[12] = {};
    float intensity = -1.0f;
    char section[8] = {};
    bool hold = false;
    bool fire = false;
    int haptics = -1;
    bool syncOffsetSet = false;
    float syncOffsetMs = 0.0f;
};

// True when `arg` is one of the switches above.
bool isAudioArg(const char* arg);
// Parses one switch (and its value in `next`); false on a bad value.
bool parseAudioArg(const char* arg, const char* next, AudioArgs& out, bool& usedNext);
void logAudioArgs(const AudioArgs& a);

}  // namespace rf
