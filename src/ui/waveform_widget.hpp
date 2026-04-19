#pragma once

#include "widget.hpp"

namespace ui {

/**
 * Full-width waveform panel occupying the waveform_fraction band of the window,
 * positioned immediately below the waterfall.
 *
 * Panel fractions must match the values passed to GpuPipeline so that GL and
 * ImGui geometry stay in sync.
 */
class WaveformWidget : public Widget {
public:
  // spectrum_fraction: fraction of window height occupied by the spectrum panel.
  // waveform_fraction: fraction of window height occupied by this waveform panel.
  // tuner_fraction:    fraction of window height occupied by the tuner panel.
  WaveformWidget(float spectrum_fraction, float waveform_fraction, float tuner_fraction);

  void draw(const FrameData& frame) override;

private:
  float spectrum_fraction_;
  float waveform_fraction_;
  float tuner_fraction_;
};

}  // namespace ui
