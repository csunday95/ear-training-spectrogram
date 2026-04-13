#version 450 core

// Solid line color; set per-draw to differentiate magnitude vs. max-hold.
layout(location = 3) uniform vec4 line_color;

out vec4 frag_color;

void main() {
  frag_color = line_color;
}
