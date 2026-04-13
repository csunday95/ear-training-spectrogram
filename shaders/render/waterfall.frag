#version 450 core

in vec2 texcoord;
out vec4 frag_color;

// Waterfall circular-buffer texture (GL_R32F). Row 0 = DC, row N = Nyquist.
// Wrap-S is GL_REPEAT so the circular-buffer unwrap below works correctly.
layout(binding = 0) uniform sampler2D waterfall_tex;

// Column that will be written NEXT frame (= oldest surviving column = left edge).
layout(location = 0) uniform uint write_col;
// Total number of columns in the texture (kWaterfallWidth, e.g. 1024).
layout(location = 1) uniform uint waterfall_width;
// dB display range for colormap normalization.
layout(location = 2) uniform float db_min;
layout(location = 3) uniform float db_max;
// Log-frequency axis parameters.
layout(location = 4) uniform float f_min;          // minimum display frequency (Hz)
layout(location = 5) uniform float log_freq_range; // log2(f_max / f_min)
layout(location = 6) uniform float nyquist;        // sample_rate / 2.0

// Classic "hot" colormap: black → red → yellow → white.
vec3 colormap_hot(float t) {
  t = clamp(t, 0.0, 1.0);
  return vec3(
      clamp(t * 3.0, 0.0, 1.0),
      clamp(t * 3.0 - 1.0, 0.0, 1.0),
      clamp(t * 3.0 - 2.0, 0.0, 1.0));
}

void main() {
  // Circular-buffer unwrap: oldest data at left (tc.x=0), newest at right (tc.x→1).
  // write_col is the oldest column. Adding tc.x * width steps through the circular
  // buffer; fract() wraps at the texture boundary (texture wrap-S is GL_REPEAT).
  float u = fract(float(write_col) / float(waterfall_width) + texcoord.x);

  // texcoord.y maps log-linearly from 0 (bottom = f_min) to 1 (top = f_max).
  // Back-project to the linear texture V coordinate (0 = DC, 1 = Nyquist).
  float freq_hz = pow(2.0, texcoord.y * log_freq_range) * f_min;
  float v       = clamp(freq_hz / nyquist, 0.0, 1.0);
  float db      = texture(waterfall_tex, vec2(u, v)).r;
  float t   = clamp((db - db_min) / (db_max - db_min), 0.0, 1.0);

  frag_color = vec4(colormap_hot(t), 1.0);
}
