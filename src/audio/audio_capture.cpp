#include "audio_capture.hpp"
#include "log.hpp"

namespace audio {

AudioCapture::AudioCapture(uint32_t sample_rate) : sample_rate_(sample_rate) {}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start() {
    if (running_)
        return true;

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 1;
    cfg.sampleRate = sample_rate_;
    cfg.dataCallback = AudioCapture::data_callback;
    cfg.pUserData = this;

    if (ma_device_init(nullptr, &cfg, &device_) != MA_SUCCESS) {
        LOG_ERROR("AudioCapture: failed to init device");
        return false;
    }

    if (ma_device_start(&device_) != MA_SUCCESS) {
        LOG_ERROR("AudioCapture: failed to start device");
        ma_device_uninit(&device_);
        return false;
    }

    running_ = true;
    return true;
}

void AudioCapture::stop() {
    if (!running_)
        return;
    ma_device_stop(&device_);
    ma_device_uninit(&device_);
    running_ = false;
}

void AudioCapture::data_callback(ma_device* device, void* /*output*/, const void* input,
                                 uint32_t frame_count) {
    auto* const self = static_cast<AudioCapture*>(device->pUserData);
    const auto* const samples = static_cast<const float*>(input);
    // Device is configured mono, so samples are already mono floats.
    self->ring_.write(samples, frame_count);
}

}  // namespace audio
