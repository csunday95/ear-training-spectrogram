#include "pitch_detect.hpp"

#include <algorithm>
#include <cmath>

namespace audio {

namespace {

// Minimum parabolic-interpolation denominator magnitude; below this the peak
// is treated as a flat top and the bin centre is used directly.
constexpr float kParabolicEpsilon = 1e-6f;

// Power half-maximum threshold in dB below a peak (−3 dB = half power).
constexpr float kHwhmThresholdDb = 3.0f;

}  // namespace

DetectionResult detect_peaks(std::span<const float> magnitude,
                             uint32_t               fft_n,
                             uint32_t               sample_rate,
                             float                  min_db,
                             float                  max_hwhm_bins,
                             uint32_t               max_peaks) {
  const auto num_bins = static_cast<uint32_t>(magnitude.size());
  if (num_bins < 3u) {
    return {};
  }

  const float bin_hz     = static_cast<float>(sample_rate) / static_cast<float>(fft_n);
  const float norm_denom = static_cast<float>(num_bins - 1u);

  DetectionResult result;

  for (uint32_t bin = 1u; bin + 1u < num_bins; ++bin) {
    const float bin_mag = magnitude[bin];

    // Local maximum check.
    if (bin_mag <= magnitude[bin - 1u] || bin_mag <= magnitude[bin + 1u]) {
      continue;
    }
    if (bin_mag < min_db) {
      continue;
    }

    // Parabolic interpolation for sub-bin frequency refinement.
    // Guard against a flat (zero-curvature) peak to avoid division by zero.
    const float left_mag  = magnitude[bin - 1u];
    const float right_mag = magnitude[bin + 1u];
    const float curvature = left_mag - 2.0f * bin_mag + right_mag;
    const float bin_frac  = (std::abs(curvature) > kParabolicEpsilon)
                               ? static_cast<float>(bin) +
                                     0.5f * (left_mag - right_mag) / curvature
                               : static_cast<float>(bin);

    // HWHM: count bins on each side where magnitude stays above bin_mag - 3 dB.
    const float hwhm_threshold = bin_mag - kHwhmThresholdDb;

    uint32_t left_bins = 0u;
    for (uint32_t scan = bin; scan > 0u; --scan) {
      if (magnitude[scan - 1u] >= hwhm_threshold) {
        ++left_bins;
      } else {
        break;
      }
    }

    uint32_t right_bins = 0u;
    for (uint32_t scan = bin + 1u; scan < num_bins; ++scan) {
      if (magnitude[scan] >= hwhm_threshold) {
        ++right_bins;
      } else {
        break;
      }
    }

    const float hwhm_bins = 0.5f * static_cast<float>(left_bins + right_bins);
    if (hwhm_bins > max_hwhm_bins) {
      continue;
    }

    result.peaks.push_back(DetectedPeak{
        .frequency      = bin_frac * bin_hz,
        .magnitude_db   = bin_mag,
        .hwhm_hz        = hwhm_bins * bin_hz,
        .bin_normalized = bin_frac / norm_denom,
    });
  }

  // Peaks are collected in bin-ascending order (the loop runs low→high).
  // No magnitude sort — the caller uses hps_fundamental() to select peaks[0].
  if (result.peaks.size() > static_cast<std::size_t>(max_peaks)) {
    result.peaks.resize(static_cast<std::size_t>(max_peaks));
  }

  return result;
}

float hps_fundamental(std::span<const float> magnitude_linear,
                      uint32_t               fft_n,
                      uint32_t               sample_rate,
                      float                  f_min,
                      float                  f_max,
                      uint32_t               n_harmonics) {
  const auto  num_bins = static_cast<uint32_t>(magnitude_linear.size());
  const float bin_hz   = static_cast<float>(sample_rate) / static_cast<float>(fft_n);

  if (num_bins < 3u || n_harmonics < 2u) {
    return f_min;
  }

  // Search range: clamp upper bound so bin * n_harmonics stays within the buffer.
  const auto bin_min = static_cast<uint32_t>(std::ceil(f_min / bin_hz));
  const auto bin_max = std::min(
      static_cast<uint32_t>(f_max / bin_hz),
      (num_bins - 1u) / n_harmonics);

  if (bin_min >= bin_max) {
    return f_min;
  }

  uint32_t best_bin = bin_min;
  float    best_hps = -1.0f;

  for (uint32_t k = bin_min; k <= bin_max; ++k) {
    float hps = 1.0f;
    for (uint32_t h = 1u; h <= n_harmonics; ++h) {
      hps *= magnitude_linear[k * h];
    }
    if (hps > best_hps) {
      best_hps = hps;
      best_bin = k;
    }
  }

  // The matched detected peak (selected in the caller by nearest-cents) carries
  // parabolic frequency refinement, so sub-bin precision is not needed here.
  return static_cast<float>(best_bin) * bin_hz;
}

}  // namespace audio
