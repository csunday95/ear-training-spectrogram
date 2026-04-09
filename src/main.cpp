#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <CLI/CLI.hpp>

#include "app_state.hpp"
#include "camera.hpp"
#include "gl_init.hpp"
#include "log.hpp"
#include "shader.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// GPU-side layout: must match update.comp
struct Particle {
    glm::vec4 pos;  // xyz = position, w = unused
    glm::vec4 vel;  // xyz = velocity, w = unused
};
static_assert(sizeof(Particle) == 32);

struct Args {
    uint32_t particles = 4096;
    int width = 1280;
    int height = 720;
};

static Args parse_args(int argc, char** argv) {
    Args args;
    CLI::App app{"opengl_template"};
    app.add_option("--particles", args.particles, "Number of particles")->capture_default_str();
    app.add_option("--width",     args.width,     "Window width")->capture_default_str();
    app.add_option("--height",    args.height,    "Window height")->capture_default_str();
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    }
    return args;
}

static std::vector<Particle> init_particles(uint32_t count) {
    std::mt19937 rng{42};
    std::uniform_real_distribution<float> pos_dist{-0.9f, 0.9f};
    std::uniform_real_distribution<float> vel_dist{-0.2f, 0.2f};

    std::vector<Particle> particles(count);
    for (auto& p : particles) {
        p.pos = {pos_dist(rng), pos_dist(rng), pos_dist(rng), 0.0f};
        p.vel = {vel_dist(rng), vel_dist(rng), vel_dist(rng), 0.0f};
    }
    return particles;
}

int main(int argc, char** argv) {
    const auto args = parse_args(argc, argv);
    const auto num_particles = args.particles;

    // --- GL init ---
    auto* window = init_gl_window(args.width, args.height, "opengl_template");
    if (!window)
        return 1;

    // --- Shaders ---
    const auto compute_prog = load_compute_program("shaders/compute/update.comp");
    if (!compute_prog) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const auto render_prog = load_render_program("shaders/render/points.vert",
                                                  "shaders/render/points.frag");
    if (!render_prog) {
        glDeleteProgram(compute_prog);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // --- Particle data ---
    const auto particles = init_particles(num_particles);

    // SSBO: immutable storage, initialized once from CPU data
    GLuint ssbo = 0;
    glCreateBuffers(1, &ssbo);
    glNamedBufferStorage(ssbo, static_cast<GLsizeiptr>(particles.size() * sizeof(Particle)),
                         particles.data(), 0);

    // VAO: attrib 0 = vec4 position read directly from the SSBO
    GLuint vao = 0;
    glCreateVertexArrays(1, &vao);
    glVertexArrayVertexBuffer(vao, 0, ssbo, 0, sizeof(Particle));
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);

    // --- GL state ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
    ImGui::StyleColorsDark();

    // --- Input callbacks ---
    AppState state;
    glfwSetWindowUserPointer(window, &state);
    glfwSetScrollCallback(window, Camera::scroll_callback);
    glfwSetMouseButtonCallback(window, Camera::mouse_button_callback);
    glfwSetCursorPosCallback(window, Camera::cursor_pos_callback);

    // --- Main loop ---
    constexpr float dt = 1.0f / 60.0f;
    double last_time = glfwGetTime();
    double fps_accum = 0.0;
    int fps_frames = 0;

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double frame_dt = now - last_time;
        last_time = now;

        fps_accum += frame_dt;
        ++fps_frames;
        if (fps_accum >= 1.0) {
            const auto fps = static_cast<int>(fps_frames / fps_accum);
            glfwSetWindowTitle(window, ("opengl_template  |  " + std::to_string(fps) + " fps").c_str());
            fps_accum = 0.0;
            fps_frames = 0;
        }

        glfwPollEvents();

        // --- Compute step ---
        glUseProgram(compute_prog);
        glUniform1f(glGetUniformLocation(compute_prog, "dt"), dt);
        glUniform1ui(glGetUniformLocation(compute_prog, "num_particles"), num_particles);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
        const auto groups = (num_particles + 255u) / 256u;
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        // --- Render ---
        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0) {
            glViewport(0, 0, fb_width, fb_height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = static_cast<float>(fb_width) / static_cast<float>(fb_height);
            const glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 200.0f);
            const glm::mat4 view = state.camera.view_matrix();
            const glm::mat4 mvp  = proj * view;

            glUseProgram(render_prog);
            glUniformMatrix4fv(glGetUniformLocation(render_prog, "mvp"), 1, GL_FALSE,
                               glm::value_ptr(mvp));
            glUniform1f(glGetUniformLocation(render_prog, "point_scale"), 8.0f);
            glUniform3f(glGetUniformLocation(render_prog, "particle_color"), 0.4f, 0.8f, 1.0f);

            glBindVertexArray(vao);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_particles));
            glBindVertexArray(0);
        }

        // --- ImGui overlay ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f);
        if (ImGui::Begin("##overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
            ImGui::Text("Particles: %u", num_particles);
            ImGui::Text("FPS:       %.1f", static_cast<double>(ImGui::GetIO().Framerate));
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glFinish();
    glDeleteBuffers(1, &ssbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(render_prog);
    glDeleteProgram(compute_prog);

    glfwMakeContextCurrent(nullptr);  // Wayland compatibility
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
