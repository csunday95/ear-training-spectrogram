#pragma once

#include <catch2/catch_test_macros.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "gl_init.hpp"

// Lazy singleton headless GL context for tier 2/3 tests.
// First call creates a hidden 1x1 GLFW window with GL 4.5 core.
// Lives until process exit (Catch2 owns the lifetime).
//
// Usage in tests:
//   REQUIRE_GL();  // skips the test case if no GL context is available
//
inline GLFWwindow*& gl_test_window() {
  static GLFWwindow* window = nullptr;
  return window;
}

inline bool is_gl_available() {
  static bool attempted = false;
  static bool success = false;

  if (!attempted) {
    attempted = true;
    gl_test_window() = init_gl_context_headless();
    success = (gl_test_window() != nullptr);
  }
  return success;
}

// Skip the enclosing TEST_CASE if no headless GL context is available.
#define REQUIRE_GL()                           \
  do {                                         \
    if (!is_gl_available()) {                  \
      SKIP("No GL context");                   \
    }                                          \
  } while (0)
