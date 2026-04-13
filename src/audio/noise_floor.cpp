#include "noise_floor.hpp"

#include <algorithm>
#include <cmath>

namespace audio {

NoiseFloor::NoiseFloor(uint32_t n_bins, uint32_t estimation_frames)
    : n_bins_{n_bins}
    , estimation_frames_{estimation_frames}
    , accum_(n_bins, 0.0)
    , floor_db_(n_bins, 0.0f) {}

bool NoiseFloor::update(std::span<const float> mag_db) {
  if (ready_) {
    return false;
  }
  const uint32_t bins = std::min(static_cast<uint32_t>(mag_db.size()), n_bins_);
  for (uint32_t i = 0; i < bins; ++i) {
    accum_[i] += std::pow(10.0, static_cast<double>(mag_db[i]) / 10.0);
  }
  ++frames_collected_;
  if (frames_collected_ >= estimation_frames_) {
    const double n = static_cast<double>(estimation_frames_);
    for (uint32_t i = 0; i < n_bins_; ++i) {
      floor_db_[i] = static_cast<float>(10.0 * std::log10(accum_[i] / n));
    }
    ready_ = true;
    return true;
  }
  return false;
}

void NoiseFloor::subtract(std::span<const float> mag_db, std::span<float> out_db,
                          float margin_db, float db_floor) const {
  const uint32_t bins = std::min({static_cast<uint32_t>(mag_db.size()),
                                  static_cast<uint32_t>(out_db.size()),
                                  n_bins_});
  for (uint32_t i = 0; i < bins; ++i) {
    const float p_signal = std::pow(10.0f, mag_db[i] / 10.0f);
    const float p_noise  = std::pow(10.0f, (floor_db_[i] + margin_db) / 10.0f);
    const float p_clean  = std::max(p_signal - p_noise, 1e-10f);
    out_db[i] = std::max(10.0f * std::log10(p_clean), db_floor);
  }
}

void NoiseFloor::reset() {
  std::fill(accum_.begin(),    accum_.end(),    0.0);
  std::fill(floor_db_.begin(), floor_db_.end(), 0.0f);
  frames_collected_ = 0;
  ready_            = false;
}

}  // namespace audio
