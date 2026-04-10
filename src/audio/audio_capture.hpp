#pragma once

#include "miniaudio.h"
#include "ring_buffer.hpp"
#include <cstdint>

namespace audio {

// 16384 floats @ 44.1kHz ≈ 372ms. Enough for several overlapping FFT windows.
static constexpr uint32_t kRingCapacity = 16384;

// Captures audio from the default input device into a RingBuffer<float>.
// Mono, 32-bit float PCM. Stereo devices are averaged to mono in the callback.
class AudioCapture {
public:
    explicit AudioCapture(uint32_t sample_rate = 44100);
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start();
    void stop();

    RingBuffer<float, kRingCapacity>& ring() { return ring_; }
    const RingBuffer<float, kRingCapacity>& ring() const { return ring_; }
    uint32_t sample_rate() const { return sample_rate_; }
    bool running() const { return running_; }

private:
    static void data_callback(ma_device* device, void* output, const void* input,
                              uint32_t frame_count);

    const uint32_t sample_rate_;
    RingBuffer<float, kRingCapacity> ring_;
    ma_device device_{};
    bool running_{false};
};

}  // namespace audio
