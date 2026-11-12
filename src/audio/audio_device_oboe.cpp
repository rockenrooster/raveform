// Android audio output through Oboe (AAudio on the test devices).
#include <oboe/Oboe.h>

#include <chrono>
#include <ctime>
#include <memory>

#include "audio/audio_device.h"
#include "core/log.h"
#include "core/time.h"

namespace rf {

i64 audioNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

namespace {

class OboeCallbacks : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    AudioRenderFn fn = nullptr;
    void* user = nullptr;
    CallbackMeter meter;
    std::atomic<bool> disconnected{false};
    std::atomic<bool> failed{false};

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* data, int32_t frames) override {
        flushDenormals();
        const auto t0 = std::chrono::steady_clock::now();
        fn(user, static_cast<float*>(data), static_cast<u32>(frames), static_cast<u32>(stream->getChannelCount()),
           static_cast<u32>(stream->getSampleRate()));
        const auto t1 = std::chrono::steady_clock::now();
        meter.record(static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()),
                     static_cast<u32>(frames));
        return oboe::DataCallbackResult::Continue;
    }

    // Oboe has already stopped and closed the stream; the main thread reopens it in update(). A route
    // change (headphones, Bluetooth) arrives as ErrorDisconnected; any other error is reopened too.
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
        if (error == oboe::Result::ErrorDisconnected) {
            disconnected.store(true);
        } else {
            failed.store(true);
        }
    }
};

}  // namespace

struct AudioDevice::Impl {
    std::shared_ptr<OboeCallbacks> callbacks = std::make_shared<OboeCallbacks>();
    std::shared_ptr<oboe::AudioStream> stream;
    i32 underrunsClosed = 0;  // from streams already closed
    i32 lastUnderruns = 0;
    u32 reopens = 0;
    std::atomic<u32> generation{0};  // read by the audio thread (Band) at each callback
    u64 lastTuneNs = 0;
    u64 retryAtNs = 0;   // a failed reopen is retried after a pause
    bool wantOpen = false;
    i32 deviceId = 0;    // read once per stream: Oboe 1.11's getDeviceId() can allocate (the overlay's stats())

    bool openStream() {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(oboe::ChannelCount::Stereo)
            ->setUsage(oboe::Usage::Game)
            ->setDataCallback(callbacks)
            ->setErrorCallback(callbacks);
        oboe::Result r = builder.openStream(stream);
        if (r != oboe::Result::OK) {
            RF_ERR("audio: openStream failed: %s", oboe::convertToText(r));
            stream.reset();
            return false;
        }
        const int32_t burst = stream->getFramesPerBurst();
        stream->setBufferSizeInFrames(burst * 2);  // start at two bursts, grow on underruns
        lastUnderruns = 0;
        generation.fetch_add(1, std::memory_order_release);  // before the first callback of this stream
        r = stream->requestStart();
        if (r != oboe::Result::OK) {
            RF_ERR("audio: requestStart failed: %s", oboe::convertToText(r));
            stream->close();
            stream.reset();
            return false;
        }
        deviceId = stream->getDeviceId();
        RF_LOG("audio: %s %s %s%s, %d Hz, %d ch, burst %d, buffer %d of %d, device %d", oboe::convertToText(stream->getAudioApi()),
               oboe::convertToText(stream->getSharingMode()), oboe::convertToText(stream->getPerformanceMode()),
               oboe::OboeExtensions::isMMapUsed(stream.get()) ? " mmap" : "", stream->getSampleRate(),
               stream->getChannelCount(), burst, stream->getBufferSizeInFrames(), stream->getBufferCapacityInFrames(),
               deviceId);
        return true;
    }

    void closeStream() {
        if (!stream) return;
        auto xr = stream->getXRunCount();
        if (xr) underrunsClosed += xr.value();
        stream->stop();
        stream->close();
        stream.reset();
    }
};

bool AudioDevice::open(AudioRenderFn fn, void* user) {
    if (!impl_) impl_ = new Impl();
    impl_->wantOpen = true;
    if (impl_->stream) return true;
    impl_->callbacks->fn = fn;
    impl_->callbacks->user = user;
    impl_->callbacks->disconnected.store(false);
    impl_->callbacks->failed.store(false);
    return impl_->openStream();
}

void AudioDevice::close() {
    if (!impl_) return;
    impl_->wantOpen = false;
    impl_->closeStream();
}

bool AudioDevice::isOpen() const { return impl_ && impl_->stream; }

void AudioDevice::update() {
    if (!impl_) return;
    const u64 now = nowNs();
    const bool disconnected = impl_->callbacks->disconnected.exchange(false);
    const bool failed = impl_->callbacks->failed.exchange(false);
    if (disconnected || failed) {
        RF_WARN("audio: stream %s, reopening", disconnected ? "disconnected (route change)" : "failed");
        if (impl_->stream) {
            auto xr = impl_->stream->getXRunCount();  // the stream is already closed by Oboe
            if (xr) impl_->underrunsClosed += xr.value();
            impl_->stream.reset();
        }
        impl_->retryAtNs = now;
    }
    if (!impl_->stream && impl_->wantOpen && impl_->retryAtNs && now >= impl_->retryAtNs) {
        if (impl_->openStream()) {
            ++impl_->reopens;
            impl_->retryAtNs = 0;
        } else {
            impl_->retryAtNs = now + 500000000ull;  // try again in half a second
        }
        return;
    }
    // Twice a second: one more burst of buffering after each new underrun.
    if (!impl_->stream || now - impl_->lastTuneNs < 500000000ull) return;
    impl_->lastTuneNs = now;
    auto xr = impl_->stream->getXRunCount();
    if (!xr) return;
    if (xr.value() > impl_->lastUnderruns) {
        impl_->lastUnderruns = xr.value();
        const int32_t size = impl_->stream->getBufferSizeInFrames() + impl_->stream->getFramesPerBurst();
        if (size <= impl_->stream->getBufferCapacityInFrames()) {
            impl_->stream->setBufferSizeInFrames(size);
            RF_WARN("audio: underrun, buffer now %d frames", impl_->stream->getBufferSizeInFrames());
        }
    }
}

AudioStats AudioDevice::stats() const {
    AudioStats s;
    if (!impl_) return s;
    const CallbackMeter& m = impl_->callbacks->meter;
    s.callbacks = m.callbacks.load(std::memory_order_relaxed);
    s.framesRendered = m.frames.load(std::memory_order_relaxed);
    s.worstCallbackMs = static_cast<float>(m.worstNs.load(std::memory_order_relaxed) * 1e-6);
    s.reopens = impl_->reopens;
    s.underruns = impl_->underrunsClosed;
    if (!impl_->stream) return s;
    oboe::AudioStream& st = *impl_->stream;
    s.open = true;
    s.api = st.getAudioApi() == oboe::AudioApi::AAudio ? "aaudio" : "opensles";
    s.sampleRate = static_cast<u32>(st.getSampleRate());
    s.channels = static_cast<u32>(st.getChannelCount());
    s.framesPerBurst = static_cast<u32>(st.getFramesPerBurst());
    s.bufferFrames = static_cast<u32>(st.getBufferSizeInFrames());
    s.bufferCapacity = static_cast<u32>(st.getBufferCapacityInFrames());
    auto xr = st.getXRunCount();
    s.underruns = xr ? impl_->underrunsClosed + xr.value() : -1;
    s.exclusive = st.getSharingMode() == oboe::SharingMode::Exclusive;
    s.lowLatency = st.getPerformanceMode() == oboe::PerformanceMode::LowLatency;
    s.mmap = oboe::OboeExtensions::isMMapUsed(&st);
    s.deviceId = impl_->deviceId;
    return s;
}

void AudioDevice::resetPeaks() {
    if (impl_) impl_->callbacks->meter.resetRequests.fetch_add(1, std::memory_order_relaxed);
}

bool AudioDevice::timestamp(AudioTimestamp& out) const {
    if (!impl_ || !impl_->stream) return false;
    auto ts = impl_->stream->getTimestamp(CLOCK_MONOTONIC);
    if (!ts) return false;
    out.position = ts.value().position;
    out.timeNs = ts.value().timestamp;
    return out.timeNs > 0;
}

u32 AudioDevice::generation() const { return impl_ ? impl_->generation.load(std::memory_order_acquire) : 0; }

u32 AudioDevice::bufferedFrames() const {
    if (!impl_ || !impl_->stream) return 0;
    return static_cast<u32>(impl_->stream->getBufferSizeInFrames());
}

}  // namespace rf
