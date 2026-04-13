#include "app_config.hpp"

#include "log.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <string_view>

namespace core {

namespace {

constexpr float    kDefaultDbMin           = -80.f;
constexpr float    kDefaultDbMax           = 0.f;
constexpr float    kDefaultSpectrumScale   = 0.9f;
constexpr float    kDefaultSpectrumFraction = 0.40f;
constexpr float    kDefaultTunerFraction   = 0.20f;
constexpr float    kDefaultWaveWidth       = 300.f;
constexpr float    kDefaultWaveHeight      = 80.f;
constexpr float    kDefaultWaveMargin      = 10.f;
constexpr float    kDefaultMinDb           = -50.f;
constexpr float    kDefaultMaxHwhmBins     = 8.f;
constexpr uint32_t kDefaultMaxPeaks        = 8u;
constexpr float    kDefaultEmaAlpha        = 0.3f;
constexpr uint32_t kDefaultStabilityFrames = 4u;
constexpr float    kDefaultGateCents       = 80.0f;

// Warn about any key in `section` that is not in `known`.
void warn_unknown_keys(const nlohmann::json&                    section,
                       std::string_view                          section_name,
                       std::initializer_list<std::string_view>   known) {
  for (const auto& [key, _] : section.items()) {
    const bool recognised = [&] {
      for (const auto& k : known) {
        if (key == k) {
          return true;
        }
      }
      return false;
    }();
    if (!recognised) {
      LOG_WARN("config: unknown key \"{}.{}\"", section_name, key);
    }
  }
}

// Known top-level section names.
constexpr std::array<std::string_view, 4> kTopLevelKeys = {
    "display", "waveform_overlay", "pitch_detection", "tuner_smoother"};

void write_default_config(const std::string& path, const AppConfig& cfg) {
  using json   = nlohmann::json;
  const json j = {
      {"display",
       {{"db_min", cfg.db_min},
        {"db_max", cfg.db_max},
        {"spectrum_scale", cfg.spectrum_scale},
        {"spectrum_fraction", cfg.spectrum_fraction},
        {"tuner_fraction", cfg.tuner_fraction}}},
      {"waveform_overlay",
       {{"width", cfg.wave_width},
        {"height", cfg.wave_height},
        {"margin", cfg.wave_margin}}},
      {"pitch_detection",
       {{"min_db", cfg.min_db},
        {"max_hwhm_bins", cfg.max_hwhm_bins},
        {"max_peaks", cfg.max_peaks}}},
      {"tuner_smoother",
       {{"ema_alpha", cfg.ema_alpha},
        {"stability_frames", cfg.stability_frames},
        {"gate_cents", cfg.gate_cents}}},
  };
  std::ofstream out{path};
  if (out) {
    out << j.dump(2) << '\n';
    LOG_INFO("config: wrote defaults to {}", path);
  } else {
    LOG_WARN("config: could not write default config to {}", path);
  }
}

}  // namespace

AppConfig load_app_config(const std::string& path) {
  AppConfig cfg{
      .db_min            = kDefaultDbMin,
      .db_max            = kDefaultDbMax,
      .spectrum_scale    = kDefaultSpectrumScale,
      .spectrum_fraction = kDefaultSpectrumFraction,
      .tuner_fraction    = kDefaultTunerFraction,
      .wave_width        = kDefaultWaveWidth,
      .wave_height       = kDefaultWaveHeight,
      .wave_margin       = kDefaultWaveMargin,
      .min_db            = kDefaultMinDb,
      .max_hwhm_bins     = kDefaultMaxHwhmBins,
      .max_peaks         = kDefaultMaxPeaks,
      .ema_alpha         = kDefaultEmaAlpha,
      .stability_frames  = kDefaultStabilityFrames,
      .gate_cents        = kDefaultGateCents,
  };

  std::ifstream file{path};
  if (!file.is_open()) {
    write_default_config(path, cfg);
    return cfg;
  }

  using json = nlohmann::json;
  json j;
  try {
    file >> j;
  } catch (const json::exception& ex) {
    LOG_ERROR("config: parse error in {}: {}", path, ex.what());
    return cfg;
  }

  // Warn on unknown top-level sections.
  for (const auto& [key, _] : j.items()) {
    const bool recognised = [&] {
      for (const auto& k : kTopLevelKeys) {
        if (key == k) {
          return true;
        }
      }
      return false;
    }();
    if (!recognised) {
      LOG_WARN("config: unknown top-level key \"{}\"", key);
    }
  }

  if (j.contains("display")) {
    const auto& d = j["display"];
    warn_unknown_keys(d, "display",
        {"db_min", "db_max", "spectrum_scale", "spectrum_fraction", "tuner_fraction"});
    cfg.db_min            = d.value("db_min",            cfg.db_min);
    cfg.db_max            = d.value("db_max",            cfg.db_max);
    cfg.spectrum_scale    = d.value("spectrum_scale",    cfg.spectrum_scale);
    cfg.spectrum_fraction = d.value("spectrum_fraction", cfg.spectrum_fraction);
    cfg.tuner_fraction    = d.value("tuner_fraction",    cfg.tuner_fraction);
  }
  if (j.contains("waveform_overlay")) {
    const auto& w = j["waveform_overlay"];
    warn_unknown_keys(w, "waveform_overlay", {"width", "height", "margin"});
    cfg.wave_width  = w.value("width",  cfg.wave_width);
    cfg.wave_height = w.value("height", cfg.wave_height);
    cfg.wave_margin = w.value("margin", cfg.wave_margin);
  }
  if (j.contains("pitch_detection")) {
    const auto& p = j["pitch_detection"];
    warn_unknown_keys(p, "pitch_detection", {"min_db", "max_hwhm_bins", "max_peaks"});
    cfg.min_db        = p.value("min_db",        cfg.min_db);
    cfg.max_hwhm_bins = p.value("max_hwhm_bins", cfg.max_hwhm_bins);
    cfg.max_peaks     = p.value("max_peaks",     cfg.max_peaks);
  }
  if (j.contains("tuner_smoother")) {
    const auto& t = j["tuner_smoother"];
    warn_unknown_keys(t, "tuner_smoother", {"ema_alpha", "stability_frames", "gate_cents"});
    cfg.ema_alpha        = t.value("ema_alpha",        cfg.ema_alpha);
    cfg.stability_frames = t.value("stability_frames", cfg.stability_frames);
    cfg.gate_cents       = t.value("gate_cents",       cfg.gate_cents);
  }

  return cfg;
}

}  // namespace core
