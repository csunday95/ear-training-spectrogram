# UI subsystem (`src/ui/`)

## Files

- **`widget.hpp`** — Abstract base `Widget`. Pure `void draw(const FrameData& frame) = 0`. Non-copyable/movable.

- **`frame_data.hpp`** — `FrameData` passed by `const&` to all `draw()` calls:
  - `waveform` — span of the current `frame_buf` samples
  - `framebuffer_width / framebuffer_height` — raw GL framebuffer pixel dimensions (use for GL viewport math only)
  - `window_width / window_height` — logical screen dimensions from `glfwGetWindowSize` (use for ImGui positioning; differs from framebuffer dims on HiDPI/scaled displays)
  - `pitch` — `optional<DetectionResult>` (nullopt when silent or gate not committed)
  - `smoothed_cents` — valid only when `pitch` has a value
  - `spectrum_peak_x_norm` — log-frequency-mapped x ∈ [0,1] of the dominant peak (used by tuner triangle)

- **`imgui_renderer.hpp/cpp`** — RAII wrapper: ctor inits ImGui + GLFW/OpenGL3 backends, dtor shuts down. `begin_frame()` / `end_frame()` per loop iteration.

- **`waveform_widget.hpp/cpp`** — Small oscilloscope overlay, bottom-right corner. Constructed with `(width, height, margin)`.

- **`tuner_widget.hpp/cpp`** — Full-width band occupying the middle `tuner_fraction` of the framebuffer.
  - Locked: blue→green→red gradient bar, tick marks at 0/±25/±50 ¢, needle at `smoothed_cents`, note name + Hz + cents text.
  - Silent/gating: "--".
  - Also draws a transparent overlay on the spectrum panel with a triangle at `frame.spectrum_peak_x_norm`.
  - Constructed with `(spectrum_fraction, tuner_fraction)`.

- **`spectrum_axis_widget.hpp/cpp`** — Transparent ImGui overlay on the spectrum viewport.
  - Horizontal dB grid lines every 20 dB.
  - Vertical frequency tick labels (A0, C2 … C8) using `t = log2(f/f_min) / log_range * fb_w` — algebraically identical to `spectrum.vert`, so labels stay aligned.
  - Constructed with `(f_min, f_max, db_min, db_max, spectrum_scale, spectrum_fraction)`.
