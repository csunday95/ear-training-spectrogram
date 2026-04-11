#pragma once

#include <span>

namespace ui {

/**
 * Per-frame application state passed to all UI widgets.
 *
 * Widgets receive a const reference to FrameData in their draw() method.
 * This struct grows with each phase as new data becomes available.
 *
 * Phase 1: waveform + framebuffer size
 * Phase 2: + FFT magnitude data, waterfall texture state
 * Phase 3: + capture state, pitch detection results
 */
struct FrameData {
  // Phase 1
  std::span<const float> waveform;
  int framebuffer_width;
  int framebuffer_height;

  // Phase 2 (reserved)
  // const audio::Magnitude* fft_data;

  // Phase 3 (reserved)
  // bool capture_running;
  // uint32_t ring_available;
  // float pitch_cents;
};

}  // namespace ui
