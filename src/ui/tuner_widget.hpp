#pragma once

#include "widget.hpp"

namespace ui {

/**
 * Full-width tuner band sitting between the waterfall and spectrum panels.
 *
 * When a pitch is detected: draws a horizontal bar with a blue→green→red
 * gradient, tick marks at 0/±25/±50 cents, a needle at the current smoothed
 * deviation, and the note name + Hz + cents readout above the bar.
 *
 * When no pitch is detected (or the stability gate is pending): shows "--"
 * centred in the panel.
 *
 * Also draws a transparent overlay over the spectrum panel with a small
 * triangle marker at the dominant peak's frequency position.
 */
class TunerWidget : public Widget {
public:
  // spectrum_fraction / tuner_fraction: same fractions supplied to GpuPipeline,
  // used to position this widget at the correct vertical band.
  TunerWidget(float spectrum_fraction, float tuner_fraction);

  void draw(const FrameData& frame) override;

private:
  float spectrum_fraction_;
  float tuner_fraction_;
};

}  // namespace ui
