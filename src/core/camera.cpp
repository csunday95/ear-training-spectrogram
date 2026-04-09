#include "camera.hpp"
#include "app_state.hpp"

#include <GLFW/glfw3.h>

static AppState& get_state(GLFWwindow* window) {
    return *static_cast<AppState*>(glfwGetWindowUserPointer(window));
}

void Camera::scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto& cam = get_state(window).camera;
    cam.distance -= static_cast<float>(yoffset) * 0.5f;
    if (cam.distance < 0.5f)
        cam.distance = 0.5f;
    if (cam.distance > 100.0f)
        cam.distance = 100.0f;
}

void Camera::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        auto& cam = get_state(window).camera;
        cam.dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(window, &cam.last_x, &cam.last_y);
    }
}

void Camera::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto& cam = get_state(window).camera;
    if (!cam.dragging)
        return;
    auto dx = static_cast<float>(xpos - cam.last_x);
    auto dy = static_cast<float>(ypos - cam.last_y);
    cam.yaw += dx * 0.005f;
    cam.pitch += dy * 0.005f;
    constexpr float limit = 1.5f;
    if (cam.pitch > limit)
        cam.pitch = limit;
    if (cam.pitch < -limit)
        cam.pitch = -limit;
    cam.last_x = xpos;
    cam.last_y = ypos;
}
