# Core subsystem (`src/core/`)

## Files

- **`log.hpp`** — `LOG_ERROR` / `LOG_INFO` / `LOG_WARN` macros via C++23 `std::println`.

- **`gl_init.hpp/cpp`** — GLFW window + GL 4.5 core context. `init_gl_context_headless()` for tests. `GlfwGuard` RAII struct (holds `GLFWwindow*`, dtor calls `glfwTerminate()`). Declare it **first** in `main()` so it is destroyed last, after `GpuPipeline` and `ImGuiRenderer`.

- **`shader.hpp/cpp`** — File reading, shader compilation, program linking. `load_compute_program` injects a preamble (`#define FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE`) after the `#version` line.

- **`param.hpp`** — `Param<T>` template bundling a JSON key (`const char* key`) and its current value. The initial value is the compiled-in default — no separate k-constant needed. Config sub-structs using `Param<T>` fields declare `static constexpr const char* kSection` for the JSON section name.

- **`app_config.hpp/cpp`** — `AppConfig` is composed of four sub-structs, each using `Param<T>` fields:

  | Sub-struct | `kSection` | Fields |
  |------------|-----------|--------|
  | `DisplayConfig` | `"display"` | db_min, db_max, spectrum_scale, spectrum_fraction, tuner_fraction, log_freq_min, log_freq_max, smooth_alpha, max_hold_decay_db |
  | `WaveformConfig` | `"waveform_overlay"` | width, height, margin |
  | `PitchConfig` | `"pitch_detection"` | min_db, max_hwhm_bins, max_peaks, noise_estimation_frames, noise_floor_margin_db |
  | `TunerConfig` | `"tuner_smoother"` | ema_alpha, stability_frames, gate_cents |

  `load_app_config(path)` — reads `ear_training.json`; writes defaults on first run. Uses `load_param(section, param)` helper (anonymous namespace in .cpp). `AppConfig cfg{}` value-initialises all Param defaults. Access pattern: `cfg.display.db_min.value`, `cfg.pitch_detection.min_db.key`, etc.

- **`gpu_pipeline.hpp/cpp`** — Owns all GPU resources. Constructor takes individual config values (not `AppConfig` directly).

  **Compute chain** (runs per hop when `accum_buf ≥ hop_size`):
  1. `window_r2c.comp` — Hann window + real→complex
  2. `fft.comp` — Stockham radix-2 FFT (single workgroup; preamble injects FFT_N etc.)
  3. `magnitude.comp` — complex → dB magnitude → `magnitude_ssbo_`
  4. `smooth_magnitude.comp` — per-bin EMA → `smooth_ssbo_`
  5. `max_hold.comp` — exponential decay on `smooth_ssbo_`
  6. `waterfall_update.comp` — write `smooth_ssbo_` column into circular R32F texture

  **Rendering:**
  - Waterfall: fullscreen triangle, circular texture unwrapped by `write_col`. Log-freq V: `v = pow(2, tc.y * log_range) * f_min / nyquist`.
  - Spectrum: `GL_LINE_STRIP` from `smooth_ssbo_` (green) and `max_hold_ssbo_` (yellow) via `gl_VertexID`. Log-freq x: `t = log2(freq/f_min) / log_range`.
  - Pitch readback (`sync_get_mag_data()`) reads raw `magnitude_ssbo_` — not the smoothed buffer.
  - SSBOs are `const GLuint`, initialised in the constructor init list via static helpers (`make_ssbo`, `make_compute`, `make_render`, `make_vao`).
