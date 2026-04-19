#include <imgui.h>

#include "waveform_widget.hpp"

namespace ui {

namespace {

constexpr float kMinMagnitude = -1.f;
constexpr float kMaxMagnitude = 1.f;

}  // namespace

WaveformWidget::WaveformWidget(float spectrum_fraction, float waveform_fraction,
                               float tuner_fraction)
    : spectrum_fraction_{spectrum_fraction}
    , waveform_fraction_{waveform_fraction}
    , tuner_fraction_{tuner_fraction} {}

void WaveformWidget::draw(const FrameData& frame) {
  if (frame.waveform.empty()) {
    return;
  }

  const float win_w  = static_cast<float>(frame.window_width);
  const float win_h  = static_cast<float>(frame.window_height);
  const float height = win_h * waveform_fraction_;
  // Waterfall occupies the top fraction; waveform panel sits immediately below it.
  const float y_top  = win_h * (1.0f - spectrum_fraction_ - tuner_fraction_ - waveform_fraction_);

  ImGui::SetNextWindowPos({0.0f, y_top}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({win_w, height}, ImGuiCond_Always);

  constexpr auto kFlags = ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##waveform", nullptr, kFlags)) {
    ImGui::PlotLines(
        "##wave",
        frame.waveform.data(),
        static_cast<int>(frame.waveform.size()),
        0,
        nullptr,
        kMinMagnitude,
        kMaxMagnitude,
        ImGui::GetContentRegionAvail());
  }
  ImGui::End();
}

}  // namespace ui
