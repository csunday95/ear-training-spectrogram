#pragma once

#include <cstdint>
#include <span>

namespace audio {

struct YinResult {
  // Estimated fundamental frequency in Hz. 0 if no pitch detected.
  float frequency;
  // CMNDF value at the detected tau (0 = perfectly periodic, 1 = noise).
  // Values below the detection threshold indicate a credible pitch estimate.
  float aperiodicity;
};

// CPU YIN pitch detector operating on time-domain samples.
// Reference: de Cheveigné & Kawahara, "YIN, a fundamental frequency estimator
// for speech and music" (JASA 2002).
// Detects pitches in the 80–1200 Hz range (full singing voice).
class Yin {
public:
  // sample_rate:  audio sample rate in Hz
  // window_size:  integration window W in samples; input must have >= window_size + sr/80 samples
  // threshold:    CMNDF detection threshold; lower is stricter (typical: 0.10–0.20)
  Yin(uint32_t sample_rate, uint32_t window_size, float threshold);

  // Returns YinResult with frequency=0 if no credible pitch is found.
  [[nodiscard]] YinResult estimate(std::span<const float> samples) const;

private:
  uint32_t sample_rate_;
  uint32_t window_size_;
  float    threshold_;
};

}  // namespace audio
