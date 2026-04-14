#include "yin.hpp"

#include <vector>

namespace audio {

namespace {

// Frequency bounds for the YIN search range, covering the full singing voice.
constexpr float kMinFreqHz = 80.0f;
constexpr float kMaxFreqHz = 1200.0f;

}  // namespace

Yin::Yin(uint32_t sample_rate, uint32_t window_size, float threshold)
    : sample_rate_{sample_rate}
    , window_size_{window_size}
    , threshold_{threshold} {}

YinResult Yin::estimate(std::span<const float> samples) const {
  const auto tau_max = static_cast<uint32_t>(
      static_cast<float>(sample_rate_) / kMinFreqHz);
  const auto tau_min = static_cast<uint32_t>(
      static_cast<float>(sample_rate_) / kMaxFreqHz) + 1u;

  if (samples.size() < static_cast<size_t>(window_size_ + tau_max)) {
    return {0.0f, 1.0f};
  }
  if (tau_min >= tau_max) {
    return {0.0f, 1.0f};
  }

  // Silence guard: CMNDF is undefined when the signal has no energy.
  float energy = 0.0f;
  for (uint32_t j = 0u; j < window_size_; ++j) {
    energy += samples[j] * samples[j];
  }
  if (energy < 1e-10f) {
    return {0.0f, 1.0f};
  }

  // Step 1: Difference function.
  // d[tau] = sum_{j=0}^{W-1} (x[j] - x[j+tau])^2
  // Index 0 is kept at 0.0 and never read in CMNDF.
  std::vector<float> d(tau_max + 1u, 0.0f);
  for (uint32_t tau = 1u; tau <= tau_max; ++tau) {
    for (uint32_t j = 0u; j < window_size_; ++j) {
      const float diff = samples[j] - samples[j + tau];
      d[tau] += diff * diff;
    }
  }

  // Step 2: Cumulative mean normalized difference function (CMNDF).
  // cmndf[0]   = 1  (by definition)
  // cmndf[tau] = tau * d[tau] / sum_{k=1}^{tau} d[k]
  std::vector<float> cmndf(tau_max + 1u);
  cmndf[0] = 1.0f;
  float running_sum = 0.0f;
  for (uint32_t tau = 1u; tau <= tau_max; ++tau) {
    running_sum += d[tau];
    cmndf[tau] = (running_sum > 0.0f)
        ? (static_cast<float>(tau) * d[tau] / running_sum)
        : 0.0f;
  }

  // Step 3: Find the first local minimum below threshold_ in [tau_min, tau_max].
  uint32_t tau_best = 0u;
  for (uint32_t tau = tau_min; tau < tau_max; ++tau) {
    if (cmndf[tau] < threshold_) {
      // Walk right to the local minimum.
      while (tau + 1u <= tau_max && cmndf[tau + 1u] < cmndf[tau]) {
        ++tau;
      }
      tau_best = tau;
      break;
    }
  }

  if (tau_best == 0u) {
    return {0.0f, 1.0f};
  }

  const float aperiodicity = cmndf[tau_best];

  // Step 4: Parabolic interpolation for sub-sample accuracy.
  // Vertex of parabola through (tau-1, y0), (tau, y1), (tau+1, y2):
  //   delta = (y0 - y2) / (2 * (y0 + y2 - 2*y1))
  float tau_f = static_cast<float>(tau_best);
  if (tau_best > tau_min && tau_best < tau_max) {
    const float y0    = cmndf[tau_best - 1u];
    const float y1    = cmndf[tau_best];
    const float y2    = cmndf[tau_best + 1u];
    const float denom = y0 + y2 - 2.0f * y1;
    if (denom > 1e-6f) {
      tau_f += (y0 - y2) / (2.0f * denom);
    }
  }

  const float freq = static_cast<float>(sample_rate_) / tau_f;
  return {freq, aperiodicity};
}

}  // namespace audio
