#include "gl_init.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "log.hpp"

static void error_callback(int /*error*/, const char* description) {
  LOG_ERROR("GLFW error: {}", description);
}

static void GLAPIENTRY gl_debug_callback(GLenum /*source*/, GLenum type, GLuint /*id*/,
                                         GLenum severity, GLsizei /*length*/, const GLchar* message,
                                         const void* /*userParam*/) {
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    return;
  const char* sev_str = "??";
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    sev_str = "HIGH";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    sev_str = "MED";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    sev_str = "LOW";
    break;
  }
  LOG_ERROR("[GL {}] type=0x{:x}: {}", sev_str, type, message);
}

GLFWwindow* init_gl_window(int width, int height, const char* title, bool visible) {
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    LOG_ERROR("Failed to init GLFW");
    return nullptr;
  }

  glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

  auto* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!window) {
    LOG_ERROR("Failed to create window");
    glfwTerminate();
    return nullptr;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  if (!gladLoadGL(glfwGetProcAddress)) {
    LOG_ERROR("Failed to load GL with glad");
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  LOG_INFO("GL {}, GLSL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
           reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(gl_debug_callback, nullptr);

  return window;
}

GlfwGuard::~GlfwGuard() { glfwTerminate(); }

GLFWwindow* init_gl_context_headless() {
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return nullptr;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  auto* window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return nullptr;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGL(glfwGetProcAddress)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(gl_debug_callback, nullptr);

  return window;
}
