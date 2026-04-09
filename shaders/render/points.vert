#version 450 core

layout(location = 0) in vec4 in_pos;  // xyz = position, w = unused

uniform mat4 mvp;
uniform float point_scale;

out float v_depth;

void main() {
    gl_Position = mvp * vec4(in_pos.xyz, 1.0);

    // Scale point size by inverse depth for perspective-correct sizing
    float dist = length(gl_Position.xyz);
    gl_PointSize = clamp(point_scale / dist, 1.0, 16.0);

    v_depth = gl_Position.z / gl_Position.w;
}
