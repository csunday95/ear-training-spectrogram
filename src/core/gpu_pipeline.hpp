#pragma once

#include <glad/gl.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "fft_config.hpp"

namespace core {

// Display and audio parameters for GpuPipeline construction.
// All fractions are proportions of the framebuffer height; they must sum to ≤ 1.
struct GpuPipelineConfig {
  // dB display range — with mag_scale = 2/fft_n, a full-scale sine hits ~0 dB.
  float    db_min;
  float    db_max;
  // Fraction of the spectrum panel height that db_max maps to (controls headroom).
  float    spectrum_scale;
  // Panel height fractions. Waterfall takes the remainder: 1 - spectrum - waveform - tuner.
  float    spectrum_fraction;
  float    waveform_fraction;
  float    tuner_fraction;
  // Log-frequency display bounds in Hz.
  float    log_freq_min;
  float    log_freq_max;
  // EMA weight for the per-bin magnitude smoothing pass (0 < alpha ≤ 1).
  float    smooth_alpha;
  // dB subtracted from the max-hold envelope per FFT frame.
  float    max_hold_decay_db;
  // Audio capture sample rate in Hz (used to compute the Nyquist frequency).
  uint32_t sample_rate;
};

/**
 * GPU compute + render pipeline for the spectrogram display.
 *
 * Owns all GPU resources: SSBOs, programs, waterfall texture.
 * Call dispatch() each frame to run the FFT compute chain, then render()
 * to draw the waterfall and spectrum before the ImGui pass.
 */
class GpuPipeline {
public:
  GpuPipeline(const audio::FFTConfig& fft_cfg, int initial_fb_w,
              const GpuPipelineConfig& cfg);
  ~GpuPipeline();

  GpuPipeline(const GpuPipeline&)            = delete;
  GpuPipeline& operator=(const GpuPipeline&) = delete;

  // False if any shader failed to compile/link at construction time.
  [[nodiscard]] bool ok() const { return ok_; }

  // Apply Hann window + bit-reversal on CPU, upload, then dispatch the full
  // GPU compute chain (FFT → magnitude → max-hold → waterfall update).
  // audio must contain at least cfg.fft_n samples.
  void dispatch(std::span<const float> audio);

  // Render waterfall (top), tuner band gap (middle), and spectrum (bottom).
  // Panel heights are determined by the spectrum_fraction and tuner_fraction
  // supplied at construction; the waterfall takes the remainder.
  // Call after dispatch(), before imgui begin_frame().
  // On the first call, or if fb_w has changed since the last call, the
  // waterfall texture is (re)created to match fb_w — this resets history.
  void render(int fb_w, int fb_h);

  // CPU pointer into the persistently-mapped magnitude SSBO (dB values).
  // Blocks (via fence) until the most recent GPU dispatch has completed.
  // Name makes the synchronisation cost explicit. (Phase 3 pitch detection.)
  [[nodiscard]] const float* sync_get_mag_data() const;

  // CPU pointer into the persistently-mapped linear-magnitude SSBO.
  // Returns normalized linear amplitudes (mag * mag_scale) written by magnitude.comp
  // before the dB conversion — avoids a dB round-trip for HPS computation.
  // The fence from sync_get_mag_data() covers this SSBO too; call sync_get_mag_data()
  // first in the same frame before calling this.
  [[nodiscard]] const float* sync_get_linear_mag_data() const;

  // Accessors for layout fractions set at construction — used by overlay widgets
  // (e.g. TunerWidget) to position themselves without duplicating values.
  [[nodiscard]] float spectrum_fraction()  const { return spectrum_fraction_; }
  [[nodiscard]] float waveform_fraction()  const { return waveform_fraction_; }
  [[nodiscard]] float tuner_fraction()     const { return tuner_fraction_; }

private:
  // kDbInit: value used to pre-fill max-hold SSBO and waterfall texture.
  // Must be below any realistic db_min so the display starts fully black.
  static constexpr float kDbInit = -120.0f;

  // --- Static helpers used in the constructor init list ---

  // Allocate an immutable SSBO and return its name. Returns 0 on failure.
  static GLuint make_ssbo(GLsizeiptr size, GLbitfield flags);

  // Load a compute program with preamble injection. Thin wrappers so that
  // calling convention matches the init list pattern.
  static GLuint make_compute(const std::filesystem::path& path,
                              const std::string& preamble);
  static GLuint make_render(const std::filesystem::path& vert,
                             const std::filesystem::path& frag);

  // Allocate an empty VAO (required by GL 4.5 core for attribute-less draws).
  static GLuint make_vao();

  // Map the magnitude SSBO for persistent CPU readback. Returns nullptr on failure.
  static const float* map_magnitude(GLuint ssbo, GLsizeiptr size);

  // Recreate the waterfall texture at the given width. Resets write_col_ and
  // clears history. Called from render() on first use or after window resize.
  void reinit_waterfall(int width);

  // --- Members ---

  audio::FFTConfig cfg_;
  uint32_t         fft_bins_;   // cfg_.fft_n / 2 + 1  (must be declared before SSBOs)
  float            db_min_;
  float            db_max_;
  float            mag_scale_;          // 2 / fft_n — normalises FFT output to 0 dB full-scale
  float            spectrum_scale_;     // fraction of panel height db_max maps to
  float            spectrum_fraction_;   // fraction of fb_h for the spectrum panel
  float            waveform_fraction_;  // fraction of fb_h for the waveform panel
  float            tuner_fraction_;     // fraction of fb_h for the tuner band
  float            smooth_alpha_;        // EMA weight for the magnitude smoothing pass
  float            max_hold_decay_db_;  // dB subtracted from max-hold envelope per FFT frame
  float            f_min_;             // minimum display frequency (Hz)
  float            log_freq_range_;    // log2(f_max / f_min), precomputed
  float            nyquist_;           // sample_rate / 2.0
  bool             ok_{false};

  // Per-frame windowed + bit-reversed scratch (avoids per-frame allocation).
  std::vector<float> windowed_;

  // SSBOs — handles are fixed after construction.
  const GLuint audio_ssbo_;
  const GLuint complex_ssbo_;
  const GLuint magnitude_ssbo_;      // dB magnitudes (binding 1 in magnitude.comp)
  const GLuint linear_mag_ssbo_;     // scaled linear amplitudes (binding 2 in magnitude.comp)
  const GLuint max_hold_ssbo_;
  const GLuint smooth_ssbo_;   // GPU-only EMA state for per-bin magnitude smoothing

  // Persistent CPU mappings (set in constructor body).
  const float* magnitude_ptr_{nullptr};
  const float* linear_mag_ptr_{nullptr};
  GLsync       fence_{nullptr};

  // Compute programs — fixed after construction.
  const GLuint prog_fft_;
  const GLuint prog_magnitude_;
  const GLuint prog_smooth_magnitude_;  // EMA smoothing pass
  const GLuint prog_max_hold_;
  const GLuint prog_waterfall_update_;

  // Render programs — fixed after construction.
  const GLuint prog_waterfall_render_;
  const GLuint prog_spectrum_render_;

  // Waterfall state — texture is recreated on resize, so not const.
  GLuint   waterfall_tex_{0};
  uint32_t waterfall_width_{0};  // 0 = texture not yet created

  // Attribute-less draw VAO — fixed after construction.
  const GLuint vao_;

  uint32_t write_col_{0};  // next column to write (= oldest surviving column)
};

}  // namespace core
