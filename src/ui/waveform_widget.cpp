#include <imgui.h>

#include "waveform_widget.hpp"

namespace ui {

namespace {

// Space consumed by the ImGui title bar and window padding above the plot.
constexpr float kTitleBarPad  = 28.f;
constexpr float kMinMagnitude = -1.f;
constexpr float kMaxMagnitude = 1.f;

}  // namespace

WaveformWidget::WaveformWidget(float width, float height, float margin)
    : width_{width}, height_{height}, margin_{margin} {}

void WaveformWidget::draw(const FrameData& frame) {
  if (frame.waveform.empty()) {
    return;
  }

  // Anchor to the bottom-right corner, clear of the spectrum viewport below.
  ImGui::SetNextWindowPos(
      {static_cast<float>(frame.framebuffer_width) - width_ - margin_,
       static_cast<float>(frame.framebuffer_height) - height_ - margin_},
      ImGuiCond_Always);
  ImGui::SetNextWindowSize({width_, height_}, ImGuiCond_Always);

  if (ImGui::Begin("Waveform", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
    const float plot_h = height_ - kTitleBarPad;
    ImGui::PlotLines(
        "##wave",
        frame.waveform.data(),
        static_cast<int>(frame.waveform.size()),
        0,
        nullptr,
        kMinMagnitude,
        kMaxMagnitude,
        {ImGui::GetContentRegionAvail().x, plot_h});
  }
  ImGui::End();
}

}  // namespace ui
