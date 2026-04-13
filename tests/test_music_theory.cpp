#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "music_theory.hpp"

#include <cmath>

using Catch::Matchers::WithinAbs;

TEST_CASE("freq_to_note: A4 = 440 Hz", "[music_theory]") {
  const auto info = audio::freq_to_note(440.0f);
  REQUIRE(info.midi_note == 69u);
  REQUIRE(info.octave == 4);
  REQUIRE(info.name == "A4");
  REQUIRE_THAT(info.frequency, WithinAbs(440.0f, 0.01f));
  REQUIRE_THAT(info.cents_offset, WithinAbs(0.0f, 0.1f));
}

TEST_CASE("freq_to_note: A4 sharp by 10 cents", "[music_theory]") {
  const float hz   = 440.0f * std::pow(2.0f, 10.0f / 1200.0f);
  const auto  info = audio::freq_to_note(hz);
  REQUIRE(info.midi_note == 69u);
  REQUIRE(info.name == "A4");
  REQUIRE_THAT(info.cents_offset, WithinAbs(10.0f, 0.5f));
}

TEST_CASE("freq_to_note: C4 middle C", "[music_theory]") {
  const auto info = audio::freq_to_note(261.63f);
  REQUIRE(info.midi_note == 60u);
  REQUIRE(info.octave == 4);
  REQUIRE(info.name == "C4");
}

TEST_CASE("freq_to_note: C5 above middle C", "[music_theory]") {
  const auto info = audio::freq_to_note(523.25f);
  REQUIRE(info.midi_note == 72u);
  REQUIRE(info.octave == 5);
  REQUIRE(info.name == "C5");
}

TEST_CASE("freq_to_note: octave boundary C0", "[music_theory]") {
  const auto info = audio::freq_to_note(16.35f);
  REQUIRE(info.octave == 0);
  REQUIRE(info.name == std::string{"C0"});
}
