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
  [[nodiscard]] std::optional<float> update(
      const std::optional<DetectionResult>& raw,
      float    ema_alpha,
      uint32_t stability_frames) {
    if (!raw || raw->peaks.empty()) {
      candidate_count_ = 0;
      locked_          = false;
      return std::nullopt;
    }

    const NoteInfo note      = freq_to_note(raw->peaks[0].frequency);
    const float    raw_cents = note.cents_offset;
    const uint8_t  midi      = note.midi_note;

    if (midi == candidate_midi_) {
      if (candidate_count_ < stability_frames) {
        ++candidate_count_;
      }
    } else {
      candidate_midi_  = midi;
      candidate_count_ = 1;
      locked_          = false;
    }

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
  uint8_t  candidate_midi_{0};
  uint32_t candidate_count_{0};
  bool     locked_{false};
  float    locked_cents_{0.0f};
};

}  // namespace audio
