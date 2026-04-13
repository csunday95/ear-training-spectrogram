#pragma once

#include "widget.hpp"

namespace ui {

/**
 * Transparent ImGui overlay drawn on top of the spectrum GL viewport.
 *
 * Draws:
 *   - Vertical frequency tick lines and labels (A0, C2, A2, C3 … C8) aligned
 *     with the log-frequency axis used by spectrum.vert and waterfall.frag.
 *   - Horizontal dB grid lines at multiples of 20 dB within the display range.
 *
 * Screen-X formula (matches spectrum.vert):
 *   t = log2(f / f_min) / log_range
 *   screen_x = t * fb_w
 *
 * Screen-Y formula (matches spectrum.vert amplitude mapping):
 *   y_norm = (db - db_min) / (db_max - db_min) * spectrum_scale
 *   screen_y = fb_h - y_norm * spectrum_h   (ImGui y: 0 = top)
 */
class SpectrumAxisWidget : public Widget {
public:
  SpectrumAxisWidget(float f_min, float f_max, float db_min, float db_max,
                     float spectrum_scale, float spectrum_fraction);
  void draw(const FrameData& frame) override;

private:
  float f_min_;
  float f_max_;
  float log_range_;       // log2(f_max / f_min), precomputed
  float db_min_;
  float db_max_;
  float spectrum_scale_;
  float spectrum_fraction_;
};

}  // namespace ui
