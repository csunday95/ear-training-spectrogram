# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Real-time ear-training spectrogram for piano/voice. Captures mic audio, runs GPU FFT, and displays a scrolling waterfall spectrogram + live spectrum + tuner indicator showing whether a sung note is flat/sharp relative to a piano reference.

## Build Commands

```bash
# Configure (clang/clang++, Ninja, debug)
cmake --preset debug

# Build
cmake --build build/debug

# Run
./build/debug/ear_training --fft-n 4096 --width 1280 --height 720

# Run tests
ctest --test-dir build/debug --output-on-failure

# Other presets: release, release-native (LTO + march=native), relwithdebinfo
```

CI runs on Ubuntu with `xvfb-run` for headless GL testing.

## Architecture

### Audio subsystem (`src/audio/`, `namespace audio`)

- `ring_buffer.hpp` — Lock-free SPSC ring buffer. `RingBuffer<T, Capacity>` where Capacity is a compile-time power-of-2. `std::array` backing (no heap). Producer: miniaudio callback thread. Consumer: main GL thread.
- `audio_capture.hpp/cpp` — Wraps miniaudio `ma_device` for mono 44.1kHz mic capture. Stores `ma_device` by value. `kRingCapacity = 16384` floats (~372ms).
- `miniaudio_impl.cpp` — Sole TU that defines `MINIAUDIO_IMPLEMENTATION`. Compiled with `-w` (no warnings — third-party code). All other files just `#include "miniaudio.h"` for types.
- `music_theory.hpp` — Header-only. `freq_to_note(float hz) → NoteInfo`. `NoteInfo` fields: `midi_note` (uint8_t), `octave` (int8_t), `name` (std::string, e.g. "A4"), `frequency` (ET ref), `cents_offset` (negative=flat, positive=sharp). `kNoteNames[12]` constexpr array — swap for alternate mappings.
- `pitch_detect.hpp/cpp` — `detect_peaks(magnitude, fft_n, sample_rate, min_db, max_hwhm_bins, max_peaks) → DetectionResult`. `magnitude` is a span of `fft_n/2+1` dB floats from `GpuPipeline::sync_get_mag_data()`. Peak picking: local-max test, parabolic interpolation for sub-bin frequency, HWHM width filter (reject broadband noise). `DetectionResult::peaks` sorted by magnitude_db descending.
- `pitch_smoother.hpp` — Header-only class `PitchSmoother`. `update(raw, ema_alpha, stability_frames, gate_cents) → std::optional<float>` (smoothed cents). Gate: requires `stability_frames` consecutive frames within `gate_cents` cents of `candidate_freq_` before locking. Candidate frequency tracks detections each frame (tolerates slow vocal drift). Returns nullopt during gate period or silence. Reset on silence or jump ≥ `gate_cents`.

### Core (`src/core/`)

- `shader.hpp/cpp` — File reading, shader compilation, program linking. `load_compute_program` injects preamble (inserts `#define`s after `#version` line) — used to pass `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE` to compute shaders.
- `gl_init.hpp/cpp` — GLFW window + GL 4.5 core context. `init_gl_context_headless()` for tests. `GlfwGuard` RAII struct: holds `GLFWwindow*`, destructor calls `glfwTerminate()`. Declare it **first** in `main()` so it is destroyed last (after `GpuPipeline` and `ImGuiRenderer`), preventing use-after-terminate errors.
- `app_config.hpp/cpp` — `AppConfig` struct + `load_app_config(path)`. Reads `ear_training.json`; writes defaults on first run. Sections: `display` (db_min, db_max, spectrum_scale, spectrum_fraction, tuner_fraction, log_freq_min, log_freq_max, smooth_alpha), `waveform_overlay` (width, height, margin), `pitch_detection` (min_db, max_hwhm_bins, max_peaks), `tuner_smoother` (ema_alpha, stability_frames, gate_cents). Unknown keys log a warning.
- `log.hpp` — `LOG_ERROR`/`LOG_INFO`/`LOG_WARN` macros using C++23 `std::println`.
- `app_state.hpp` — Top-level state struct (currently empty).

### GPU Pipeline

Compute chain per frame (dispatch runs only when a full hop is available — see overlap below):
1. `window_r2c.comp` — Hann window + real→complex (audio SSBO → complex SSBO)
2. `fft.comp` — Stockham radix-2 FFT (adapted from gravity project). Single workgroup. Preamble: `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE`.
3. `magnitude.comp` — Complex → dB magnitude (complex SSBO → magnitude SSBO)
4. `smooth_magnitude.comp` — Per-bin EMA: `smooth[i] = mix(smooth[i], magnitude[i], alpha)` (magnitude SSBO → smooth SSBO). Alpha configurable via `display.smooth_alpha`.
5. `max_hold.comp` — Exponential max-hold decay on smooth SSBO
6. `waterfall_update.comp` — Write smooth SSBO column into circular R32F texture

Rendering:
- Waterfall: fullscreen triangle, circular-buffer texture with `write_col` unwrapping. Log-frequency V mapping: `v = pow(2, tc.y * log_range) * f_min / nyquist`.
- Spectrum: `GL_LINE_STRIP` drawn from smooth SSBO (bright green) and max-hold SSBO (dim yellow) via `gl_VertexID` (no VBO). Log-frequency x mapping: `t = log2(freq/f_min) / log_range`.
- Pitch detection readback (`sync_get_mag_data()`) reads raw `magnitude_ssbo_` — not the smoothed buffer.

### UI subsystem (`src/ui/`)

- `widget.hpp` — Abstract base class `Widget`. Pure `void draw(const FrameData& frame) = 0`. Non-copyable/movable.
- `frame_data.hpp` — `FrameData` struct passed by `const&` to all widget `draw()` calls. Fields: `waveform` (span), `framebuffer_width/height` (int), `pitch` (optional DetectionResult — nullopt when silent or gate not yet committed), `smoothed_cents` (float, valid only when pitch has a value), `spectrum_peak_x_norm` (float [0,1] — log-frequency-mapped x of dominant peak, used by tuner triangle marker).
- `imgui_renderer.hpp/cpp` — RAII wrapper: ctor inits ImGui + backends, dtor calls shutdown. `begin_frame()` / `end_frame()` per loop iteration.
- `waveform_widget.hpp/cpp` — Small oscilloscope overlay, bottom-right corner.
- `tuner_widget.hpp/cpp` — Full-width band occupying the middle `tuner_fraction` of the framebuffer. When locked: blue→green→red gradient bar, tick marks at 0/±25/±50 ¢, needle at `smoothed_cents`, note name + Hz + cents text. When silent/gating: "--". Separately draws a transparent overlay on the spectrum band with a triangle at the dominant peak's x-position (uses `frame.spectrum_peak_x_norm`). Constructed with `(spectrum_fraction, tuner_fraction)`.
- `spectrum_axis_widget.hpp/cpp` — Transparent ImGui overlay on the spectrum viewport. Draws horizontal dB grid lines (every 20 dB) and vertical frequency tick labels (A0, C2 … C8) aligned with the log-frequency axis. Tick screen-x = `log2(f/f_min) / log_range * fb_w`, matching `spectrum.vert`. Constructed with `(f_min, f_max, db_min, db_max, spectrum_scale, spectrum_fraction)`.

### Screen Layout

Three GL panels share the framebuffer height (fractions from `AppConfig`):
- **Top** `(1 - spectrum_fraction - tuner_fraction)`: Waterfall — time scrolling L→R, frequency bottom→top.
- **Middle** `tuner_fraction` (default 20%): Tuner band — ImGui window, no GL content.
- **Bottom** `spectrum_fraction` (default 40%): Spectrum — current magnitude (bright green) + max-hold envelope (dim yellow).

`GpuPipeline::render()` sets GL viewports: waterfall at `(0, spectrum_h + tuner_h, fb_w, waterfall_h)`, spectrum at `(0, 0, fb_w, spectrum_h)`. ImGui y=0 is top, so tuner window is at `y = fb_h - spectrum_h - tuner_h`.

### Per-frame data flow

Audio is consumed with 50% overlap: `ring.read()` → `accum_buf` each render frame; when `accum_buf ≥ hop_size` (= fft_n/2), `frame_buf` advances by one hop and dispatch runs. At 44.1 kHz and fft_n=4096, this gives ~21 FFT dispatches/second independent of render frame rate.

```
AudioCapture::ring() → accum_buf (drained each render frame)
  → (when accum_buf ≥ hop_size) advance frame_buf, then:
  → GpuPipeline::dispatch(frame_buf)  (GPU compute chain)
  → GpuPipeline::sync_get_mag_data()  (fence + CPU readback of raw magnitude)
  → detect_peaks()                    → DetectionResult
  → PitchSmoother::update()           → optional<float> smoothed_cents
  → FrameData{pitch, smoothed_cents, spectrum_peak_x_norm} → widget draw calls
```

`display_pitch`, `smoothed_cents`, and `spectrum_peak_x_norm` persist across render frames so the tuner holds its last committed value between hops.

### Shaders (`shaders/`)

Copied to build dir at build time. Paths are relative to binary (e.g., `"shaders/compute/fft.comp"`). Run from the build directory.

### Tests (`tests/`)

Catch2 v3 with headless GL context fixture (`gl_test_fixture.hpp`). Tests skip gracefully if no GPU available. Compiled with `-Wno-c2y-extensions` to suppress Catch2's `__COUNTER__` warnings. Current test files: `test_ring_buffer.cpp`, `test_shader_loading.cpp`, `test_fft.cpp`, `test_music_theory.cpp`, `test_pitch_detect.cpp`.

## Key Constraints

- C++23 (`std::println`, `__VA_OPT__`)
- All code in `namespace audio` for types under `src/audio/`
- Strict warnings: `-Wall -Wextra -Wpedantic` + `-Werror` on return-type, switch-enum, implicit-fallthrough, unused-result
- glad headers are marked `SYSTEM` to suppress khrplatform.h `-Wlanguage-extension-token` warnings
- `const` on local variables where not onerous; **not** on by-value function parameters
- FFT_N must be power-of-2. For FFT_N=4096: needs 64KB GPU shared memory (query `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE` at startup, fallback to 2048 if needed — unlikely on any desktop GPU post-2012)
- miniaudio: only one TU defines `MINIAUDIO_IMPLEMENTATION`; all others include the header for types only

## Style

### Formatting & Includes

- `.clang-format`: LLVM base, 2-space indent, 100-col limit, attached braces, left pointer/qualifier alignment
- Include ordering: glad/GLFW first (priority -1/0), project headers second, STL last

### Namespacing

- All code in `namespace audio` for types under `src/audio/` — keeps subsystem types cleanly isolated, avoids collisions as project grows. Example: `audio::AudioCapture`, `audio::RingBuffer<>`.

### Const Correctness

- Use `const` on local variables and member functions where natural and not onerous. Apply it to any non-mutating member function.
- **Do NOT** use `const` on by-value function parameters — it's an implementation detail and adds noise without enforcement.

### Naming & Patterns

- Constants: `k` prefix — `kRingCapacity`, `kDefaultDbMin`, etc.
- GPU shader names: descriptive task names — `window_r2c.comp`, `magnitude.comp`, `waterfall_update.comp`.
- Compile-time shader constants: `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE` injected via preamble.
- `struct` for plain data aggregates (no invariants, all members public). `class` for types with private state or invariants (e.g. `PitchSmoother`, `GpuPipeline`, `AudioCapture`).
- No default member values or default function arguments in production code — callers are always explicit.
- `const GLuint` handles in GPU objects: initialized in constructor init list via `static` helper functions (`make_ssbo`, `make_compute`, etc.) — not assigned in the constructor body.
