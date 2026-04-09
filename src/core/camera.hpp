#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

struct GLFWwindow;
struct AppState;

struct Camera {
    float distance = 5.0f;
    float yaw = 0.0f;
    float pitch = 0.3f;
    bool dragging = false;
    double last_x = 0.0, last_y = 0.0;

    glm::mat4 view_matrix() const {
        const glm::vec3 eye{
            distance * std::cos(pitch) * std::sin(yaw),
            distance * std::sin(pitch),
            distance * std::cos(pitch) * std::cos(yaw)
        };
        return glm::lookAt(eye, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    }

    // GLFW callbacks — retrieve Camera from AppState via window user pointer
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
};
