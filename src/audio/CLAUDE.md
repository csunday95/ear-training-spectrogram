# Audio subsystem (`src/audio/`, `namespace audio`)

## Files

- **`ring_buffer.hpp`** — Lock-free SPSC ring buffer. `RingBuffer<T, Capacity>` (Capacity: compile-time power-of-2, `std::array` backing). Producer: miniaudio callback thread. Consumer: main GL thread. `kRingCapacity = 16384` floats (~372 ms).

- **`audio_capture.hpp/cpp`** — Wraps `ma_device` for mono 44.1 kHz mic capture. `start()` / `stop()`. Exposes `ring()` for the consumer.

- **`miniaudio_impl.cpp`** — Sole TU defining `MINIAUDIO_IMPLEMENTATION`. Compiled with `-w`. All other TUs include the header for types only.

- **`music_theory.hpp`** — Header-only. `freq_to_note(float hz) → NoteInfo`. Fields: `midi_note`, `octave`, `name` (e.g. "A4"), `frequency` (ET ref), `cents_offset` (negative=flat, positive=sharp). `kNoteNames[12]` constexpr array.

- **`fft_config.hpp/cpp`** — `make_fft_config(fft_n, max_shared_mem) → optional<FFTConfig>`. Validates power-of-2 and shared-memory limits, computes `log2_n` and `local_size`.

- **`pitch_detect.hpp/cpp`** — `detect_peaks(magnitude, fft_n, sample_rate, min_db, max_hwhm_bins, max_peaks) → DetectionResult`. Input: span of `fft_n/2+1` dB floats from `GpuPipeline::sync_get_mag_data()` (or noise-subtracted). Peak picking: local-max test → parabolic interpolation (sub-bin freq) → HWHM width filter. `DetectionResult::peaks` sorted by magnitude_db descending.

- **`pitch_smoother.hpp`** — Header-only `PitchSmoother`. `update(raw, ema_alpha, stability_frames, gate_cents) → optional<float>` (smoothed cents). Gate: `stability_frames` consecutive frames within `gate_cents` cents of candidate before locking. Resets on silence or jump ≥ `gate_cents`.

- **`noise_floor.hpp/cpp`** — `NoiseFloor(n_bins, estimation_frames)`. Accumulates first N frames in linear power (`10^(dB/10)`), then provides `subtract(raw, out, margin_db, db_floor)` for spectral subtraction before peak detection. `ready()` → true once estimation complete. `reset()` restarts estimation (bound to 'R' key in `main.cpp`). Display pipeline is unaffected — subtraction is CPU-side only.
