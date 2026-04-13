# Ear-Training Spectrogram — Implementation Plan

## Context

This repo is a copy of an OpenGL 4.5 compute shader particle simulation template. We're converting it into a **real-time spectrogram for ear training** — specifically, to show whether a sung note is flat or sharp relative to a piano reference pitch. The app captures audio from a single mic (picking up both piano and voice), runs FFT on the GPU, and renders a waterfall spectrogram + live spectrum + tuner indicator.

### Key Decisions
- **Audio capture**: miniaudio (single-header C lib)
- **FFT**: GPU compute shader — adapted from existing Stockham radix-2 FFT in `C:\workspace\graphics\compute-shader-gravity-fiddling\shaders\pm\pm_fft.comp`
- **FFT size**: Runtime-configurable (default 4096). Guard: query `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE` at startup, fallback to 2048 if < 64KB — unlikely on any desktop GPU from 2012+, but satisfies the GL 4.5 spec floor
- **Visuals**: Scrolling waterfall + real-time FFT magnitude spectrum + tuner bar
- **Tuner**: Centered bar with color coding (green=in-tune, blue=flat, red=sharp); directional arrow on spectrum view

### Status
- **Phase 0**: Complete ✓ — audio capture, ring buffer, waveform display, all tests passing
- **Phase 0.5 (UI subsystem)**: Complete ✓ — ImGui extracted into `src/ui/`, Widget ABC + `FrameData` established
- **Phase 1 (partial)**: Complete ✓ — `fft_config.hpp/cpp`, `shaders/compute/` shaders, all FFT tests passing. Remaining: SSBO setup + compute dispatch in `main.cpp`.
- **Phase 2**: Complete ✓ — `GpuPipeline` class, full compute chain + waterfall/spectrum rendering, dB normalization, configurable display range and spectrum headroom.
- **Phase 3**: Complete ✓ — pitch detection, music theory, EMA smoother with frequency-proximity gate, tuner band widget, JSON app config.

---

## Ongoing: Documentation + Style

These apply across all phases:

- **`.clang-format`** — LLVM-based, 2-space indent, 100-col limit, attached braces, left-aligned pointers, include sorting with glad/GLFW priority.
- **`CLAUDE.md`** — Update as architecture evolves. Add new build targets, shader pipeline docs, audio subsystem notes. Keep it accurate to current state after each phase.
- **Style conventions** — `namespace audio` for all types in `src/audio/`. `const` on local variables where not onerous, not on by-value function parameters.

---

## Phase 0.5: UI Subsystem + Widget ABC ✓

**Goal**: Extract ImGui lifecycle and widget drawing from `main.cpp` into a clean, extensible subsystem.

### Created
- `src/ui/frame_data.hpp` — `FrameData` struct: per-frame state passed by `const&` to all widget `draw()` calls. No default values. Extensible: Phase 2 adds FFT magnitude pointer, Phase 3 adds pitch/capture state.
- `src/ui/widget.hpp` — Abstract base class. All widgets inherit and implement `void draw(const FrameData& frame) = 0`. Non-copyable/movable.
- `src/ui/imgui_renderer.hpp/cpp` — RAII wrapper: constructor inits ImGui context + backends, destructor tears down. `begin_frame()` / `end_frame()` called each loop iteration.
- `src/ui/waveform_widget.hpp/cpp` — Concrete `Widget` subclass. Wraps `ImGui::Begin/PlotLines/End`. Temporary — replaced in Phase 2. Constants in anonymous namespace (`kWindowX`, `kWindowY`, etc.).

### Modified
- `src/main.cpp` — Removed inline ImGui boilerplate; uses `ImGuiRenderer` + `WaveformWidget`. Render loop builds `FrameData` and calls `waveform_widget.draw(frame)`.
- `CMakeLists.txt` — Added `src/ui/` to sources and include directories; kept `copy_shaders` copying both `shaders/compute` and `shaders/render`.
- `shaders/render/.gitkeep` — Tracks the (currently empty) `shaders/render/` directory so CI's `copy_directory` step succeeds.
- `tests/gl_test_fixture.hpp` — Added `REQUIRE_GL()` macro (single-line replacement for 3-line skip boilerplate).

### Widget pattern for future phases
All future widgets (`WaterfallWidget`, `SpectrumWidget`, `TunerWidget`) inherit from `Widget`. Only `FrameData` grows — widget `draw()` signatures never change.

---

## Phase 0: Scaffolding + Audio Capture ✓

**Goal**: Strip particle code, add miniaudio mic capture, add .clang-format, verify audio with ImGui waveform plot.

### Created
- `.clang-format`, `.clangd`
- `src/audio/ring_buffer.hpp` — Lock-free SPSC `RingBuffer<T, Capacity>`, compile-time power-of-2, `std::array` backing
- `src/audio/audio_capture.hpp/.cpp` — `audio::AudioCapture` wrapping miniaudio, mono 44.1kHz, stores `ma_device` by value
- `src/audio/miniaudio_impl.cpp` — sole TU defining `MINIAUDIO_IMPLEMENTATION`, compiled with `-w`
- `tests/test_ring_buffer.cpp` — 7 tests: basic r/w, partial read, wrap-around, overflow, peek, concurrent SPSC

### Modified
- `CMakeLists.txt` — miniaudio FetchContent, renamed to `ear_training`, glad as SYSTEM includes, `-Wno-c2y-extensions` on tests
- `src/core/app_state.hpp` — stripped Camera
- `src/main.cpp` — audio capture init, ring buffer → ImGui waveform plot
- `tests/test_shader_loading.cpp` — simplified to GL context smoke test

### Removed
- `src/core/camera.hpp/.cpp`, `shaders/compute/update.comp`, `shaders/render/points.vert/.frag`

---

## Phase 1: GPU FFT Pipeline (partial ✓)

**Goal**: Window → FFT → magnitude, all on GPU compute shaders. Validate with ImGui spectrum plot (persistent-mapped readback).

### Done ✓
- `shaders/compute/window_r2c.comp` — Hann window + real→complex. Reads `float audio[]` (binding 0), writes `vec2 complex_out[]` (binding 1). Uses preamble `FFT_N`.
- `shaders/compute/fft.comp` — Adapted from `pm_fft.comp`: strip `global_index()` 3D indexing → flat 1D. Single workgroup per dispatch. Preamble: `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE`.
- `shaders/compute/magnitude.comp` — Complex → dB. Reads FFT output (binding 0), writes `float magnitude[]` (binding 1). `uniform float mag_scale`.
- `src/audio/fft_config.hpp/cpp` — `FFTConfig` struct + `make_fft_config(uint32_t n)`. Queries `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE`.
- `tests/test_fft.cpp` — 10 tests, all passing. Note: Hann sidelobe floor is **−31 dB** (not −40 dB); bins ±1 from signal are the 3-bin main lobe and skipped in sidelobe assertions.

### Remaining
- `src/main.cpp` — Create SSBOs, dispatch compute chain each frame, read magnitude via mapped pointer for ImGui plot. See SSBO layout and dispatch sections below.

### SSBO Layout
| Buffer | Type | Size | Flags |
|--------|------|------|-------|
| `audio_ssbo` | float | FFT_N × 4B | `DYNAMIC_STORAGE_BIT` (CPU upload) |
| `complex_a_ssbo` | vec2 | FFT_N × 8B | 0 (GPU only) |
| `complex_b_ssbo` | vec2 | FFT_N × 8B | 0 (GPU only) |
| `magnitude_ssbo` | float | (FFT_N/2+1) × 4B | `MAP_READ_BIT \| MAP_PERSISTENT_BIT \| MAP_COHERENT_BIT` |

### Compute Dispatch (per frame)
1. `glNamedBufferSubData(audio_ssbo, ...)` — upload frame
2. `window_r2c.comp`: binding 0=audio, 1=complex_a. Dispatch `(FFT_N+255)/256` groups.
3. `fft.comp`: binding 0=complex_a (src), 1=complex_b (dst). `inverse=0`. Dispatch 1 group.
4. `magnitude.comp`: binding 0=complex_b, 1=magnitude. Dispatch `((FFT_N/2+1)+255)/256` groups.
5. Memory barriers between each step.

### Persistent Mapped Buffer for Magnitude Readback
The `magnitude_ssbo` uses persistent mapping (`GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT`) to avoid pipeline stalls. After `glNamedBufferStorage`, call `glMapNamedBufferRange` once at init. Each frame the CPU reads the previous frame's data while the GPU writes the current frame's (~16ms latency). A `GLsync` fence after the magnitude dispatch guards against reading in-flight data.

### Tests (`tests/test_fft.cpp`)
All tests use headless GL context. Upload known signal → run full compute pipeline → readback → assert.

1. **All-zero input**: All magnitude bins ≤ epsilon.
2. **DC offset**: Constant value → energy concentrates in bin 0.
3. **Single sine at bin center**: Peak at bin `k`, sidelobes ≥ 60dB below (Hann suppression).
4. **Single sine between bins**: Peak near bin 64 or 65 (spectral leakage behavior).
5. **Nyquist signal**: Alternating ±1 → energy at bin N/2.
6. **Known amplitude**: Peak magnitude matches expected (validates `mag_scale`).
7. **Two sines**: Two peaks at correct bins with correct relative amplitudes.
8. **Parseval's theorem**: Energy conservation within Hann window correction factor.
9. **Centered impulse**: n=N/2 impulse (Hann=1.0 there) → flat magnitude spectrum.

### Modify
- **`src/main.cpp`** — Create SSBOs, load shaders with preamble, dispatch compute chain, read magnitude via mapped pointer for ImGui plot.
- **`src/core/shader.hpp`** — Add preamble support to `load_render_program()` (needed in Phase 2).

---

## Phase 2: Waterfall + Spectrum Rendering ✓

**Goal**: Replace ImGui plots with GPU-rendered waterfall spectrogram and spectrum line chart.

### Created
- `src/core/gpu_pipeline.hpp/.cpp` — Encapsulates all GPU resources and per-frame dispatch/render. `const GLuint` handles initialised in constructor init list via static helpers. Constructor takes `initial_fb_w`, `db_min`, `db_max`, `spectrum_scale`.
- `shaders/compute/waterfall_update.comp` — `imageStore` into circular-buffer R32F texture at `write_col`.
- `shaders/compute/max_hold.comp` — `max_hold[i] = max(magnitude[i], max_hold[i] - decay_db)` (subtractive dB decay).
- `shaders/render/waterfall.vert/.frag` — Fullscreen triangle via `gl_VertexID`; fragment unwraps circular buffer with `fract(write_col/width + tc.x)` + `GL_REPEAT` wrap-S. Hot colormap.
- `shaders/render/spectrum.vert/.frag` — Reads magnitude or max-hold SSBO via `gl_VertexID`; two `GL_LINE_STRIP` draws per frame. `spectrum_scale` uniform (location 4) controls fraction of panel height `db_max` maps to.

### Screen Layout
- **Top 40%**: Waterfall, time scrolling L→R, frequency bottom→top
- **Middle 20%**: Tuner band (added in Phase 3)
- **Bottom 40%**: Spectrum — current magnitude (bright green) + max-hold envelope (dim yellow)
- ImGui waveform overlay composites on top

### CLI args / config
Display range and panel sizing are loaded from `ear_training.json` (written on first run with defaults). Key fields: `db_min`, `db_max`, `spectrum_scale`, `spectrum_fraction`, `tuner_fraction`; waveform overlay `width`/`height`/`margin`.

---

## Phase 3: Note Detection + Tuner ✓

**Goal**: Detect dominant pitch, show note name + cents deviation, render tuner bar.

### Created
- `src/audio/music_theory.hpp` — `freq_to_note(float hz) → NoteInfo { midi_note, octave, name, frequency, cents_offset }`. `cents = 1200 * log2(freq / ref_freq)`.
- `src/audio/pitch_detect.hpp/.cpp` — FFT peak picking on persistent-mapped magnitude buffer. Parabolic interpolation + HWHM width filter to reject broadband noise. Returns `DetectionResult` with sorted `DetectedPeak`s.
- `src/audio/pitch_smoother.hpp` — Header-only `PitchSmoother` class. EMA on cents + frequency-proximity stability gate: requires `stability_frames` consecutive frames within `gate_cents` of the previous frame's frequency before committing. Gate resets on silence or a jump larger than `gate_cents`; tracks slow vocal drift without resetting.
- `src/core/app_config.hpp/.cpp` — `AppConfig` struct loaded from `ear_training.json` via nlohmann/json. Written with defaults on first run. Sections: `display`, `waveform_overlay`, `pitch_detection`, `tuner_smoother`.
- `src/ui/tuner_widget.hpp/.cpp` — Full-width horizontal band (middle 20% of framebuffer). When locked: gradient bar (blue→green→red), tick marks at 0/±25/±50 ¢, needle at smoothed cents, note name + Hz + cents readout. When silent/gating: "--". Transparent overlay on spectrum panel shows a triangle marker at the dominant peak's x-position.
- `tests/test_music_theory.cpp`, `tests/test_pitch_detect.cpp` — freq↔note correctness; headless GL pipeline tests for peak picking on known sinusoids.

### Modified
- `src/ui/frame_data.hpp` — Added `std::optional<audio::DetectionResult> pitch` (nullopt during gate or silence) and `float smoothed_cents`.
- `src/core/gpu_pipeline.hpp/.cpp` — `spectrum_fraction_` and `tuner_fraction_` runtime parameters (replaced compile-time constant). `render()` computes waterfall/tuner/spectrum viewports from these fractions.
- `src/core/gl_init.hpp/.cpp` — Added `GlfwGuard` struct; its destructor calls `glfwTerminate()`. Declared first in `main()` so it is destroyed last, after `GpuPipeline` and `ImGuiRenderer` RAII destructors.

---

## Phase 4: Overlap, Smoothing, Log-Frequency Axis + Spectrum Axes

### Changes
- **50% overlap**: CPU-side `frame_buf[FFT_N]`, shift by `hop_size = FFT_N/2` each frame.
- **Exponential smoothing**: `smoothed[i] = mix(smoothed[i], magnitude[i], alpha)` (~0.3).
- **Log-frequency display**: `log2(freq/f_min)/log2(f_max/f_min)` mapping in waterfall.frag and spectrum.vert. Piano notes become equally spaced.
- **Spectrum axes + ticks**: Frequency (X) and dB (Y) axis lines + tick marks drawn via `ImGui::GetWindowDrawList()` overlaid on the spectrum GL viewport. No new library dependency — the GPU `GL_LINE_STRIP` rendering is preserved. Tick label positions computed from the same log-frequency mapping used in spectrum.vert so they stay aligned. Piano note labels (C2, C3, C4, A4, C5, etc.) on X; dB grid lines on Y.

---

## Phase 5: Voice Pitch via YIN + Source Discrimination

### Create
- `src/audio/yin.hpp/.cpp` — CPU YIN pitch detector on time-domain data. Window ~1200 samples for 80–1000 Hz range. Difference function → CMNDF → threshold (0.15) → parabolic interpolation.
- `tests/test_yin.cpp` — Synthetic sine/sawtooth → correct pitch.

### Voice vs. Piano Heuristic
Amplitude envelope state machine tracking RMS over ~200ms. Sharp onset → piano; steady amplitude → voice. Expected to need significant iterative tuning with real audio.

---

## Phase 6: Interval / Chord Detection (Later Feature)

### Create
- `src/audio/interval.hpp/.cpp` — Pairwise frequency ratios from `DetectionResult::peaks` → interval classification (±30 cents tolerance). Chord dictionary for 3–4 note sets.

---

## File Inventory (Final State)

```
src/
  main.cpp
  core/
    app_config.hpp/.cpp     — JSON config loader (ear_training.json)
    app_state.hpp, gl_init.hpp/.cpp, shader.hpp/.cpp, log.hpp
  audio/
    ring_buffer.hpp, audio_capture.hpp/.cpp, miniaudio_impl.cpp
    fft_config.hpp/.cpp
    music_theory.hpp        — freq_to_note(), NoteInfo
    pitch_detect.hpp/.cpp   — FFT peak picking, parabolic interp, HWHM filter
    pitch_smoother.hpp      — EMA + frequency-proximity stability gate
    yin.hpp/.cpp            — (Phase 5)
    interval.hpp/.cpp       — (Phase 6)
  ui/
    frame_data.hpp          — per-frame state: waveform, fb dims, pitch, smoothed_cents
    widget.hpp              — abstract Widget base class
    imgui_renderer.hpp/.cpp — RAII ImGui context wrapper
    waveform_widget.hpp/.cpp
    tuner_widget.hpp/.cpp   — full-width tuner band + spectrum triangle marker
shaders/
  compute/
    window_r2c.comp, fft.comp, magnitude.comp
    waterfall_update.comp, max_hold.comp
  render/
    waterfall.vert/.frag, spectrum.vert/.frag
tests/
  gl_test_fixture.hpp       — headless GL fixture + REQUIRE_GL() macro
  test_ring_buffer.cpp, test_shader_loading.cpp, test_fft.cpp
  test_music_theory.cpp, test_pitch_detect.cpp
  test_yin.cpp              — (Phase 5)
```
