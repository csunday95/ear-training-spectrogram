#version 450 core

in float v_depth;

out vec4 frag_color;

uniform vec3 particle_color;

void main() {
    // Discard corners to make round points from gl_PointCoord
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0) discard;

    // Simple depth-based fade for a bit of depth cueing
    float brightness = mix(0.3, 1.0, 1.0 - clamp(v_depth, 0.0, 1.0));
    frag_color = vec4(particle_color * brightness, 1.0);
}
