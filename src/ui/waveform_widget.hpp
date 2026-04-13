#pragma once

#include "widget.hpp"

namespace ui {

/**
 * Small waveform overlay widget (bottom-right corner).
 *
 * Dimensions are set at construction so they can be driven from CLI args or
 * a future grid-fraction tiling layout. No defaults — the caller is explicit.
 */
class WaveformWidget : public Widget {
public:
  // width / height: ImGui window size in pixels.
  // margin: gap between the widget edge and the framebuffer corner.
  WaveformWidget(float width, float height, float margin);

  void draw(const FrameData& frame) override;

private:
  float width_;
  float height_;
  float margin_;
};

}  // namespace ui
