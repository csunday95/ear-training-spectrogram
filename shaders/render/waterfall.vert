#version 450 core

// Fullscreen triangle — no VBO required.
// gl_VertexID 0,1,2 → three vertices that cover the entire NDC square [-1,1]².
// The triangle extends past the right and top edges; clipping discards the excess.

out vec2 texcoord;

void main() {
  const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  const vec2 tc[3]  = vec2[3](vec2( 0.0,  0.0), vec2(2.0,  0.0), vec2( 0.0, 2.0));

  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
  texcoord    = tc[gl_VertexID];
}
