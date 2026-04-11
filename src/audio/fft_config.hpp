#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace audio {

/**
 * FFT configuration with compile-time preamble generation.
 * Validates power-of-2 FFT size and queries GPU shared memory constraints.
 */
struct FFTConfig {
  uint32_t fft_n;         ///< FFT size (power-of-2)
  uint32_t log2_n;        ///< log2(fft_n)
  uint32_t local_size;    ///< Compute shader local group size

  /**
   * Generate GLSL #define preamble for shader injection.
   * Inserts after #version line in compute shaders.
   * Format:
   *   #define FFT_N 4096
   *   #define FFT_LOG2_N 12
   *   #define FFT_LOCAL_SIZE 256
   */
  [[nodiscard]] std::string preamble() const {
    return "#define FFT_N " + std::to_string(fft_n) + "\n"
           "#define FFT_LOG2_N " + std::to_string(log2_n) + "\n"
           "#define FFT_LOCAL_SIZE " + std::to_string(local_size) + "\n";
  }
};

/**
 * Create FFTConfig with power-of-2 validation and GPU memory checks.
 *
 * @param fft_n Desired FFT size (must be power-of-2)
 * @param max_shared_memory_bytes Max compute shader shared memory (from
 *                                 GL_MAX_COMPUTE_SHARED_MEMORY_SIZE)
 * @return FFTConfig if validation succeeds, std::nullopt if fft_n is not
 *         power-of-2 or shared memory insufficient
 *
 * Heuristic:
 * - Shared memory required: fft_n × 8 bytes (one vec2 array, in-place butterfly)
 * - local_size = min(256, fft_n) to fit within typical GPU limits
 * - Validates shared memory requirement against GPU capability
 */
[[nodiscard]] std::optional<FFTConfig> make_fft_config(
    uint32_t fft_n, uint32_t max_shared_memory_bytes);

// Reverse the lower log2_n bits of n.
// Used by callers to prepare bit-reversed input for the DIT FFT shader.
[[nodiscard]] inline uint32_t bit_reverse(uint32_t n, uint32_t log2_n) {
  uint32_t r = 0;
  for (uint32_t i = 0; i < log2_n; ++i) {
    r = (r << 1) | (n & 1u);
    n >>= 1;
  }

  return r;
}

}  // namespace audio
