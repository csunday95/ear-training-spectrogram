#pragma once

#include <optional>
#include <span>

#include "pitch_detect.hpp"

namespace ui {

/**
 * Per-frame application state passed to all UI widgets.
 *
 * Widgets receive a const reference to FrameData in their draw() method.
 */
struct FrameData {
  std::span<const float> waveform;
  int framebuffer_width;
  int framebuffer_height;

  // Nullopt when no stable pitch is detected (either no peaks above threshold,
  // or the note-stability gate has not yet committed).
  std::optional<audio::DetectionResult> pitch;

  // EMA-smoothed cents offset for the tuner display. Valid only when pitch has a value.
  float smoothed_cents;

  // Log-frequency-mapped x position of the dominant peak, in [0, 1] (0=left, 1=right).
  // Used by overlay widgets to position markers aligned with the spectrum display.
  // Valid only when pitch has a value.
  float spectrum_peak_x_norm;
};

}  // namespace ui
