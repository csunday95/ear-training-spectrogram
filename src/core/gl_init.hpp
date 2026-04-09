#pragma once

struct GLFWwindow;

// Create a GLFW window with an OpenGL 4.5 core debug context and load GL via glad.
// Pass visible=false to create a hidden window (e.g. for headless/deferred render).
// Returns nullptr on failure (errors printed to stderr).
GLFWwindow* init_gl_window(int width, int height, const char* title, bool visible = true);

// Create a hidden 1x1 GLFW window for headless GL testing.
// Same GL 4.5 core debug context, no swap interval.
// Returns nullptr if GLFW/GL initialization fails (e.g. no display server).
GLFWwindow* init_gl_context_headless();
