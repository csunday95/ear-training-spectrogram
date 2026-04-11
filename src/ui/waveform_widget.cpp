#include <imgui.h>

#include "waveform_widget.hpp"

namespace ui {

namespace {

constexpr float kWindowX = 10.f;
constexpr float kWindowY = 10.f;
constexpr float kWindowWidthOffset = 20.f;  // margin on each side
constexpr float kWindowHeight = 160.f;
constexpr float kPlotHeight = 100.f;
constexpr float kMinMagnitude = -1.f;
constexpr float kMaxMagnitude = 1.f;

}  // namespace

void WaveformWidget::draw(const FrameData& frame) {
  if (frame.waveform.empty()) {
    return;
  }

  ImGui::SetNextWindowPos({kWindowX, kWindowY}, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      {static_cast<float>(frame.framebuffer_width) - kWindowWidthOffset, kWindowHeight},
      ImGuiCond_Always);

  if (ImGui::Begin("Waveform", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
    ImGui::PlotLines(
        "##wave",
        frame.waveform.data(),
        static_cast<int>(frame.waveform.size()),
        0,
        nullptr,
        kMinMagnitude,
        kMaxMagnitude,
        {ImGui::GetContentRegionAvail().x, kPlotHeight});
  }
  ImGui::End();
}

}  // namespace ui
