#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace audio {

// Note name table — constexpr array so alternate mappings (flats, solfège)
// can be substituted by swapping a single declaration.
constexpr std::array<const char*, 12> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct NoteInfo {
  // MIDI note number 0–127; A4 = 69. Unsigned: MIDI notes are never negative.
  uint8_t midi_note;
  // Octave number; C4 = octave 4, A4 = octave 4.
  // Stored separately so callers can reason about octave membership without
  // string-parsing the name field.
  int8_t octave;
  // Full note name with octave suffix, e.g. "A4", "C#3".
  // std::string because combining kNoteNames[midi%12] with the octave number
  // requires a heap buffer — a const char* into a local buffer would dangle.
  std::string name;
  // Exact equal-temperament frequency for this note.
  float frequency;
  // Deviation in cents: negative = flat, positive = sharp.
  float cents_offset;
};

// Returns the NoteInfo for the equal-temperament note closest to hz.
[[nodiscard]] inline NoteInfo freq_to_note(float hz) {
  const float raw_midi = 12.0f * std::log2(hz / 440.0f) + 69.0f;
  const long  rounded  = std::lround(std::clamp(raw_midi, 0.0f, 127.0f));
  const auto  midi     = static_cast<uint8_t>(rounded);
  const auto  octave   = static_cast<int8_t>(static_cast<int>(midi) / 12 - 1);

  const float ref_freq =
      440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
  const float cents = 1200.0f * std::log2(hz / ref_freq);

  return NoteInfo{
      .midi_note    = midi,
      .octave       = octave,
      .name         = std::string{kNoteNames[static_cast<std::size_t>(midi) % 12u]} +
                      std::to_string(static_cast<int>(octave)),
      .frequency    = ref_freq,
      .cents_offset = cents,
  };
}

}  // namespace audio
