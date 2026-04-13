#pragma once

#include "param.hpp"

#include <cstdint>
#include <string>

namespace core {

struct DisplayConfig {
  static constexpr const char* kSection = "display";
  Param<float> db_min            {"db_min",            -80.0f};
  Param<float> db_max            {"db_max",              0.0f};
  Param<float> spectrum_scale    {"spectrum_scale",      0.9f};
  Param<float> spectrum_fraction {"spectrum_fraction",   0.40f};
  Param<float> tuner_fraction    {"tuner_fraction",      0.20f};
  Param<float> log_freq_min      {"log_freq_min",       27.5f};   // A0 — lowest piano key
  Param<float> log_freq_max      {"log_freq_max",     4186.0f};   // C8 — highest piano key
  Param<float> smooth_alpha      {"smooth_alpha",        0.3f};   // EMA weight for per-bin magnitude smoothing
  Param<float> max_hold_decay_db {"max_hold_decay_db",   0.4f};   // dB subtracted from max-hold envelope per frame
};

struct WaveformConfig {
  static constexpr const char* kSection = "waveform_overlay";
  Param<float> width  {"width",  300.0f};
  Param<float> height {"height",  80.0f};
  Param<float> margin {"margin",  10.0f};
};

struct PitchConfig {
  static constexpr const char* kSection = "pitch_detection";
  Param<float>    min_db                  {"min_db",                   -50.0f};
  Param<float>    max_hwhm_bins           {"max_hwhm_bins",              8.0f};
  Param<uint32_t> max_peaks               {"max_peaks",                    8u};
  Param<uint32_t> noise_estimation_frames {"noise_estimation_frames",     60u};  // ~3s at 21 FFT frames/s
  Param<float>    noise_floor_margin_db   {"noise_floor_margin_db",      3.0f};
};

struct TunerConfig {
  static constexpr const char* kSection = "tuner_smoother";
  Param<float>    ema_alpha        {"ema_alpha",        0.3f};   // EMA weight for new cents value
  Param<uint32_t> stability_frames {"stability_frames",    4u};  // consecutive frames within gate before committing
  Param<float>    gate_cents       {"gate_cents",        80.0f}; // frequency jump threshold (cents) that resets the gate
};

struct AppConfig {
  DisplayConfig  display;
  WaveformConfig waveform_overlay;
  PitchConfig    pitch_detection;
  TunerConfig    tuner_smoother;
};

// Load AppConfig from a JSON file at path.
// Missing keys fall back to compiled-in defaults.
// If the file does not exist, the default config is written to path and returned.
[[nodiscard]] AppConfig load_app_config(const std::string& path);

}  // namespace core
