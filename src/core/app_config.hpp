#pragma once

#include <cstdint>
#include <string>

namespace core {

struct AppConfig {
  // Display
  float db_min;
  float db_max;
  float spectrum_scale;
  float spectrum_fraction;  // fraction of framebuffer height for the spectrum panel
  float tuner_fraction;     // fraction of framebuffer height for the tuner band
  // Waveform overlay
  float wave_width;
  float wave_height;
  float wave_margin;
  // Pitch detection
  float    min_db;
  float    max_hwhm_bins;
  uint32_t max_peaks;
  // Tuner smoother
  float    ema_alpha;        // EMA weight for new cents value; lower = smoother
  uint32_t stability_frames; // consecutive same-note frames before committing to display
};

// Load AppConfig from a JSON file at path.
// Missing keys fall back to compiled-in defaults.
// If the file does not exist, the default config is written to path and returned.
[[nodiscard]] AppConfig load_app_config(const std::string& path);

}  // namespace core
