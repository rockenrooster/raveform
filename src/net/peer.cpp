#include "net/peer.h"

#include <cmath>
#include <new>

#include "core/alloc_counter.h"

namespace rf::net {

using namespace rf::sim;

namespace {
constexpr i64 kTickUs = 16667;  // 1/60 s
}

bool NetPeer::start(const PeerConfig& cfg, const SpeciesTable* species) {
    stop();
    cfg_ = cfg;
    world_ = new (std::nothrow) World;
    if (!world_ || !species) return false;
    world_->init(species);
    if (!session_.start(cfg.session, world_, contentHash() ^ cfg.contentSalt)) return false;
    session_.setClock([](void* self) { const NetPeer& p = *static_cast<const NetPeer*>(self); return p.localUs(wireNowUs()); }, this);
    bot_.reset(cfg.botSeed, cfg.policy);
    controller_.reset(world_->player(0).pos);
    songStarted_ = runStarted_ = false;
    trueStart_ = 0;
    simHostUs_ = 0;
    music_ = 0.0;
    stats_ = Stats{};
    return true;
}

void NetPeer::stop() {
    session_.stop();
    delete world_;
    world_ = nullptr;
}

i64 NetPeer::localUs(i64 trueUs) const {
    const double since = static_cast<double>(trueUs - trueStart_);
    return trueUs + static_cast<i64>(since * cfg_.clockPpm * 1e-6) + cfg_.clockOffsetUs;
}

double NetPeer::songNow(i64 trueUs) const {
    // The host's song from its own anchor (its SongClock as of its last frame, carried to this moment).
    if (session_.host()) return session_.songAt(localUs(trueUs));
    return session_.songAt(session_.clock().hostAt(localUs(trueUs)));
}

float NetPeer::tempoOf(void* self, double barTick) {
    // The host's song: the bar's tempo by the director (the Build's last bar, the Track change), Track 1's before.
    const NetPeer& p = *static_cast<const NetPeer*>(self);
    if (!p.runStarted_) return 120.0f;
    return p.world_->tempoAt(barTick - p.session_.runOffset() + 1.0);
}

void NetPeer::frame(i64 trueUs) {
    if (!world_) return;
    const u64 a0 = measureAllocs_ ? heapAllocCountThisThread() : 0;
    if (!trueStart_) trueStart_ = trueUs;
    ++stats_.frames;
    const i64 local = localUs(trueUs);
    session_.receive(local);
    const i64 hostNow = session_.hostNow();
    World& w = *world_;
    if (session_.host()) {
        if (!songStarted_) {
            songClock_.reset(hostNow, 0.0, 120.0f);
            songStarted_ = true;
        }
        songClock_.advanceTo(hostNow, &tempoOf, this);
        session_.setHostSong(songClock_.anchor(&tempoOf, this));
    }
    if (session_.running() && session_.songValid()) {
        if (!runStarted_) {
            // The run begins on its bar line: the first tick crosses it.
            const double run = session_.songAt(hostNow) - session_.runOffset();
            if (w.restartPending() && run >= w.restartTick()) {
                runStarted_ = true;
                music_ = w.restartTick();
                const double tps = session_.tpsAt(hostNow);
                simHostUs_ = hostNow - static_cast<i64>((run - w.restartTick()) / tps * 1e6);
                controller_.reset(w.player(session_.seat()).pos);
            }
        }
        if (runStarted_) {
            u32 n = 0;
            while (simHostUs_ + kTickUs <= hostNow && n < 3) {
                const i64 tEnd = simHostUs_ + kTickUs;
                simHostUs_ = tEnd;
                double song = session_.songAt(tEnd);
                if (session_.resuming() && song >= session_.resumeSong()) session_.resumed();
                double m1 = song - session_.runOffset();
                if (session_.paused() && m1 > session_.pauseTick()) m1 = session_.pauseTick();
                if (m1 <= music_) {  // paused (the run holds its place; the song goes on)
                    if (!session_.paused()) session_.noteTickSkipped();
                    continue;
                }
                const float bpm = static_cast<float>(60.0 / 96.0 * (m1 - music_) / (static_cast<double>(kTickUs) * 1e-6));
                tick(m1, bpm > 1.0f ? bpm : 120.0f);
                ++n;
            }
            if (n > stats_.maxTicksInFrame) stats_.maxTicksInFrame = n;
            if (hostNow - simHostUs_ > 3 * kTickUs) {
                simHostUs_ = hostNow - kTickUs / 2;  // TECH_SPEC 4: beyond three ticks a frame, drop time
                ++stats_.dropped;
                session_.noteTimeDropped();
            }
            session_.noteFrame(local, n);
        }
    }
    session_.send(local, controller_.pose());
    if (measureAllocs_) stats_.heapAllocs += heapAllocCountThisThread() - a0;
}

void NetPeer::tick(double m1, float bpm) {
    World& w = *world_;
    const u32 seat = session_.seat();
    TickInput in;
    in.music0 = music_;
    in.music1 = m1;
    in.bpm = bpm;
    const StickInput stick = bot_.think(w, seat, controller_, music_);
    controller_.update(stick, kDt, m1, bpm, w, seat);
    in.players[seat] = controller_.pose();
    session_.fillRemote(in, simHostUs_);
    w.tick(in);
    controller_.clearEdges();
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if (e.type == EventType::Restart) {
            controller_.reset(w.player(seat).pos);
            ++stats_.restarts;
        } else if (e.type == EventType::Revive && e.a == seat) {
            controller_.reset(w.player(seat).pos);
        }
    }
    session_.afterTick();
    music_ = m1;
    ++stats_.ticks;
}

}  // namespace rf::net
