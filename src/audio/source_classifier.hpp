#pragma once

#include <cmath>
#include <cstdint>
#include <span>

namespace audio {

enum class AudioSource : uint8_t {
  Unknown,
  Piano,
  Voice,
};

// Amplitude-envelope state machine for voice/piano source discrimination.
//
// Tracks short-time RMS per hop. A sharp onset (rapid RMS increase) → Piano.
// Sustained steady-state amplitude → Voice. Silence → Unknown.
//
// All thresholds require iterative tuning with real audio.
class SourceClassifier {
public:
  // onset_threshold:  normalised RMS rate-of-change (new/avg - 1) that triggers Piano
  // piano_hold_hops:  hops to hold Piano state after an onset before returning to Voice
  // silence_rms:      RMS below this → Unknown
  SourceClassifier(float onset_threshold, uint32_t piano_hold_hops, float silence_rms)
      : onset_threshold_{onset_threshold}
      , piano_hold_hops_{piano_hold_hops}
      , silence_rms_{silence_rms} {}

  [[nodiscard]] AudioSource update(std::span<const float> samples) {
    float sum_sq = 0.0f;
    for (const float s : samples) {
      sum_sq += s * s;
    }
    const float rms = std::sqrt(sum_sq / static_cast<float>(samples.size()));

    if (rms < silence_rms_) {
      smooth_rms_      = 0.0f;
      piano_hold_left_ = 0u;
      return AudioSource::Unknown;
    }

    // Normalised onset rate: how much RMS jumped relative to running average.
    const float onset_rate = (smooth_rms_ > 1e-6f)
        ? (rms - smooth_rms_) / smooth_rms_
        : 0.0f;

    // EMA envelope tracker (~5-hop time constant).
    smooth_rms_ = 0.2f * rms + 0.8f * smooth_rms_;

    if (onset_rate > onset_threshold_) {
      piano_hold_left_ = piano_hold_hops_;
    }

    if (piano_hold_left_ > 0u) {
      --piano_hold_left_;
      return AudioSource::Piano;
    }

    return AudioSource::Voice;
  }

private:
  float    onset_threshold_;
  uint32_t piano_hold_hops_;
  float    silence_rms_;

  float    smooth_rms_{0.0f};
  uint32_t piano_hold_left_{0u};
};

}  // namespace audio
