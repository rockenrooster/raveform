// Desktop audio output through an SDL3 audio stream (WASAPI on Windows).
#include <SDL3/SDL_audio.h>

#include <chrono>

#include "audio/audio_device.h"
#include "core/log.h"

namespace rf {

i64 audioNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct AudioDevice::Impl {
    static constexpr u32 kChunkFrames = 1024;
    SDL_AudioStream* stream = nullptr;
    AudioRenderFn fn = nullptr;
    void* user = nullptr;
    CallbackMeter meter;
    SDL_AudioSpec spec{};
    int deviceFrames = 0;
    std::atomic<u32> generation{0};
    std::atomic<u32> queuedFrames{0};  // what the stream still held when the device asked for more
    float scratch[kChunkFrames * 2] = {};

    static void SDLCALL callback(void* userdata, SDL_AudioStream* s, int additional, int) {
        auto* self = static_cast<Impl*>(userdata);
        flushDenormals();
        const auto t0 = std::chrono::steady_clock::now();
        const u32 channels = static_cast<u32>(self->spec.channels);
        const int queued = SDL_GetAudioStreamQueued(s);
        self->queuedFrames.store(queued > 0 ? static_cast<u32>(queued) / (sizeof(float) * channels) : 0u,
                                 std::memory_order_relaxed);
        u32 framesLeft = static_cast<u32>(additional) / (sizeof(float) * channels);
        const u32 total = framesLeft;
        while (framesLeft > 0) {
            const u32 n = framesLeft < kChunkFrames ? framesLeft : kChunkFrames;
            self->fn(self->user, self->scratch, n, channels, static_cast<u32>(self->spec.freq));
            SDL_PutAudioStreamData(s, self->scratch, static_cast<int>(n * channels * sizeof(float)));
            framesLeft -= n;
        }
        const auto t1 = std::chrono::steady_clock::now();
        self->meter.record(static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()),
                           total);
    }
};

bool AudioDevice::open(AudioRenderFn fn, void* user) {
    if (!impl_) impl_ = new Impl();
    if (impl_->stream) return true;
    impl_->fn = fn;
    impl_->user = user;
    impl_->spec.format = SDL_AUDIO_F32;
    impl_->spec.channels = 2;
    impl_->spec.freq = 48000;
    impl_->generation.fetch_add(1, std::memory_order_release);
    impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &impl_->spec, Impl::callback, impl_);
    if (!impl_->stream) {
        RF_ERR("audio: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec deviceSpec{};
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(impl_->stream), &deviceSpec, &impl_->deviceFrames);
    SDL_ResumeAudioStreamDevice(impl_->stream);
    RF_LOG("audio: SDL %s, stream %d Hz %d ch, device %d Hz %d ch, device buffer %d frames", SDL_GetCurrentAudioDriver(),
           impl_->spec.freq, impl_->spec.channels, deviceSpec.freq, deviceSpec.channels, impl_->deviceFrames);
    return true;
}

void AudioDevice::close() {
    if (impl_ && impl_->stream) {
        SDL_DestroyAudioStream(impl_->stream);
        impl_->stream = nullptr;
    }
}

bool AudioDevice::isOpen() const { return impl_ && impl_->stream; }

void AudioDevice::update() {}

AudioStats AudioDevice::stats() const {
    AudioStats s;
    if (!impl_) return s;
    s.callbacks = impl_->meter.callbacks.load(std::memory_order_relaxed);
    s.framesRendered = impl_->meter.frames.load(std::memory_order_relaxed);
    s.worstCallbackMs = static_cast<float>(impl_->meter.worstNs.load(std::memory_order_relaxed) * 1e-6);
    if (!impl_->stream) return s;
    s.open = true;
    s.api = "sdl";
    s.sampleRate = static_cast<u32>(impl_->spec.freq);
    s.channels = static_cast<u32>(impl_->spec.channels);
    s.framesPerBurst = static_cast<u32>(impl_->deviceFrames);
    s.bufferFrames = static_cast<u32>(impl_->deviceFrames);
    s.underruns = -1;  // SDL does not expose device underruns
    return s;
}

void AudioDevice::resetPeaks() {
    if (impl_) impl_->meter.resetRequests.fetch_add(1, std::memory_order_relaxed);
}

bool AudioDevice::timestamp(AudioTimestamp&) const { return false; }  // SDL has no presentation timestamps

u32 AudioDevice::generation() const { return impl_ ? impl_->generation.load(std::memory_order_acquire) : 0; }

u32 AudioDevice::bufferedFrames() const {
    if (!impl_ || !impl_->stream) return 0;
    // The stream's backlog plus the device's own buffer (WASAPI shared mode holds about two periods).
    return impl_->queuedFrames.load(std::memory_order_relaxed) + 2u * static_cast<u32>(impl_->deviceFrames);
}

}  // namespace rf
