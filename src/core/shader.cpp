#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

std::string read_file(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f) {
    std::cerr << "shader: failed to open " << path << "\n";
    return {};
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

GLuint compile_shader(GLenum type, const std::string& source) {
  auto shader = glCreateShader(type);
  const auto src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len));
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    std::cerr << "shader compile error:\n" << log.data() << "\n";
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint link_program(std::initializer_list<GLuint> shaders) {
  auto program = glCreateProgram();
  for (auto s : shaders) {
    glAttachShader(program, s);
  }
  glLinkProgram(program);

  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len));
    glGetProgramInfoLog(program, len, nullptr, log.data());
    std::cerr << "program link error:\n" << log.data() << "\n";
    glDeleteProgram(program);
    return 0;
  }

  // Detach and delete individual shaders — they're baked into the program now
  for (auto s : shaders) {
    glDetachShader(program, s);
    glDeleteShader(s);
  }
  return program;
}

GLuint load_compute_program(const std::filesystem::path& comp_path, const std::string& preamble) {
  auto source = read_file(comp_path);
  if (source.empty())
    return 0;

  if (!preamble.empty()) {
    // Inject the preamble after the first line (#version ...) so #define directives
    // are visible to the GLSL preprocessor before any layout() qualifiers are parsed.
    const auto newline = source.find('\n');
    const auto insert_pos = (newline == std::string::npos) ? source.size() : newline + 1;
    source.insert(insert_pos, preamble);
  }

  auto cs = compile_shader(GL_COMPUTE_SHADER, source);
  if (!cs)
    return 0;

  return link_program({cs});
}

GLuint load_render_program(const std::filesystem::path& vert_path,
                           const std::filesystem::path& frag_path) {
  auto vs_src = read_file(vert_path);
  auto fs_src = read_file(frag_path);
  if (vs_src.empty() || fs_src.empty())
    return 0;

  auto vs = compile_shader(GL_VERTEX_SHADER, vs_src);
  if (!vs)
    return 0;
  auto fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
  if (!fs) {
    glDeleteShader(vs);
    return 0;
  }

  return link_program({vs, fs});
}
