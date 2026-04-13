#include <glad/gl.h>

#include "gpu_pipeline.hpp"

#include "fft_config.hpp"
#include "log.hpp"
#include "shader.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace core {

// ---------------------------------------------------------------------------
// Static helpers (called from the constructor init list)
// ---------------------------------------------------------------------------

GLuint GpuPipeline::make_ssbo(GLsizeiptr size, GLbitfield flags) {
  GLuint buf{0};
  glCreateBuffers(1, &buf);
  glNamedBufferStorage(buf, size, nullptr, flags);
  return buf;
}

GLuint GpuPipeline::make_compute(const std::filesystem::path& path,
                                  const std::string& preamble) {
  return load_compute_program(path, preamble);
}

GLuint GpuPipeline::make_render(const std::filesystem::path& vert,
                                 const std::filesystem::path& frag) {
  return load_render_program(vert, frag);
}

GLuint GpuPipeline::make_vao() {
  GLuint v{0};
  glCreateVertexArrays(1, &v);
  return v;
}

const float* GpuPipeline::map_magnitude(GLuint ssbo, GLsizeiptr size) {
  return static_cast<const float*>(
      glMapNamedBufferRange(ssbo, 0, size,
          GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT));
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GpuPipeline::GpuPipeline(const audio::FFTConfig& cfg, int initial_fb_w,
                          float db_min, float db_max, float spectrum_scale,
                          float spectrum_fraction, float tuner_fraction,
                          float log_freq_min, float log_freq_max, float smooth_alpha,
                          float max_hold_decay_db, uint32_t sample_rate)
    : cfg_{cfg}
    , fft_bins_{cfg.fft_n / 2u + 1u}
    , db_min_{db_min}
    , db_max_{db_max}
    , mag_scale_{2.0f / static_cast<float>(cfg.fft_n)}
    , spectrum_scale_{spectrum_scale}
    , spectrum_fraction_{spectrum_fraction}
    , tuner_fraction_{tuner_fraction}
    , smooth_alpha_{smooth_alpha}
    , max_hold_decay_db_{max_hold_decay_db}
    , f_min_{log_freq_min}
    , log_freq_range_{std::log2(log_freq_max / log_freq_min)}
    , nyquist_{static_cast<float>(sample_rate) * 0.5f}
    , windowed_(cfg.fft_n, 0.0f)
    , audio_ssbo_{make_ssbo(
          static_cast<GLsizeiptr>(cfg.fft_n * sizeof(float)),
          GL_DYNAMIC_STORAGE_BIT)}
    , complex_ssbo_{make_ssbo(
          static_cast<GLsizeiptr>(cfg.fft_n * sizeof(float) * 2u), 0)}
    , magnitude_ssbo_{make_ssbo(
          static_cast<GLsizeiptr>(fft_bins_ * sizeof(float)),
          GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)}
    , max_hold_ssbo_{make_ssbo(
          static_cast<GLsizeiptr>(fft_bins_ * sizeof(float)), 0)}
    , smooth_ssbo_{make_ssbo(
          static_cast<GLsizeiptr>(fft_bins_ * sizeof(float)), 0)}
    , prog_fft_{make_compute("shaders/compute/fft.comp", cfg.preamble())}
    , prog_magnitude_{make_compute("shaders/compute/magnitude.comp", cfg.preamble())}
    , prog_smooth_magnitude_{make_compute("shaders/compute/smooth_magnitude.comp",
                                          cfg.preamble())}
    , prog_max_hold_{make_compute("shaders/compute/max_hold.comp", cfg.preamble())}
    , prog_waterfall_update_{make_compute("shaders/compute/waterfall_update.comp",
                                          cfg.preamble())}
    , prog_waterfall_render_{make_render(
          "shaders/render/waterfall.vert", "shaders/render/waterfall.frag")}
    , prog_spectrum_render_{make_render(
          "shaders/render/spectrum.vert", "shaders/render/spectrum.frag")}
    , vao_{make_vao()} {

  // Map magnitude SSBO for CPU readback (Phase 3 pitch detection).
  // The input audio span passed to dispatch() is never modified — bit-reversal
  // writes into the internal windowed_ scratch buffer only.
  magnitude_ptr_ = map_magnitude(magnitude_ssbo_,
      static_cast<GLsizeiptr>(fft_bins_ * sizeof(float)));
  if (!magnitude_ptr_) {
    LOG_ERROR("GpuPipeline: failed to map magnitude buffer");
    return;
  }

  // Initialise max-hold and smooth buffers to dB floor so the display starts silent.
  glClearNamedBufferData(max_hold_ssbo_, GL_R32F, GL_RED, GL_FLOAT, &kDbInit);
  glClearNamedBufferData(smooth_ssbo_,   GL_R32F, GL_RED, GL_FLOAT, &kDbInit);

  if (!prog_fft_ || !prog_magnitude_ || !prog_smooth_magnitude_ || !prog_max_hold_ ||
      !prog_waterfall_update_ || !prog_waterfall_render_ || !prog_spectrum_render_) {
    LOG_ERROR("GpuPipeline: one or more shader programs failed to load");
    return;
  }

  // Create the waterfall texture at the initial framebuffer width so dispatch()
  // can write to it immediately (dispatch() runs before the first render() call).
  reinit_waterfall(initial_fb_w);

  ok_ = true;
}

// ---------------------------------------------------------------------------
// Destruction
// ---------------------------------------------------------------------------

GpuPipeline::~GpuPipeline() {
  if (fence_) {
    glDeleteSync(fence_);
  }
  // glDeleteProgram(0) is a no-op per spec — safe even when loading failed.
  glDeleteProgram(prog_fft_);
  glDeleteProgram(prog_magnitude_);
  glDeleteProgram(prog_smooth_magnitude_);
  glDeleteProgram(prog_max_hold_);
  glDeleteProgram(prog_waterfall_update_);
  glDeleteProgram(prog_waterfall_render_);
  glDeleteProgram(prog_spectrum_render_);
  glDeleteTextures(1, &waterfall_tex_);
  const GLuint bufs[] = {audio_ssbo_, complex_ssbo_, magnitude_ssbo_, max_hold_ssbo_,
                         smooth_ssbo_};
  glDeleteBuffers(5, bufs);
  glDeleteVertexArrays(1, &vao_);
}

// ---------------------------------------------------------------------------
// Per-frame dispatch
// ---------------------------------------------------------------------------

void GpuPipeline::dispatch(std::span<const float> audio) {
  if (!ok_) {
    return;
  }

  const uint32_t n = std::min(static_cast<uint32_t>(audio.size()), cfg_.fft_n);
  constexpr float kPi = std::numbers::pi_v<float>;

  // CPU: Hann window + bit-reverse permutation into the internal scratch buffer.
  // The input span is read-only; waveform display in main.cpp is unaffected.
  for (uint32_t i = 0; i < n; ++i) {
    const float hann =
        0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) /
                                 static_cast<float>(cfg_.fft_n - 1u)));
    windowed_[audio::bit_reverse(i, cfg_.log2_n)] = audio[i] * hann;
  }
  // Zero-pad if the span is shorter than fft_n (not expected in normal operation).
  for (uint32_t i = n; i < cfg_.fft_n; ++i) {
    windowed_[audio::bit_reverse(i, cfg_.log2_n)] = 0.0f;
  }

  glNamedBufferSubData(audio_ssbo_, 0,
      static_cast<GLsizeiptr>(cfg_.fft_n * sizeof(float)), windowed_.data());

  const GLuint fft_dispatch = (fft_bins_ + 255u) / 256u;

  // fft.comp — single workgroup, in-place Cooley-Tukey DIT via shared memory.
  glUseProgram(prog_fft_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, audio_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, complex_ssbo_);
  glDispatchCompute(1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // magnitude.comp — complex → dB (location 0 = mag_scale).
  glUseProgram(prog_magnitude_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, complex_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, magnitude_ssbo_);
  glProgramUniform1f(prog_magnitude_, 0, mag_scale_);
  glDispatchCompute(fft_dispatch, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // smooth_magnitude.comp — EMA smoothing; output feeds the waterfall and spectrum display.
  // Pitch detection readback still reads magnitude_ssbo_ (unsmoothed).
  glUseProgram(prog_smooth_magnitude_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, magnitude_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, smooth_ssbo_);
  glProgramUniform1f(prog_smooth_magnitude_, 0, smooth_alpha_);
  glDispatchCompute(fft_dispatch, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // max_hold.comp — peak-hold with per-frame dB decay; reads smoothed magnitude.
  glUseProgram(prog_max_hold_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, smooth_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, max_hold_ssbo_);
  glProgramUniform1f(prog_max_hold_, 0, max_hold_decay_db_);
  glDispatchCompute(fft_dispatch, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // waterfall_update.comp — write smoothed magnitude column into the texture.
  glUseProgram(prog_waterfall_update_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, smooth_ssbo_);
  glBindImageTexture(0, waterfall_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
  glProgramUniform1ui(prog_waterfall_update_, 0, write_col_);
  glDispatchCompute(fft_dispatch, 1, 1);

  // Advance write pointer; write_col_ now points at the next (oldest) slot.
  write_col_ = (write_col_ + 1u) % waterfall_width_;

  // Single barrier covers SSBO reads by spectrum.vert and texture reads by waterfall.frag.
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                  GL_TEXTURE_FETCH_BARRIER_BIT);

  // Fence for Phase 3 CPU readback via magnitude_data().
  if (fence_) {
    glDeleteSync(fence_);
  }
  fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

// ---------------------------------------------------------------------------
// Waterfall texture management
// ---------------------------------------------------------------------------

void GpuPipeline::reinit_waterfall(int width) {
  glDeleteTextures(1, &waterfall_tex_);
  glCreateTextures(GL_TEXTURE_2D, 1, &waterfall_tex_);
  glTextureStorage2D(waterfall_tex_, 1, GL_R32F,
      static_cast<GLsizei>(width), static_cast<GLsizei>(fft_bins_));
  glTextureParameteri(waterfall_tex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(waterfall_tex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // GL_REPEAT in S so the circular-buffer U offset in waterfall.frag wraps cleanly.
  glTextureParameteri(waterfall_tex_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(waterfall_tex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glClearTexImage(waterfall_tex_, 0, GL_RED, GL_FLOAT, &kDbInit);

  waterfall_width_ = static_cast<uint32_t>(width);
  write_col_       = 0u;
}

// ---------------------------------------------------------------------------
// Per-frame render
// ---------------------------------------------------------------------------

void GpuPipeline::render(int fb_w, int fb_h) {
  if (!ok_) {
    return;
  }

  // (Re)create the waterfall texture if the framebuffer width changed or on first use.
  if (static_cast<int>(waterfall_width_) != fb_w) {
    reinit_waterfall(fb_w);
  }

  const int spectrum_h  = static_cast<int>(static_cast<float>(fb_h) * spectrum_fraction_);
  const int tuner_h     = static_cast<int>(static_cast<float>(fb_h) * tuner_fraction_);
  const int waterfall_h = fb_h - spectrum_h - tuner_h;
  const GLint fft_bins  = static_cast<GLint>(fft_bins_);

  glBindVertexArray(vao_);

  // --- Waterfall (top band) ---
  glViewport(0, spectrum_h + tuner_h, fb_w, waterfall_h);
  glUseProgram(prog_waterfall_render_);
  glBindTextureUnit(0, waterfall_tex_);
  // write_col_ is the oldest surviving column → maps to the left (oldest time) edge.
  glProgramUniform1ui(prog_waterfall_render_, 0, write_col_);
  glProgramUniform1ui(prog_waterfall_render_, 1, waterfall_width_);
  glProgramUniform1f(prog_waterfall_render_, 2, db_min_);
  glProgramUniform1f(prog_waterfall_render_, 3, db_max_);
  glProgramUniform1f(prog_waterfall_render_, 4, f_min_);
  glProgramUniform1f(prog_waterfall_render_, 5, log_freq_range_);
  glProgramUniform1f(prog_waterfall_render_, 6, nyquist_);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // --- Spectrum (bottom panel) ---
  glViewport(0, 0, fb_w, spectrum_h);
  glUseProgram(prog_spectrum_render_);
  glProgramUniform1i(prog_spectrum_render_, 0, fft_bins);
  glProgramUniform1f(prog_spectrum_render_, 1, db_min_);
  glProgramUniform1f(prog_spectrum_render_, 2, db_max_);
  glProgramUniform1f(prog_spectrum_render_, 4, spectrum_scale_);
  glProgramUniform1f(prog_spectrum_render_, 5, f_min_);
  glProgramUniform1f(prog_spectrum_render_, 6, log_freq_range_);
  glProgramUniform1f(prog_spectrum_render_, 7, nyquist_);

  // Smoothed magnitude line (bright green).
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, smooth_ssbo_);
  glProgramUniform4f(prog_spectrum_render_, 3, 0.2f, 0.9f, 0.2f, 1.0f);
  glDrawArrays(GL_LINE_STRIP, 0, fft_bins);

  // Max-hold envelope (dim yellow).
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, max_hold_ssbo_);
  glProgramUniform4f(prog_spectrum_render_, 3, 0.7f, 0.7f, 0.2f, 0.6f);
  glDrawArrays(GL_LINE_STRIP, 0, fft_bins);

  // Restore full viewport so ImGui renders correctly.
  glViewport(0, 0, fb_w, fb_h);
}

// ---------------------------------------------------------------------------
// CPU readback (Phase 3)
// ---------------------------------------------------------------------------

const float* GpuPipeline::sync_get_mag_data() const {
  if (fence_) {
    // 10ms timeout; in practice completes in well under one frame.
    glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, 10'000'000);
  }
  return magnitude_ptr_;
}

}  // namespace core
