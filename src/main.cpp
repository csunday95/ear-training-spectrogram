#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <CLI/CLI.hpp>

#include "app_config.hpp"
#include "audio_capture.hpp"
#include "fft_config.hpp"
#include "gl_init.hpp"
#include "gpu_pipeline.hpp"
#include "log.hpp"
#include "noise_floor.hpp"
#include "pitch_detect.hpp"
#include "pitch_smoother.hpp"
#include "ui/imgui_renderer.hpp"
#include "ui/spectrum_axis_widget.hpp"
#include "ui/tuner_widget.hpp"
#include "ui/waveform_widget.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Must match the sample rate passed to AudioCapture.
constexpr uint32_t kSampleRate = 44100u;

// ---------------------------------------------------------------------------
// Startup CLI args (affect window/GL/FFT initialisation)
// ---------------------------------------------------------------------------

struct StartupArgs {
  uint32_t    fft_n       = 4096;
  int         width       = 1280;
  int         height      = 720;
  std::string config_path = "ear_training.json";
};

static StartupArgs parse_cli(int argc, char** argv) {
  StartupArgs args;
  CLI::App app{"ear_training"};
  app.add_option("--fft-n",  args.fft_n,       "FFT size (must be power of 2)")->capture_default_str();
  app.add_option("--width",  args.width,        "Window width")->capture_default_str();
  app.add_option("--height", args.height,       "Window height")->capture_default_str();
  app.add_option("--config", args.config_path,  "Path to JSON config file")->capture_default_str();
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    std::exit(app.exit(e));
  }
  return args;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  const auto args = parse_cli(argc, argv);
  const auto cfg  = core::load_app_config(args.config_path);

  // --- Window + GL ---
  // GlfwGuard is declared first so its destructor (glfwTerminate) runs last,
  // after pipeline and imgui have already been destroyed.
  GlfwGuard glfw{init_gl_window(args.width, args.height, "ear_training")};
  if (!glfw.window) {
    return 1;
  }
  GLFWwindow* const window = glfw.window;

  // --- FFT config ---
  GLint max_shared_mem = 0;
  glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &max_shared_mem);

  const auto fft_cfg = audio::make_fft_config(
      args.fft_n, static_cast<uint32_t>(max_shared_mem));
  if (!fft_cfg) {
    LOG_ERROR("Invalid FFT size {} (not power-of-2 or insufficient GPU shared memory)",
              args.fft_n);
    return 1;
  }
  LOG_INFO("FFT: N={}, log2_n={}, local_size={}", fft_cfg->fft_n, fft_cfg->log2_n,
           fft_cfg->local_size);

  // --- GPU pipeline ---
  int fb_w = 0, fb_h = 0;
  glfwGetFramebufferSize(window, &fb_w, &fb_h);
  const auto& dc = cfg.display;
  core::GpuPipeline pipeline{*fft_cfg, fb_w,
                             dc.db_min.value, dc.db_max.value,
                             dc.spectrum_scale.value, dc.spectrum_fraction.value,
                             dc.tuner_fraction.value,
                             dc.log_freq_min.value, dc.log_freq_max.value,
                             dc.smooth_alpha.value, dc.max_hold_decay_db.value,
                             kSampleRate};
  if (!pipeline.ok()) {
    LOG_ERROR("GPU pipeline failed to initialise — check shader compile errors above");
    return 1;
  }

  // --- UI ---
  ui::ImGuiRenderer  imgui{window, "#version 450"};
  const auto& wc = cfg.waveform_overlay;
  ui::WaveformWidget waveform_widget{wc.width.value, wc.height.value, wc.margin.value};
  ui::TunerWidget    tuner_widget{dc.spectrum_fraction.value, dc.tuner_fraction.value};
  ui::SpectrumAxisWidget axis_widget{dc.log_freq_min.value, dc.log_freq_max.value,
                                     dc.db_min.value, dc.db_max.value,
                                     dc.spectrum_scale.value, dc.spectrum_fraction.value};
  audio::PitchSmoother smoother;

  // --- Audio ---
  audio::AudioCapture capture;
  if (!capture.start()) {
    LOG_ERROR("Failed to start audio capture");
    // Continue without audio — allows visual testing without mic.
  }

  const uint32_t fft_n    = fft_cfg->fft_n;
  const uint32_t hop_size = fft_n / 2u;
  const uint32_t fft_bins = fft_n / 2u + 1u;

  audio::NoiseFloor noise_floor{
      fft_bins,
      cfg.pitch_detection.noise_estimation_frames.value};

  // Sliding FFT window: always fft_n samples, advanced by hop_size each dispatch.
  std::vector<float> frame_buf(fft_n, 0.0f);
  // Intermediate accumulator: ring buffer samples not yet consumed into frame_buf.
  std::vector<float> accum_buf;
  accum_buf.reserve(fft_n);
  // Scratch buffer for noise-subtracted magnitude (pitch detection only; display unaffected).
  std::vector<float> denoised_mag(fft_bins, 0.0f);

  // Precompute log range for the peak x-norm computation (matches spectrum.vert).
  const float log_freq_range = std::log2(dc.log_freq_max.value / dc.log_freq_min.value);

  // Pitch and display state — persists across frames so the tuner holds its last
  // committed value during frames when no new hop is dispatched.
  std::optional<audio::DetectionResult> display_pitch;
  float smoothed_cents     = 0.0f;
  float spectrum_peak_x    = 0.0f;  // [0, 1], aligned with log-frequency axis

  glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

  // --- Main loop ---
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // --- Read audio ---
    // Drain all available ring-buffer samples into accum_buf each frame.
    // When accum_buf accumulates hop_size samples, advance frame_buf by one hop.
    {
      const uint32_t avail     = capture.ring().available();
      const size_t   prev_size = accum_buf.size();
      accum_buf.resize(prev_size + avail);
      capture.ring().read(accum_buf.data() + prev_size, avail);
    }

    bool dispatched = false;
    while (accum_buf.size() >= hop_size) {
      // Shift frame_buf left by hop_size, then fill the right half from accum_buf.
      std::copy(frame_buf.begin() + static_cast<ptrdiff_t>(hop_size),
                frame_buf.end(),
                frame_buf.begin());
      std::copy(accum_buf.begin(),
                accum_buf.begin() + static_cast<ptrdiff_t>(hop_size),
                frame_buf.end() - static_cast<ptrdiff_t>(hop_size));
      accum_buf.erase(accum_buf.begin(),
                      accum_buf.begin() + static_cast<ptrdiff_t>(hop_size));

      // --- GPU compute ---
      pipeline.dispatch(frame_buf);
      dispatched = true;
    }

    // --- Pitch detection (only when new FFT data is available) ---
    if (dispatched) {
      const std::span<const float> raw_mag{pipeline.sync_get_mag_data(), fft_bins};

      // Feed the noise estimator; once ready, subtract the estimated floor.
      noise_floor.update(raw_mag);
      std::span<const float> mag = raw_mag;
      if (noise_floor.ready()) {
        noise_floor.subtract(raw_mag, denoised_mag,
                             cfg.pitch_detection.noise_floor_margin_db.value,
                             cfg.pitch_detection.min_db.value);
        mag = denoised_mag;
      }

      auto detection = audio::detect_peaks(
          mag, fft_n, kSampleRate,
          cfg.pitch_detection.min_db.value,
          cfg.pitch_detection.max_hwhm_bins.value,
          cfg.pitch_detection.max_peaks.value);

      const std::optional<audio::DetectionResult> raw_pitch =
          detection.peaks.empty() ? std::nullopt
                                   : std::optional<audio::DetectionResult>{detection};

      const auto& tc             = cfg.tuner_smoother;
      const std::optional<float> cents =
          smoother.update(raw_pitch, tc.ema_alpha.value, tc.stability_frames.value,
                          tc.gate_cents.value);

      if (cents.has_value()) {
        display_pitch  = std::move(detection);
        smoothed_cents = *cents;

        // Map the dominant peak frequency to a log-space x-norm [0, 1].
        if (!display_pitch->peaks.empty()) {
          const float f = display_pitch->peaks[0].frequency;
          spectrum_peak_x =
              std::clamp(std::log2(f / dc.log_freq_min.value) / log_freq_range, 0.0f, 1.0f);
        }
      } else {
        display_pitch = std::nullopt;
      }
    }

    // --- Render ---
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glClear(GL_COLOR_BUFFER_BIT);

    pipeline.render(fb_w, fb_h);

    // --- UI overlay ---
    imgui.begin_frame();

    // 'R' — restart noise floor estimation from scratch.
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      noise_floor.reset();
      LOG_INFO("Noise floor reset — re-estimating over {} frames",
               cfg.pitch_detection.noise_estimation_frames.value);
    }

    ui::FrameData frame{
        .waveform              = frame_buf,
        .framebuffer_width     = fb_w,
        .framebuffer_height    = fb_h,
        .pitch                 = display_pitch,
        .smoothed_cents        = smoothed_cents,
        .spectrum_peak_x_norm  = spectrum_peak_x,
    };
    waveform_widget.draw(frame);
    tuner_widget.draw(frame);
    axis_widget.draw(frame);

    imgui.end_frame();

    glfwSwapBuffers(window);
  }

  // --- Cleanup ---
  capture.stop();
  // All other resources cleaned up by RAII destructors (pipeline, imgui, glfw).
  return 0;
}
