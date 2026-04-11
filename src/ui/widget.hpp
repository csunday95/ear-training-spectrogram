#pragma once

#include "frame_data.hpp"

namespace ui {

/**
 * Abstract base class for ImGui widgets.
 *
 * Each widget subclass (WaveformWidget, TunerWidget, etc.) implements draw()
 * to render its ImGui content during the frame.
 *
 * Widgets receive per-frame state via const reference to FrameData, which grows
 * with each phase as new data becomes available (FFT results, pitch detection, etc.).
 * This keeps all widgets using the same draw signature while remaining extensible.
 */
class Widget {
 public:
  virtual ~Widget() = default;

  /**
   * Draw the widget. Must be called between ImGuiRenderer::begin_frame()
   * and ImGuiRenderer::end_frame().
   *
   * @param frame Per-frame application state.
   */
  virtual void draw(const FrameData& frame) = 0;

  // Non-copyable and non-movable (each widget owns its state)
  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;

 protected:
  Widget() = default;
};

}  // namespace ui
