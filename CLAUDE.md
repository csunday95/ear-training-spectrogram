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

### Core (`src/core/`)

- `shader.hpp/cpp` — File reading, shader compilation, program linking. `load_compute_program` injects preamble (inserts `#define`s after `#version` line) — used to pass `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE` to compute shaders.
- `gl_init.hpp/cpp` — GLFW window + GL 4.5 core context. `init_gl_context_headless()` for tests.
- `log.hpp` — `LOG_ERROR`/`LOG_INFO` macros using C++23 `std::println`.
- `app_state.hpp` — Top-level state struct (currently empty, expanded as project grows).

### GPU Pipeline (Phase 1+)

Compute chain per frame:
1. `window_r2c.comp` — Hann window + real→complex (audio SSBO → complex SSBO)
2. `fft.comp` — Stockham radix-2 FFT (adapted from gravity project). Single workgroup. Preamble: `FFT_N`, `FFT_LOG2_N`, `FFT_LOCAL_SIZE`.
3. `magnitude.comp` — Complex → dB magnitude (complex SSBO → magnitude SSBO)
4. `waterfall_update.comp` — Write new column into circular R32F texture
5. `max_hold.comp` — Exponential max-hold decay on magnitude SSBO

Rendering:
- Waterfall: fullscreen triangle, circular-buffer texture with `write_col` unwrapping
- Spectrum: `GL_LINE_STRIP` drawn from magnitude SSBO via `gl_VertexID` (no VBO)

### Shaders (`shaders/`)

Copied to build dir at build time. Paths are relative to binary (e.g., `"shaders/compute/fft.comp"`). Run from the build directory.

### Tests (`tests/`)

Catch2 v3 with headless GL context fixture (`gl_test_fixture.hpp`). Tests skip gracefully if no GPU available. Compiled with `-Wno-c2y-extensions` to suppress Catch2's `__COUNTER__` warnings.

## Key Constraints

- C++23 (`std::println`, `__VA_OPT__`)
- All code in `namespace audio` for types under `src/audio/`
- Strict warnings: `-Wall -Wextra -Wpedantic` + `-Werror` on return-type, switch-enum, implicit-fallthrough, unused-result
- glad headers are marked `SYSTEM` to suppress khrplatform.h `-Wlanguage-extension-token` warnings
- `const` on local variables where not onerous; **not** on by-value function parameters
- FFT_N must be power-of-2. For FFT_N=4096: needs 64KB GPU shared memory (query `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE` at startup, fallback to 2048 if needed — unlikely on any desktop GPU post-2012)
- miniaudio: only one TU defines `MINIAUDIO_IMPLEMENTATION`; all others include the header for types only

## Style

- `.clang-format`: LLVM base, 2-space indent, 100-col limit, attached braces, left pointer/qualifier alignment
- Include ordering: glad/GLFW first (priority -1/0), project headers second, STL last
