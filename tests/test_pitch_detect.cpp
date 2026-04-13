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

TEST_CASE("detect_peaks: strongest peak returned first", "[pitch_detect]") {
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
  // E5 (0 dB) is stronger than A4 (−10 dB) so it should be first.
  const float e5_expected = e5_bin * kBinHz;
  REQUIRE_THAT(result.peaks[0].frequency, WithinAbs(e5_expected, 5.0f));
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
