// M5: the sound of the content (design/music/sound_m5.json, audio/cues.h): the game's cues, the fights' recipes and
// held voices. Audio thread, like the rest of the engine: no allocation, lock, log or file access.

#include "audio/engine.h"

namespace rf::audio {

void Engine::applyContentCommand(const Command& c) {
    (void)c;  // built behind the published ids (cues.h) in the following commits
}

}  // namespace rf::audio
