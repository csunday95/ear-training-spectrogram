# Ear Training Spectrogram

A real-time spectrogram for piano/voice ear training. Captures microphone audio and displays a live waterfall spectrogram alongside a pitch detector that shows whether a sung note is flat or sharp relative to a piano reference.

## Features

- **Live spectrogram**: Scrolling waterfall display + real-time FFT magnitude spectrum
- **GPU-accelerated FFT**: Stockham radix-2 compute shader pipeline
- **Tuner indicator**: Flat/sharp direction with cent offset, color-coded (blue=flat, green=in-tune, red=sharp)
- **Piano note detection**: FFT peak picking with parabolic interpolation for sub-bin accuracy
- **Voice pitch tracking**: YIN algorithm for monophonic voice pitch
- **Single mic**: Captures both piano and voice; software source discrimination
- **Configurable FFT size**: Trade frequency resolution vs. time resolution at runtime

## Requirements

- OpenGL 4.5 capable GPU
- Microphone (any system input device)
- Windows or Linux

## Building

```bash
cmake --preset debug
cmake --build build/debug
./build/debug/ear_training
```

Requires clang and Ninja. Dependencies (GLFW, GLM, ImGui, miniaudio, Catch2) are fetched automatically by CMake.

## Options

```
--fft-n N     FFT size, must be power of 2 (default: 4096)
--width W     Window width (default: 1280)
--height H    Window height (default: 720)
```

Larger FFT size gives better frequency resolution (useful for low bass notes) at the cost of slower time response. For piano ear training in middle octaves, 4096 at 44.1kHz gives ~10.7 Hz/bin — enough to resolve adjacent semitones above A1.

## Running Tests

```bash
ctest --test-dir build/debug --output-on-failure
```

GL-dependent tests skip automatically if no display or GPU is available (headless/CI).

## Architecture

Audio is captured from the default mic via miniaudio into a lock-free ring buffer. Each frame, the GPU pipeline windows and FFTs the audio, computes magnitudes, and updates the waterfall texture. Pitch detection runs on the CPU from a persistent-mapped magnitude buffer (no pipeline stall).

See [`CLAUDE.md`](CLAUDE.md) for developer-focused architecture details.
