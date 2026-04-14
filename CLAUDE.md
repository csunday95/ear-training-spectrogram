# CLAUDE.md

Real-time ear-training spectrogram for piano/voice. Captures mic audio, runs GPU FFT, displays a scrolling waterfall + live spectrum + tuner showing flat/sharp relative to a piano reference.

## Build

```bash
cmake --preset debug                                    # configure (clang/clang++, Ninja)
cmake --build build/debug                               # build
./build/debug/ear_training --fft-n 4096 --width 1280 --height 720
ctest --test-dir build/debug --output-on-failure        # tests
# other presets: release, release-native (LTO+march=native), relwithdebinfo
```

CI runs on Ubuntu with `xvfb-run` for headless GL testing.

## Architecture

Subsystem details live in per-directory CLAUDE.md files. This file covers cross-cutting concerns.

| Subsystem | Location | Notes |
|-----------|----------|-------|
| Audio capture + DSP | `src/audio/` | `namespace audio`; see `src/audio/CLAUDE.md` |
| Core / config / GPU | `src/core/` | see `src/core/CLAUDE.md` |
| UI widgets | `src/ui/` | ImGui overlay; see `src/ui/CLAUDE.md` |
| Shaders | `shaders/` | copied to build dir at build time |
| Tests | `tests/` | Catch2 v3, headless GL fixture |

### Screen layout

Three GL panels share the framebuffer height (`AppConfig::display` fractions):
- **Top** `(1 − spectrum_fraction − tuner_fraction)`: Waterfall — time L→R, frequency bottom→top
- **Middle** `tuner_fraction` (default 20 %): Tuner band — ImGui, no GL content
- **Bottom** `spectrum_fraction` (default 40 %): Spectrum — magnitude (green) + max-hold (yellow)

`GpuPipeline::render()` viewports: waterfall at `(0, spectrum_h+tuner_h, fb_w, waterfall_h)`, spectrum at `(0, 0, fb_w, spectrum_h)`. ImGui y=0 is top → tuner at `y = fb_h − spectrum_h − tuner_h`.

### Per-frame data flow

50 % overlap: `ring.read()` → `accum_buf`; when `accum_buf ≥ hop_size` (= fft_n/2), `frame_buf` advances one hop and dispatch runs (~21 FFT frames/s at 44.1 kHz / fft_n=4096).

```
AudioCapture::ring() → accum_buf (drained each render frame)
  → (accum_buf ≥ hop_size) advance frame_buf →
  → GpuPipeline::dispatch()          GPU compute chain
  → GpuPipeline::sync_get_mag_data() fence + CPU readback (raw magnitude)
  → NoiseFloor::update/subtract()    spectral denoising (once floor is ready)
  → detect_peaks()                 → DetectionResult          (FFT path)
  → Yin::estimate()                → YinResult                (time-domain path)
  → SourceClassifier::update()     → AudioSource (Piano/Voice/Unknown)
  → select active pitch (Voice→YIN, else→FFT peaks)
  → PitchSmoother::update()        → optional<float> smoothed_cents
  → FrameData{...}                 → widget draw calls
```

`display_pitch`, `smoothed_cents`, `spectrum_peak_x_norm` persist across render frames so the tuner holds its last committed value between hops.

## Key Constraints

- C++23 (`std::println`, `__VA_OPT__`)
- All types under `src/audio/` in `namespace audio`
- Strict warnings: `-Wall -Wextra -Wpedantic` + `-Werror` on return-type, switch-enum, implicit-fallthrough, unused-result
- glad headers marked `SYSTEM` to suppress khrplatform.h warnings
- FFT_N must be power-of-2; FFT_N=4096 needs 64 KB GPU shared memory
- miniaudio: only `miniaudio_impl.cpp` defines `MINIAUDIO_IMPLEMENTATION`

## Style

### Formatting & includes

- `.clang-format`: LLVM base, 2-space indent, 100-col limit, attached braces, left pointer/qualifier
- Include order: glad/GLFW first, project headers second, STL last

### Naming & patterns

- Constants: `k` prefix (`kRingCapacity`, `kDbInit`)
- `struct` for plain data aggregates; `class` for types with private state or invariants
- `const GLuint` handles in GPU objects: init in constructor init list via static helpers (`make_ssbo`, `make_compute`, …)
- No default member values or default function arguments in production code — exception: `Param<T>` fields in config sub-structs (the initial value *is* the compiled-in default by design)
- `const` on locals and non-mutating member functions; **not** on by-value function parameters

### Namespacing

All types under `src/audio/` go in `namespace audio`.

## Development Notes

**Stale clangd diagnostics:** diagnostics injected into Edit tool results reflect clangd's pre-edit AST and are unreliable mid-edit. Ignore them — always build to verify correctness.
