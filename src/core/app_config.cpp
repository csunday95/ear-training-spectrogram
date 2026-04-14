#include "app_config.hpp"

#include "log.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <string_view>

namespace core {

namespace {

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

// Populate a Param<T> from a JSON section using the param's own key.
// Falls back to the current value (i.e. the compiled-in default) if the key is absent.
template<typename T>
void load_param(const nlohmann::json& section, Param<T>& param) {
  param.value = section.value(param.key, param.value);
}

// Known top-level section names — derived from kSection constants so they can't drift.
constexpr std::array<std::string_view, 5> kTopLevelKeys = {
    DisplayConfig::kSection, WaveformConfig::kSection,
    PitchConfig::kSection,   TunerConfig::kSection,
    YinConfig::kSection};

void write_default_config(const std::string& path, const AppConfig& cfg) {
  using json       = nlohmann::json;
  const auto& dc   = cfg.display;
  const auto& wc   = cfg.waveform_overlay;
  const auto& pd   = cfg.pitch_detection;
  const auto& tc   = cfg.tuner_smoother;
  const auto& yc   = cfg.yin;
  const json j = {
      {DisplayConfig::kSection,
       {{dc.db_min.key,            dc.db_min.value},
        {dc.db_max.key,            dc.db_max.value},
        {dc.spectrum_scale.key,    dc.spectrum_scale.value},
        {dc.spectrum_fraction.key, dc.spectrum_fraction.value},
        {dc.tuner_fraction.key,    dc.tuner_fraction.value},
        {dc.log_freq_min.key,      dc.log_freq_min.value},
        {dc.log_freq_max.key,      dc.log_freq_max.value},
        {dc.smooth_alpha.key,      dc.smooth_alpha.value},
        {dc.max_hold_decay_db.key, dc.max_hold_decay_db.value}}},
      {WaveformConfig::kSection,
       {{wc.width.key,  wc.width.value},
        {wc.height.key, wc.height.value},
        {wc.margin.key, wc.margin.value}}},
      {PitchConfig::kSection,
       {{pd.min_db.key,                  pd.min_db.value},
        {pd.max_hwhm_bins.key,           pd.max_hwhm_bins.value},
        {pd.max_peaks.key,               pd.max_peaks.value},
        {pd.noise_estimation_frames.key, pd.noise_estimation_frames.value},
        {pd.noise_floor_margin_db.key,   pd.noise_floor_margin_db.value}}},
      {TunerConfig::kSection,
       {{tc.ema_alpha.key,        tc.ema_alpha.value},
        {tc.stability_frames.key, tc.stability_frames.value},
        {tc.gate_cents.key,       tc.gate_cents.value}}},
      {YinConfig::kSection,
       {{yc.window_size.key,     yc.window_size.value},
        {yc.threshold.key,       yc.threshold.value},
        {yc.onset_threshold.key, yc.onset_threshold.value},
        {yc.piano_hold_hops.key, yc.piano_hold_hops.value},
        {yc.silence_rms.key,     yc.silence_rms.value}}},
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
  AppConfig cfg{};  // all Param<T> member defaults kick in

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

  if (j.contains(DisplayConfig::kSection)) {
    const auto& d = j[DisplayConfig::kSection];
    auto& dc      = cfg.display;
    warn_unknown_keys(d, DisplayConfig::kSection,
        {dc.db_min.key, dc.db_max.key, dc.spectrum_scale.key, dc.spectrum_fraction.key,
         dc.tuner_fraction.key, dc.log_freq_min.key, dc.log_freq_max.key,
         dc.smooth_alpha.key, dc.max_hold_decay_db.key});
    load_param(d, dc.db_min);
    load_param(d, dc.db_max);
    load_param(d, dc.spectrum_scale);
    load_param(d, dc.spectrum_fraction);
    load_param(d, dc.tuner_fraction);
    load_param(d, dc.log_freq_min);
    load_param(d, dc.log_freq_max);
    load_param(d, dc.smooth_alpha);
    load_param(d, dc.max_hold_decay_db);
  }
  if (j.contains(WaveformConfig::kSection)) {
    const auto& w = j[WaveformConfig::kSection];
    auto& wc      = cfg.waveform_overlay;
    warn_unknown_keys(w, WaveformConfig::kSection, {wc.width.key, wc.height.key, wc.margin.key});
    load_param(w, wc.width);
    load_param(w, wc.height);
    load_param(w, wc.margin);
  }
  if (j.contains(PitchConfig::kSection)) {
    const auto& p = j[PitchConfig::kSection];
    auto& pd      = cfg.pitch_detection;
    warn_unknown_keys(p, PitchConfig::kSection,
        {pd.min_db.key, pd.max_hwhm_bins.key, pd.max_peaks.key,
         pd.noise_estimation_frames.key, pd.noise_floor_margin_db.key});
    load_param(p, pd.min_db);
    load_param(p, pd.max_hwhm_bins);
    load_param(p, pd.max_peaks);
    load_param(p, pd.noise_estimation_frames);
    load_param(p, pd.noise_floor_margin_db);
  }
  if (j.contains(TunerConfig::kSection)) {
    const auto& t = j[TunerConfig::kSection];
    auto& tc      = cfg.tuner_smoother;
    warn_unknown_keys(t, TunerConfig::kSection,
        {tc.ema_alpha.key, tc.stability_frames.key, tc.gate_cents.key});
    load_param(t, tc.ema_alpha);
    load_param(t, tc.stability_frames);
    load_param(t, tc.gate_cents);
  }
  if (j.contains(YinConfig::kSection)) {
    const auto& y = j[YinConfig::kSection];
    auto& yc      = cfg.yin;
    warn_unknown_keys(y, YinConfig::kSection,
        {yc.window_size.key, yc.threshold.key, yc.onset_threshold.key,
         yc.piano_hold_hops.key, yc.silence_rms.key});
    load_param(y, yc.window_size);
    load_param(y, yc.threshold);
    load_param(y, yc.onset_threshold);
    load_param(y, yc.piano_hold_hops);
    load_param(y, yc.silence_rms);
  }

  return cfg;
}

}  // namespace core
