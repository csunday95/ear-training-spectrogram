#version 450 core

// Reads directly from the magnitude (or max-hold) SSBO via gl_VertexID.
// Caller issues glDrawArrays(GL_LINE_STRIP, 0, fft_bins) with an empty VAO.
// Rebind binding 0 and the line_color uniform for the second (max-hold) draw.

layout(std430, binding = 0) restrict readonly buffer MagnitudeBuffer {
  float magnitude[];
};

// Number of frequency bins to render (FFT_N/2 + 1).
layout(location = 0) uniform int fft_bins;
// dB display range — maps db_min→bottom (-1 NDC), db_max→top (+1 NDC).
layout(location = 1) uniform float db_min;
layout(location = 2) uniform float db_max;
// Fraction of panel height that db_max maps to (e.g. 0.9 leaves 10% headroom).
layout(location = 4) uniform float spectrum_scale;

void main() {
  int i = gl_VertexID;

  // Linear frequency axis: bin 0 at left, bin (fft_bins-1) at right.
  float x = float(i) / float(fft_bins - 1) * 2.0 - 1.0;

  float db = magnitude[i];
  float y  = clamp((db - db_min) / (db_max - db_min), 0.0, 1.0) * spectrum_scale * 2.0 - 1.0;

  gl_Position = vec4(x, y, 0.0, 1.0);
}
