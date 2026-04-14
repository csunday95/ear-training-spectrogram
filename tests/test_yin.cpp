#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "yin.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr uint32_t kSampleRate = 44100u;
constexpr uint32_t kWindowSize = 1024u;
constexpr float    kThreshold  = 0.15f;
constexpr float    kFreqTol    = 5.0f;   // Hz tolerance for frequency assertions

// Generate a mono sine wave of n samples at freq Hz.
std::vector<float> make_sine(float freq, uint32_t n) {
  std::vector<float> buf(n);
  const float w = 2.0f * std::numbers::pi_v<float> * freq / static_cast<float>(kSampleRate);
  for (uint32_t i = 0u; i < n; ++i) {
    buf[i] = std::sin(w * static_cast<float>(i));
  }
  return buf;
}

// Generate a sawtooth wave of n samples at freq Hz, amplitude in [-1, 1].
std::vector<float> make_sawtooth(float freq, uint32_t n) {
  std::vector<float> buf(n);
  const float period = static_cast<float>(kSampleRate) / freq;
  for (uint32_t i = 0u; i < n; ++i) {
    const float phase = std::fmod(static_cast<float>(i), period) / period;
    buf[i] = 2.0f * phase - 1.0f;
  }
  return buf;
}

}  // namespace

TEST_CASE("yin: sine at A4 (440 Hz)", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  const auto result = yin.estimate(make_sine(440.0f, 4096u));

  REQUIRE(result.frequency > 0.0f);
  REQUIRE_THAT(result.frequency, WithinAbs(440.0f, kFreqTol));
  REQUIRE(result.aperiodicity < kThreshold);
}

TEST_CASE("yin: sine at C3 (130 Hz)", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  const auto result = yin.estimate(make_sine(130.81f, 4096u));

  REQUIRE(result.frequency > 0.0f);
  REQUIRE_THAT(result.frequency, WithinAbs(130.81f, kFreqTol));
}

TEST_CASE("yin: sine at soprano C6 (1047 Hz)", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  const auto result = yin.estimate(make_sine(1046.5f, 4096u));

  REQUIRE(result.frequency > 0.0f);
  REQUIRE_THAT(result.frequency, WithinAbs(1046.5f, kFreqTol));
}

TEST_CASE("yin: sawtooth at E4 (330 Hz)", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  const auto result = yin.estimate(make_sawtooth(329.63f, 4096u));

  REQUIRE(result.frequency > 0.0f);
  REQUIRE_THAT(result.frequency, WithinAbs(329.63f, kFreqTol));
}

TEST_CASE("yin: silence returns no pitch", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  const auto result = yin.estimate(std::vector<float>(4096u, 0.0f));

  REQUIRE(result.frequency == 0.0f);
}

TEST_CASE("yin: buffer too small returns no pitch", "[yin]") {
  const audio::Yin yin{kSampleRate, kWindowSize, kThreshold};
  // tau_max for 80 Hz at 44100 = 551; window_size + tau_max = 1575; use 100 < that.
  const auto result = yin.estimate(std::vector<float>(100u, 0.5f));

  REQUIRE(result.frequency == 0.0f);
}
