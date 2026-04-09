#pragma once

#include <glad/gl.h>
#include <filesystem>
#include <initializer_list>
#include <string>

// Read an entire file into a string. Returns empty + prints to stderr on failure.
std::string read_file(const std::filesystem::path& path);

// Compile a single shader stage. Returns 0 on failure (error printed to stderr).
GLuint compile_shader(GLenum type, const std::string& source);

// Link compiled shader stages into a program. Returns 0 on failure.
GLuint link_program(std::initializer_list<GLuint> shaders);

// Convenience: load and link a compute program from a single .comp file.
// If preamble is non-empty it is injected after the first line (#version ...) of the source,
// allowing callers to inject #define directives that drive layout qualifiers and constants.
GLuint load_compute_program(const std::filesystem::path& comp_path,
                            const std::string& preamble = "");

// Convenience: load and link a vert+frag render program.
GLuint load_render_program(const std::filesystem::path& vert_path,
                           const std::filesystem::path& frag_path);
