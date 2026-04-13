#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace audio {

/**
 * Estimates a per-bin noise floor from the first `estimation_frames` of
 * magnitude data, then provides linear-domain spectral subtraction.
 *
 * Accumulation uses linear power (10^(dB/10)) so the average is in power
 * space rather than dB space. subtract() converts back to dB after subtracting.
 *
 * Intended use: call update() each FFT dispatch frame. Once ready(), call
 * subtract() to produce a denoised magnitude span before peak detection.
 * The display pipeline is unaffected — subtraction is CPU-side only.
 */
class NoiseFloor {
public:
  NoiseFloor(uint32_t n_bins, uint32_t estimation_frames);

  // Feed one frame of dB magnitude. Returns true the frame estimation completes.
  bool update(std::span<const float> mag_db);

  // True once estimation_frames have been accumulated.
  [[nodiscard]] bool ready() const { return ready_; }

  // How many frames have been collected so far (resets to 0 after reset()).
  [[nodiscard]] uint32_t frames_collected() const { return frames_collected_; }

  // How many frames are needed before ready() becomes true.
  [[nodiscard]] uint32_t estimation_frames() const { return estimation_frames_; }

  // Clear the accumulator and restart estimation from scratch.
  void reset();

  // Write noise-subtracted dB values into out_db.
  // margin_db is added above the measured floor before subtracting (prevents
  // over-subtraction on signals just barely above the noise level).
  // Results are clamped to db_floor.
  // Must only be called when ready() is true.
  void subtract(std::span<const float> mag_db, std::span<float> out_db,
                float margin_db, float db_floor) const;

private:
  uint32_t            n_bins_;
  uint32_t            estimation_frames_;
  uint32_t            frames_collected_{0};
  bool                ready_{false};
  std::vector<double> accum_;     // sum of linear powers per bin during estimation
  std::vector<float>  floor_db_;  // per-bin noise floor in dB, set once ready
};

}  // namespace audio
