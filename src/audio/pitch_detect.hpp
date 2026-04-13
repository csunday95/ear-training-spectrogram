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
  // Peaks sorted by magnitude_db descending, at most max_peaks entries.
  std::vector<DetectedPeak> peaks;
};

// Peak-pick a dB magnitude spectrum.
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

}  // namespace audio
