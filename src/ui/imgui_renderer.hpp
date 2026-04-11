#pragma once

struct GLFWwindow;

namespace ui {

/**
 * RAII wrapper for ImGui context and backend initialization.
 *
 * Manages the lifecycle of ImGui, ImGui_ImplGlfw, and ImGui_ImplOpenGL3.
 * Non-copyable and non-movable (owns the ImGui context).
 *
 * Usage:
 *   ui::ImGuiRenderer imgui{window, "#version 450"};
 *   while (...) {
 *     imgui.begin_frame();
 *     // ... draw ImGui windows ...
 *     imgui.end_frame();
 *   }
 */
class ImGuiRenderer {
 public:
  /**
   * Initialize ImGui and backends. Must be called while window's GL context is current.
   * @param window GLFW window
   * @param glsl_version GLSL version string (e.g., "#version 450")
   */
  ImGuiRenderer(GLFWwindow* window, const char* glsl_version);

  /**
   * Shutdown ImGui and backends. Must be called while window's GL context is still current.
   */
  ~ImGuiRenderer();

  // Non-copyable and non-movable (owns ImGui context)
  ImGuiRenderer(const ImGuiRenderer&) = delete;
  ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

  /**
   * Begin a new ImGui frame. Must be called once per frame before drawing ImGui windows.
   * Calls NewFrame for both ImGui_ImplOpenGL3 and ImGui_ImplGlfw, then ImGui::NewFrame.
   */
  void begin_frame();

  /**
   * End the ImGui frame and render to the current GL framebuffer.
   * Calls ImGui::Render and ImGui_ImplOpenGL3_RenderDrawData.
   */
  void end_frame();
};

}  // namespace ui
