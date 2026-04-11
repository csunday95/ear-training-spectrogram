#include "fft_config.hpp"

#include <bit>

namespace audio {

std::optional<FFTConfig> make_fft_config(uint32_t fft_n,
                                         uint32_t max_shared_memory_bytes) {
  // Validate power-of-2
  if (fft_n == 0 || !std::has_single_bit(fft_n)) {
    return std::nullopt;
  }

  // Calculate log2(fft_n)
  const auto log2_n = static_cast<uint32_t>(std::countr_zero(fft_n));

  // Compute shared memory requirement: fft_n × 8 bytes (one vec2 array, in-place butterfly)
  const uint32_t required_shared_memory = fft_n * 8;
  if (required_shared_memory > max_shared_memory_bytes) {
    return std::nullopt;
  }

  // Local group size: typically 256, but cap to fft_n if smaller
  const uint32_t local_size = std::min(256u, fft_n);

  return FFTConfig{
      .fft_n = fft_n,
      .log2_n = log2_n,
      .local_size = local_size,
  };
}

}  // namespace audio
