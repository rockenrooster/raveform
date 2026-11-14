#pragma once
// M5: where the game's sound commands go. The band in the game (Band, which posts them to its engine on the audio thread), an
// engine driven offline (raveform_headless --render fights, the listening sheet of the fights' sounds). game/pending_sounds.cpp
// speaks to this, so the sheet plays exactly what the game would.

#include "audio/engine.h"
#include "audio/music_data.h"

namespace rf::audio {

class CommandSink {
public:
    virtual void send(const Command& c) = 0;
    virtual const MusicData& music() const = 0;
    virtual u32 track() const = 0;  // the Track the band plays now (0-based)

protected:
    ~CommandSink() = default;
};

}  // namespace rf::audio
