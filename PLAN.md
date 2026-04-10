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

---

## Ongoing: Documentation + Style

These apply across all phases:

- **`.clang-format`** — LLVM-based, 2-space indent, 100-col limit, attached braces, left-aligned pointers, include sorting with glad/GLFW priority.
- **`CLAUDE.md`** — Update as architecture evolves. Add new build targets, shader pipeline docs, audio subsystem notes. Keep it accurate to current state after each phase.
- **Style conventions** — `namespace audio` for all types in `src/audio/`. `const` on local variables where not onerous, not on by-value function parameters.

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

## Phase 1: GPU FFT Pipeline

**Goal**: Window → FFT → magnitude, all on GPU compute shaders. Validate with ImGui spectrum plot (persistent-mapped readback).

### Create
- `shaders/compute/window_r2c.comp` — Hann window + real→complex. Reads `float audio[]` (binding 0), writes `vec2 complex_out[]` (binding 1). Uses preamble `FFT_N`.
- `shaders/compute/fft.comp` — Adapted from `pm_fft.comp`: strip `global_index()` 3D indexing → flat 1D (`src[i0]`/`dst[i1]`). Keep `uniform uint inverse`. Single workgroup per dispatch. Preamble: `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE`.
- `shaders/compute/magnitude.comp` — `mag = sqrt(re² + im²)` → dB conversion. Reads FFT output (binding 0), writes `float magnitude[]` (binding 1). `uniform float mag_scale`.
- `src/audio/fft_config.hpp` — `FFTConfig` struct: `fft_n`, `log2_n`, `local_size`, `preamble()` method generating `#define` strings. Factory `make_fft_config(uint32_t n)` validates power-of-2. Queries `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE` to cap FFT_N (needs 2×N×8 bytes shared mem).
- `tests/test_fft.cpp` — Headless GL: upload known signals, run full pipeline, readback magnitude, assert.

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

## Phase 2: Waterfall + Spectrum Rendering

**Goal**: Replace ImGui plots with GPU-rendered waterfall spectrogram and spectrum line chart.

### Create
- `shaders/compute/waterfall_update.comp` — Writes new magnitude column into circular-buffer R32F texture via `imageStore`. Tracks `write_col` index (no pixel shifting).
- `shaders/compute/max_hold.comp` — `max_hold[i] = max(current[i], max_hold[i] * decay)`. Decay ~0.995/frame.
- `shaders/render/waterfall.vert` — Fullscreen triangle (3 vertices, no VBO, `gl_VertexID` trick).
- `shaders/render/waterfall.frag` — Reads circular-buffer texture with `write_col` unwrapping. Colormap. `uniform uint write_col, waterfall_width`.
- `shaders/render/spectrum.vert` — Reads magnitude SSBO via `gl_VertexID`. Maps bin→x, dB→y into viewport sub-region.
- `shaders/render/spectrum.frag` — Solid line color.

### Screen Layout
- **Top ~65%**: Waterfall (time scrolling L→R, frequency bottom→top)
- **Bottom ~35%**: Live spectrum (GL_LINE_STRIP) + max-hold envelope (second line strip, dimmer)
- Visual layout iterated after MVP inspection.

### Waterfall Texture
- `GL_TEXTURE_2D`, `GL_R32F`, 1024 × (FFT_N/2+1). ~4MB. `image2D` for compute write, `sampler2D` for fragment read.
- `write_col` increments mod 1024 each frame — no pixel copying.

---

## Phase 3: Note Detection + Tuner

**Goal**: Detect dominant pitch, show note name + cents deviation, render tuner bar.

### Create
- `src/audio/music_theory.hpp` — `freq_to_note(float hz) → NoteInfo { midi_note, name, frequency, cents_offset }`. `cents = 1200 * log2(freq / ref_freq)`.
- `src/audio/pitch_detect.hpp/.cpp` — FFT peak picking on persistent-mapped magnitude buffer. Parabolic interpolation (`k + 0.5*(m[k-1]-m[k+1])/(m[k-1]-2*m[k]+m[k+1])`). Returns `DetectionResult` with multiple `DetectedPeak`s (future-proofing for chords).
- `tests/test_music_theory.cpp` — Verify freq↔note for A4=440, C4≈261.63, etc.

### Tuner Widget (ImGui custom draw)
- Horizontal bar, centered = in-tune. Tick marks at ±50, ±25, 0 cents.
- Color gradient: blue (flat) → green (center) → red (sharp).
- Large note name + Hz and cents readout.

### Spectrum Arrow Overlay
- At detected frequency x-position: `←` or `→` arrow via `ImGui::AddTriangleFilled`, same color as tuner.

---

## Phase 4: Overlap, Smoothing, Log-Frequency Axis

### Changes
- **50% overlap**: CPU-side `frame_buf[FFT_N]`, shift by `hop_size = FFT_N/2` each frame.
- **Exponential smoothing**: `smoothed[i] = mix(smoothed[i], magnitude[i], alpha)` (~0.3).
- **Log-frequency display**: `log2(freq/f_min)/log2(f_max/f_min)` mapping in waterfall.frag and spectrum.vert. Piano notes become equally spaced.
- **Piano note labels**: ImGui text overlay at C2, C3, C4, A4, C5, etc.

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
    app_state.hpp, gl_init.hpp/.cpp, shader.hpp/.cpp, log.hpp
  audio/
    ring_buffer.hpp, audio_capture.hpp/.cpp, miniaudio_impl.cpp
    fft_config.hpp, pitch_detect.hpp/.cpp, music_theory.hpp
    yin.hpp/.cpp, interval.hpp/.cpp
shaders/
  compute/
    window_r2c.comp, fft.comp, magnitude.comp
    waterfall_update.comp, max_hold.comp
  render/
    waterfall.vert/.frag, spectrum.vert/.frag
tests/
  gl_test_fixture.hpp, test_ring_buffer.cpp, test_fft.cpp
  test_music_theory.cpp, test_yin.cpp
```
