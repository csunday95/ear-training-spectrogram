#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glad/gl.h>
#include <cmath>
#include <memory>
#include <vector>

#include "fft_config.hpp"
#include "gl_test_fixture.hpp"
#include "shader.hpp"

namespace {

constexpr uint32_t kTestFFTN = 4096;
constexpr float kSampleRate = 44100.0f;
const float kPi = std::acos(-1.0f);

struct FFTPipeline {
  audio::FFTConfig cfg;
  GLuint fft_prog = 0;
  GLuint magnitude_prog = 0;
  GLuint audio_ssbo = 0;
  GLuint complex_ssbo = 0;
  GLuint magnitude_ssbo = 0;
  float* magnitude_ptr = nullptr;
  bool ok = false;

  FFTPipeline() {
    GLint max_shared = 0;
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &max_shared);

    auto maybe = audio::make_fft_config(kTestFFTN, static_cast<uint32_t>(max_shared));
    if (!maybe) {
      return;
    }
    cfg = *maybe;

    fft_prog = load_compute_program("shaders/compute/fft.comp", cfg.preamble());
    magnitude_prog = load_compute_program("shaders/compute/magnitude.comp", cfg.preamble());
    if (!fft_prog || !magnitude_prog) {
      return;
    }

    // audio_ssbo: float[FFT_N] — CPU-prepared windowed + bit-reversed reals
    glCreateBuffers(1, &audio_ssbo);
    glNamedBufferStorage(audio_ssbo, kTestFFTN * sizeof(float), nullptr, GL_DYNAMIC_STORAGE_BIT);

    // complex_ssbo: vec2[FFT_N] — FFT output in natural frequency order
    glCreateBuffers(1, &complex_ssbo);
    glNamedBufferStorage(complex_ssbo, kTestFFTN * 2 * sizeof(float), nullptr, 0);

    // magnitude_ssbo: float[FFT_N/2+1] — dB magnitude, persistent-mapped for readback
    const GLbitfield map_flags =
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glCreateBuffers(1, &magnitude_ssbo);
    glNamedBufferStorage(magnitude_ssbo, (kTestFFTN / 2 + 1) * sizeof(float), nullptr, map_flags);
    magnitude_ptr = static_cast<float*>(
        glMapNamedBufferRange(magnitude_ssbo, 0, (kTestFFTN / 2 + 1) * sizeof(float), map_flags));
    if (!magnitude_ptr) {
      return;
    }

    ok = true;
  }

  ~FFTPipeline() {
    if (magnitude_ptr) {
      glUnmapNamedBuffer(magnitude_ssbo);
    }
    glDeleteBuffers(1, &audio_ssbo);
    glDeleteBuffers(1, &complex_ssbo);
    glDeleteBuffers(1, &magnitude_ssbo);
    glDeleteProgram(fft_prog);
    glDeleteProgram(magnitude_prog);
  }

  // Apply Hann window + bit-reversal on CPU, upload, then dispatch FFT + magnitude.
  void dispatch(const std::vector<float>& audio) {
    std::vector<float> br(kTestFFTN);
    for (uint32_t n = 0; n < kTestFFTN; ++n) {
      const float w = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(n) / static_cast<float>(kTestFFTN - 1)));
      br[audio::bit_reverse(n, cfg.log2_n)] = audio[n] * w;
    }
    glNamedBufferSubData(audio_ssbo, 0, br.size() * sizeof(float), br.data());

    glUseProgram(fft_prog);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, audio_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, complex_ssbo);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(magnitude_prog);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, complex_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, magnitude_ssbo);
    glUniform1f(glGetUniformLocation(magnitude_prog, "mag_scale"), 1.0f);
    glDispatchCompute((kTestFFTN / 2 + 1 + cfg.local_size - 1) / cfg.local_size, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

    glFinish();
  }

  float magnitude(size_t bin) const { return magnitude_ptr[bin]; }

  size_t peak_bin() const {
    const size_t n_bins = kTestFFTN / 2 + 1;
    return static_cast<size_t>(
        std::max_element(magnitude_ptr, magnitude_ptr + n_bins) - magnitude_ptr);
  }
};

std::vector<float> sine_signal(float freq_hz, float amplitude = 1.0f) {
  std::vector<float> out(kTestFFTN);
  for (uint32_t n = 0; n < kTestFFTN; ++n) {
    out[n] = amplitude * std::sin(2.0f * kPi * freq_hz * float(n) / kSampleRate);
  }
  return out;
}

float bin_freq(size_t k) {
  return kSampleRate / float(kTestFFTN) * float(k);
}

}  // namespace

TEST_CASE("FFT pipeline: trivial inputs", "[fft][gl]") {
  REQUIRE_GL();
  auto p = std::make_unique<FFTPipeline>();
  REQUIRE(p->ok);

  SECTION("zero input → all bins near noise floor") {
    p->dispatch(std::vector<float>(kTestFFTN, 0.0f));
    for (size_t i = 0; i < kTestFFTN / 2 + 1; ++i) {
      REQUIRE(p->magnitude(i) < -140.0f);
    }
  }

  SECTION("DC offset → energy concentrated in bin 0") {
    p->dispatch(std::vector<float>(kTestFFTN, 1.0f));
    const float dc = p->magnitude(0);
    REQUIRE(dc > -30.0f);
    // Hann main lobe is 3 bins wide: bins ±1 are part of the main lobe (~−6 dB),
    // not sidelobes. Skip them; all other bins should be below −31 dB (Hann spec).
    for (size_t i = 2; i < kTestFFTN / 2 + 1; ++i) {
      REQUIRE(p->magnitude(i) < dc - 31.0f);
    }
  }

  SECTION("Nyquist signal → energy at bin N/2") {
    std::vector<float> nyq(kTestFFTN);
    for (size_t n = 0; n < kTestFFTN; ++n) {
      nyq[n] = (n % 2 == 0) ? 1.0f : -1.0f;
    }
    p->dispatch(nyq);
    const float nyq_mag = p->magnitude(kTestFFTN / 2);
    REQUIRE(nyq_mag > -30.0f);
    // Skip adjacent bins (main lobe ±1); all others below −31 dB.
    for (size_t i = 0; i < kTestFFTN / 2 - 1; ++i) {
      REQUIRE(p->magnitude(i) < nyq_mag - 31.0f);
    }
  }
}

TEST_CASE("FFT pipeline: single-tone localisation", "[fft][gl]") {
  REQUIRE_GL();
  auto p = std::make_unique<FFTPipeline>();
  REQUIRE(p->ok);

  SECTION("peak bin matches signal frequency") {
    constexpr size_t kTargetBin = 64;
    p->dispatch(sine_signal(bin_freq(kTargetBin)));
    REQUIRE(p->peak_bin() == kTargetBin);
  }

  SECTION("Hann window suppresses sidelobes ≥ 40 dB") {
    constexpr size_t kTargetBin = 64;
    p->dispatch(sine_signal(bin_freq(kTargetBin)));
    const float peak = p->magnitude(kTargetBin);
    REQUIRE(peak > -30.0f);
    // Hann main lobe spans ±1 bins (~−6 dB); skip them. True sidelobes (≥±2)
    // must be ≥ 40 dB below the peak per the Hann window spec.
    for (size_t i = 0; i < kTestFFTN / 2 + 1; ++i) {
      if (i >= kTargetBin - 1 && i <= kTargetBin + 1) {
        continue;
      }
      REQUIRE(p->magnitude(i) < peak - 40.0f);
    }
  }

  SECTION("centred impulse → all bins populated") {
    std::vector<float> impulse(kTestFFTN, 0.0f);
    impulse[kTestFFTN / 2] = 1.0f;
    p->dispatch(impulse);
    for (size_t i = 0; i < kTestFFTN / 2 + 1; ++i) {
      REQUIRE(p->magnitude(i) > -100.0f);
    }
  }
}

TEST_CASE("FFT pipeline: multi-tone", "[fft][gl]") {
  REQUIRE_GL();
  auto p = std::make_unique<FFTPipeline>();
  REQUIRE(p->ok);

  SECTION("two sines → peaks at correct bins with ~6 dB amplitude ratio") {
    constexpr size_t kBin1 = 32;
    constexpr size_t kBin2 = 128;
    auto sig = sine_signal(bin_freq(kBin1));
    const auto s2 = sine_signal(bin_freq(kBin2), 0.5f);
    for (size_t n = 0; n < kTestFFTN; ++n) {
      sig[n] += s2[n];
    }
    p->dispatch(sig);
    const float m1 = p->magnitude(kBin1);
    const float m2 = p->magnitude(kBin2);
    REQUIRE(m1 > -30.0f);
    REQUIRE(m2 > -30.0f);
    CHECK_THAT(m1 - m2, Catch::Matchers::WithinAbs(6.0f, 3.0f));
  }
}
