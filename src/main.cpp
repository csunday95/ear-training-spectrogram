#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <CLI/CLI.hpp>

#include "audio_capture.hpp"
#include "gl_init.hpp"
#include "log.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

struct Args {
    uint32_t fft_n  = 4096;
    int      width  = 1280;
    int      height = 720;
};

static Args parse_args(int argc, char** argv) {
    Args args;
    CLI::App app{"ear_training"};
    app.add_option("--fft-n",  args.fft_n,  "FFT size (must be power of 2)")->capture_default_str();
    app.add_option("--width",  args.width,  "Window width")->capture_default_str();
    app.add_option("--height", args.height, "Window height")->capture_default_str();
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    }
    return args;
}

int main(int argc, char** argv) {
    const auto args = parse_args(argc, argv);

    // --- Window + GL ---
    auto* window = init_gl_window(args.width, args.height, "ear_training");
    if (!window)
        return 1;

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
    ImGui::StyleColorsDark();

    // --- Audio ---
    audio::AudioCapture capture;
    if (!capture.start()) {
        LOG_ERROR("Failed to start audio capture");
        // Continue without audio — allows UI testing without mic
    }

    // Per-frame CPU buffer for waveform display
    std::vector<float> waveform(args.fft_n, 0.f);

    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

    // --- Main loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- Read audio ---
        // Peek at the most recent fft_n samples without consuming them.
        // If fewer are available, the waveform buffer is padded with prior data.
        {
            const uint32_t avail = capture.ring().available();
            if (avail >= args.fft_n) {
                // Advance past old data so we're always at the tail of the ring.
                const uint32_t skip = avail - args.fft_n;
                if (skip > 0) {
                    float discard[256];
                    uint32_t remaining = skip;
                    while (remaining > 0) {
                        const uint32_t chunk = std::min(remaining, 256u);
                        capture.ring().read(discard, chunk);
                        remaining -= chunk;
                    }
                }
                capture.ring().peek(waveform.data(), args.fft_n);
            }
        }

        // --- Render ---
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- ImGui ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({10.f, 10.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({static_cast<float>(fb_w) - 20.f, 160.f}, ImGuiCond_Always);
        if (ImGui::Begin("Waveform", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("Audio: %s | Ring: %u samples available",
                        capture.running() ? "running" : "stopped",
                        capture.ring().available());
            ImGui::PlotLines("##wave", waveform.data(), static_cast<int>(waveform.size()),
                             0, nullptr, -1.f, 1.f,
                             {ImGui::GetContentRegionAvail().x, 100.f});
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    capture.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwMakeContextCurrent(nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
