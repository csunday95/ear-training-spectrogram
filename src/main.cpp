#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <CLI/CLI.hpp>

#include "app_config.hpp"
#include "audio_capture.hpp"
#include "fft_config.hpp"
#include "gl_init.hpp"
#include "gpu_pipeline.hpp"
#include "log.hpp"
#include "pitch_detect.hpp"
#include "pitch_smoother.hpp"
#include "ui/imgui_renderer.hpp"
#include "ui/tuner_widget.hpp"
#include "ui/waveform_widget.hpp"

#include <algorithm>
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
  auto* window = init_gl_window(args.width, args.height, "ear_training");
  if (!window) {
    return 1;
  }

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
  core::GpuPipeline pipeline{*fft_cfg, fb_w, cfg.db_min, cfg.db_max,
                             cfg.spectrum_scale, cfg.spectrum_fraction, cfg.tuner_fraction};
  if (!pipeline.ok()) {
    LOG_ERROR("GPU pipeline failed to initialise — check shader compile errors above");
    return 1;
  }

  // --- UI ---
  ui::ImGuiRenderer  imgui{window, "#version 450"};
  ui::WaveformWidget waveform_widget{cfg.wave_width, cfg.wave_height, cfg.wave_margin};
  ui::TunerWidget    tuner_widget{cfg.spectrum_fraction, cfg.tuner_fraction};
  audio::PitchSmoother smoother;

  // --- Audio ---
  audio::AudioCapture capture;
  if (!capture.start()) {
    LOG_ERROR("Failed to start audio capture");
    // Continue without audio — allows visual testing without mic.
  }

  // Per-frame CPU buffer for waveform display and FFT upload.
  std::vector<float> waveform(fft_cfg->fft_n, 0.f);

  const uint32_t fft_bins = fft_cfg->fft_n / 2u + 1u;

  glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

  // --- Main loop ---
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // --- Read audio ---
    // Peek at the most recent fft_n samples without consuming them.
    // If fewer are available, the waveform buffer retains prior data.
    {
      const uint32_t avail = capture.ring().available();
      if (avail >= fft_cfg->fft_n) {
        // Skip old data so we're always at the tail of the ring.
        const uint32_t skip = avail - fft_cfg->fft_n;
        if (skip > 0u) {
          float discard[256];
          uint32_t remaining = skip;
          while (remaining > 0u) {
            const uint32_t chunk = std::min(remaining, 256u);
            capture.ring().read(discard, chunk);
            remaining -= chunk;
          }
        }
        capture.ring().peek(waveform.data(), fft_cfg->fft_n);
      }
    }

    // --- GPU compute ---
    pipeline.dispatch(waveform);

    // --- Pitch detection ---
    const std::span<const float> mag{pipeline.sync_get_mag_data(), fft_bins};
    auto detection = audio::detect_peaks(
        mag, fft_cfg->fft_n, kSampleRate, cfg.min_db, cfg.max_hwhm_bins, cfg.max_peaks);

    // Keep a const-ref copy for the smoother (detect_peaks result is small).
    const std::optional<audio::DetectionResult> raw_pitch =
        detection.peaks.empty() ? std::nullopt
                                 : std::optional<audio::DetectionResult>{detection};

    const std::optional<float> smoothed_cents =
        smoother.update(raw_pitch, cfg.ema_alpha, cfg.stability_frames);

    // Pitch is only forwarded to the UI once the stability gate has committed.
    std::optional<audio::DetectionResult> display_pitch;
    if (smoothed_cents.has_value()) {
      display_pitch = std::move(detection);
    }

    // --- Render ---
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glClear(GL_COLOR_BUFFER_BIT);

    pipeline.render(fb_w, fb_h);

    // --- UI overlay ---
    imgui.begin_frame();

    ui::FrameData frame{
        .waveform           = waveform,
        .framebuffer_width  = fb_w,
        .framebuffer_height = fb_h,
        .pitch              = std::move(display_pitch),
        .smoothed_cents     = smoothed_cents.value_or(0.0f),
    };
    waveform_widget.draw(frame);
    tuner_widget.draw(frame);

    imgui.end_frame();

    glfwSwapBuffers(window);
  }

  // --- Cleanup ---
  capture.stop();
  // All other resources cleaned up by RAII destructors (pipeline, imgui).

  glfwTerminate();
  return 0;
}
