#pragma once

#include "music_theory.hpp"
#include "pitch_detect.hpp"

#include <cstdint>
#include <optional>

namespace audio {

// EMA smoothing + consecutive-frame note-stability gate for pitch display.
//
// The gate requires `stability_frames` consecutive frames reporting the same
// MIDI note before committing to display. Once locked, the displayed cents
// value is updated via EMA each frame. The gate resets on silence or a note
// change.
//
// Note: frame-count gating is refresh-rate dependent; a future iteration
// should gate on wall-clock duration instead.
class PitchSmoother {
public:
  // Returns EMA-smoothed cents when locked, nullopt during the gate period or
  // when no pitch is present.
  //
  // Gate logic: the candidate frequency tracks the detected pitch each frame.
  // If the new detection is within gate_cents of the previous frame's frequency
  // the stability counter increments; a jump larger than gate_cents resets it.
  // This tolerates slow vocal drift without resetting, while still reacting to
  // genuine note changes.
  [[nodiscard]] std::optional<float> update(
      const std::optional<DetectionResult>& raw,
      float    ema_alpha,
      uint32_t stability_frames,
      float    gate_cents) {
    if (!raw || raw->peaks.empty()) {
      candidate_count_ = 0;
      locked_          = false;
      return std::nullopt;
    }

    const float    raw_freq  = raw->peaks[0].frequency;
    const NoteInfo note      = freq_to_note(raw_freq);
    const float    raw_cents = note.cents_offset;

    // On the first call after silence, candidate_count_ == 0 so we always
    // treat the detection as a new candidate.
    const float cents_distance = (candidate_count_ == 0)
        ? gate_cents + 1.0f
        : std::abs(1200.0f * std::log2(raw_freq / candidate_freq_));

    if (cents_distance >= gate_cents) {
      candidate_count_ = 1;
      locked_          = false;
    } else if (candidate_count_ < stability_frames) {
      ++candidate_count_;
    }
    // Always update so the window follows slow pitch drift.
    candidate_freq_ = raw_freq;

    if (candidate_count_ >= stability_frames) {
      if (!locked_) {
        locked_       = true;
        locked_cents_ = raw_cents;
      } else {
        locked_cents_ = ema_alpha * raw_cents + (1.0f - ema_alpha) * locked_cents_;
      }
    }

    return locked_ ? std::optional<float>{locked_cents_} : std::nullopt;
  }

private:
  float    candidate_freq_{0.0f};
  uint32_t candidate_count_{0};
  bool     locked_{false};
  float    locked_cents_{0.0f};
};

}  // namespace audio
