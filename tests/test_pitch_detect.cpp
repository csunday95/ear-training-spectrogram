#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pitch_detect.hpp"

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr uint32_t kFftN       = 4096u;
constexpr uint32_t kSampleRate = 44100u;
constexpr uint32_t kFftBins    = kFftN / 2u + 1u;
constexpr float    kBinHz      = static_cast<float>(kSampleRate) / static_cast<float>(kFftN);

// Gaussian peak in dB centred at center_bin with the given peak level.
// sigma controls width; a tonal peak has sigma ~1–2 bins.
std::vector<float> make_spike(float center_bin, float peak_db, float sigma = 1.5f) {
  std::vector<float> mag(kFftBins, -120.0f);
  for (uint32_t bin = 0u; bin < kFftBins; ++bin) {
    const float d  = static_cast<float>(bin) - center_bin;
    const float db = peak_db + 20.0f * std::log10(
        std::exp(-0.5f * d * d / (sigma * sigma)));
    if (db > mag[bin]) {
      mag[bin] = db;
    }
  }
  return mag;
}

// Build a linear-amplitude spectrum with harmonic spikes for C3 (130.8 Hz).
// The 5th harmonic (~654 Hz) is boosted so it would win a magnitude-only test.
// Returns normalized linear amplitudes (simulating sync_get_linear_mag_data()).
std::vector<float> make_harmonic_series_linear(float fundamental_hz,
                                               uint32_t n_harmonics,
                                               uint32_t loud_harmonic) {
  std::vector<float> linear(kFftBins, 0.0f);
  for (uint32_t h = 1u; h <= n_harmonics; ++h) {
    const float freq      = fundamental_hz * static_cast<float>(h);
    const float center    = freq / kBinHz;
    // Loud harmonic is 10× louder (≈20 dB) than the fundamental.
    const float amplitude = (h == loud_harmonic) ? 1.0f : 0.1f;
    for (uint32_t bin = 0u; bin < kFftBins; ++bin) {
      const float d = static_cast<float>(bin) - center;
      linear[bin] += amplitude * std::exp(-0.5f * d * d / (1.5f * 1.5f));
    }
  }
  return linear;
}

}  // namespace

TEST_CASE("detect_peaks: single tone at A4", "[pitch_detect]") {
  // Round to nearest bin so the Gaussian peaks exactly at an integer bin.
  const float a4_bin      = std::round(440.0f / kBinHz);
  const float a4_expected = a4_bin * kBinHz;
  const auto  mag         = make_spike(a4_bin, 0.0f);

  const auto result = audio::detect_peaks(mag, kFftN, kSampleRate, -60.0f, 8.0f, 8u);

  REQUIRE(!result.peaks.empty());
  REQUIRE_THAT(result.peaks[0].frequency, WithinAbs(a4_expected, 5.0f));
  REQUIRE(result.peaks[0].hwhm_hz < 50.0f);
}

TEST_CASE("detect_peaks: peaks returned in ascending frequency order", "[pitch_detect]") {
  // A4 at −10 dB, E5 at 0 dB: E5 is louder, but A4 is lower in frequency.
  // detect_peaks() no longer sorts by magnitude — peaks come out low→high.
  const float a4_bin = std::round(440.0f / kBinHz);
  const float e5_bin = std::round(659.25f / kBinHz);

  auto mag = make_spike(a4_bin, -10.0f);
  for (uint32_t bin = 0u; bin < kFftBins; ++bin) {
    const float e5 = make_spike(e5_bin, 0.0f)[bin];
    if (e5 > mag[bin]) {
      mag[bin] = e5;
    }
  }

  const auto result = audio::detect_peaks(mag, kFftN, kSampleRate, -60.0f, 8.0f, 8u);

  REQUIRE(result.peaks.size() >= 2u);
  // A4 (lower frequency) should precede E5 in the output.
  const float a4_expected = a4_bin * kBinHz;
  const float e5_expected = e5_bin * kBinHz;
  REQUIRE_THAT(result.peaks[0].frequency, WithinAbs(a4_expected, 5.0f));
  REQUIRE_THAT(result.peaks[1].frequency, WithinAbs(e5_expected, 5.0f));
}

TEST_CASE("detect_peaks: below threshold returns no peaks", "[pitch_detect]") {
  const std::vector<float> mag(kFftBins, -120.0f);
  const auto result = audio::detect_peaks(mag, kFftN, kSampleRate, -60.0f, 8.0f, 8u);
  REQUIRE(result.peaks.empty());
}

TEST_CASE("detect_peaks: max_peaks limits results", "[pitch_detect]") {
  // Three well-separated spikes.
  auto mag = make_spike(100.0f, 0.0f);
  for (uint32_t bin = 0u; bin < kFftBins; ++bin) {
    mag[bin] = std::max(mag[bin], make_spike(500.0f, -5.0f)[bin]);
    mag[bin] = std::max(mag[bin], make_spike(1000.0f, -10.0f)[bin]);
  }

  const auto result = audio::detect_peaks(mag, kFftN, kSampleRate, -60.0f, 8.0f, 2u);
  REQUIRE(result.peaks.size() <= 2u);
}

TEST_CASE("hps_fundamental: selects root over loud overtone", "[pitch_detect]") {
  // C3 ≈ 130.8 Hz, 5th harmonic ≈ 654 Hz.
  // The 5th harmonic is 10× louder (≈20 dB) so detect_peaks alone would pick it.
  constexpr float kFundamental  = 130.8f;
  constexpr uint32_t kNHarmonics = 8u;
  constexpr uint32_t kLoudH      = 5u;

  const auto linear = make_harmonic_series_linear(kFundamental, kNHarmonics, kLoudH);

  // Confirm that the loud overtone wins without HPS (sanity-check the test fixture).
  const float loud_overtone_hz = kFundamental * static_cast<float>(kLoudH);
  float max_val = 0.0f;
  uint32_t max_bin = 0u;
  for (uint32_t b = 0u; b < kFftBins; ++b) {
    if (linear[b] > max_val) { max_val = linear[b]; max_bin = b; }
  }
  const float max_hz = static_cast<float>(max_bin) * kBinHz;
  // Loudest bin is near the overtone, not the fundamental.
  REQUIRE(std::abs(max_hz - loud_overtone_hz) < std::abs(max_hz - kFundamental));

  // HPS should recover the fundamental.
  const float hps_hz = audio::hps_fundamental(linear, kFftN, kSampleRate,
                                              27.5f, 4186.0f, 5u);
  // Within 50 cents of C3.
  const float cents_error = std::abs(1200.0f * std::log2(hps_hz / kFundamental));
  REQUIRE(cents_error < 50.0f);
}
