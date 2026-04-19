#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace audio {

struct DetectedPeak {
  // Frequency in Hz, refined via parabolic interpolation.
  float frequency;
  // Magnitude at the peak bin in dB.
  float magnitude_db;
  // Half-width at half-maximum power (-3 dB point), in Hz.
  // Narrow = tonal; wide = broadband / noise.
  float hwhm_hz;
  // Normalized bin position in [0, 1]: 0 = DC, 1 = Nyquist.
  // Maps directly to the spectrum display x coordinate (bin / (fft_bins - 1)).
  float bin_normalized;
};

struct DetectionResult {
  // Peaks in ascending frequency order, at most max_peaks entries.
  // The caller is expected to move the HPS-selected fundamental to peaks[0]
  // after running hps_fundamental().
  std::vector<DetectedPeak> peaks;
};

// Peak-pick a dB magnitude spectrum. Returns peaks in ascending frequency order.
//   magnitude:     span of fft_n/2+1 floats in dB (from GpuPipeline::sync_get_mag_data())
//   fft_n:         FFT window size used to compute the spectrum
//   sample_rate:   audio sample rate in Hz
//   min_db:        ignore bins at or below this level
//   max_hwhm_bins: reject peaks wider than this many bins (broadband noise filter)
//   max_peaks:     maximum number of peaks to return
[[nodiscard]] DetectionResult detect_peaks(std::span<const float> magnitude,
                                           uint32_t               fft_n,
                                           uint32_t               sample_rate,
                                           float                  min_db,
                                           float                  max_hwhm_bins,
                                           uint32_t               max_peaks);

// Estimate the fundamental frequency using the Harmonic Product Spectrum (HPS).
// Multiplies the linear magnitude spectrum by downsampled copies of itself;
// the argmax of the product identifies the frequency with energy at the most
// harmonic multiples — typically the fundamental even when overtones are louder.
//
//   magnitude_linear: span of fft_n/2+1 normalized linear amplitudes
//                     (from GpuPipeline::sync_get_linear_mag_data())
//   fft_n:            FFT window size
//   sample_rate:      audio sample rate in Hz
//   f_min / f_max:    search range in Hz
//   n_harmonics:      number of harmonic copies to multiply (≥ 2; 5 is a good default)
//
// Returns the estimated fundamental in Hz. If the search range is empty or
// n_harmonics would push harmonic bins out of range, returns f_min as a safe fallback.
[[nodiscard]] float hps_fundamental(std::span<const float> magnitude_linear,
                                    uint32_t               fft_n,
                                    uint32_t               sample_rate,
                                    float                  f_min,
                                    float                  f_max,
                                    uint32_t               n_harmonics);

}  // namespace audio
