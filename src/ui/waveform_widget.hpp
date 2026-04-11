#pragma once

#include "widget.hpp"

namespace ui {

/**
 * Immediate-mode waveform display widget.
 *
 * Renders a scrollable window with a PlotLines visualization of audio samples.
 * This widget is temporary (replaced in Phase 2 by GPU-rendered waterfall/spectrum)
 * but establishes the pattern for future custom widgets like TunerWidget.
 *
 * Usage (each frame, between begin_frame/end_frame):
 *   waveform_widget.draw(frame_data);
 */
class WaveformWidget : public Widget {
 public:
  /**
   * Draw the waveform window.
   *
   * @param frame Per-frame state including waveform samples and framebuffer dimensions.
   */
  void draw(const FrameData& frame) override;
};

}  // namespace ui
