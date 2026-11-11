// Android audio output through Oboe (AAudio on the test devices).
#include <oboe/Oboe.h>

#include <chrono>
#include <memory>

#include "audio/audio_device.h"
#include "core/log.h"
#include "core/time.h"

namespace rf {

namespace {

class OboeCallbacks : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    AudioRenderFn fn = nullptr;
    void* user = nullptr;
    CallbackMeter meter;
    std::atomic<bool> disconnected{false};

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

    // Oboe has already stopped and closed the stream; the main thread reopens it in update().
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
        if (error == oboe::Result::ErrorDisconnected) disconnected.store(true);
    }
};

}  // namespace

struct AudioDevice::Impl {
    std::shared_ptr<OboeCallbacks> callbacks = std::make_shared<OboeCallbacks>();
    std::shared_ptr<oboe::AudioStream> stream;
    i32 underrunsClosed = 0;  // from streams already closed
    i32 lastUnderruns = 0;
    u32 reopens = 0;
    u64 lastTuneNs = 0;

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
        r = stream->requestStart();
        if (r != oboe::Result::OK) {
            RF_ERR("audio: requestStart failed: %s", oboe::convertToText(r));
            stream->close();
            stream.reset();
            return false;
        }
        RF_LOG("audio: %s %s %s%s, %d Hz, %d ch, burst %d, buffer %d of %d", oboe::convertToText(stream->getAudioApi()),
               oboe::convertToText(stream->getSharingMode()), oboe::convertToText(stream->getPerformanceMode()),
               oboe::OboeExtensions::isMMapUsed(stream.get()) ? " mmap" : "", stream->getSampleRate(),
               stream->getChannelCount(), burst, stream->getBufferSizeInFrames(), stream->getBufferCapacityInFrames());
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
    if (impl_->stream) return true;
    impl_->callbacks->fn = fn;
    impl_->callbacks->user = user;
    impl_->callbacks->disconnected.store(false);
    return impl_->openStream();
}

void AudioDevice::close() {
    if (impl_) impl_->closeStream();
}

bool AudioDevice::isOpen() const { return impl_ && impl_->stream; }

void AudioDevice::update() {
    if (!impl_) return;
    if (impl_->callbacks->disconnected.exchange(false)) {
        RF_WARN("audio: stream disconnected (route change), reopening");
        if (impl_->stream) {
            auto xr = impl_->stream->getXRunCount();  // the stream is already closed by Oboe
            if (xr) impl_->underrunsClosed += xr.value();
            impl_->stream.reset();
        }
        if (impl_->openStream()) ++impl_->reopens;
        return;
    }
    // Twice a second: one more burst of buffering after each new underrun.
    const u64 now = nowNs();
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
    return s;
}

void AudioDevice::resetPeaks() {
    if (impl_) impl_->callbacks->meter.resetRequests.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace rf
