#include <imgui.h>

#include "spectrum_axis_widget.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

struct FreqTick {
  float       hz;
  const char* label;
};

// Piano-range frequency landmarks. Filtered at draw time to [f_min_, f_max_].
constexpr std::array<FreqTick, 16> kFreqTicks = {{
    {27.5f,   "A0"},
    {32.7f,   "C1"},
    {55.0f,   "A1"},
    {65.4f,   "C2"},
    {110.0f,  "A2"},
    {130.8f,  "C3"},
    {220.0f,  "A3"},
    {261.6f,  "C4"},
    {440.0f,  "A4"},
    {523.3f,  "C5"},
    {880.0f,  "A5"},
    {1046.5f, "C6"},
    {1760.0f, "A6"},
    {2093.0f, "C7"},
    {3520.0f, "A7"},
    {4186.0f, "C8"},
}};

constexpr int kGridAlpha  = 80;
constexpr int kTickAlpha  = 160;
constexpr int kLabelAlpha = 200;

}  // namespace

SpectrumAxisWidget::SpectrumAxisWidget(float f_min, float f_max, float db_min, float db_max,
                                       float spectrum_scale, float spectrum_fraction)
    : f_min_{f_min}
    , f_max_{f_max}
    , log_range_{std::log2(f_max / f_min)}
    , db_min_{db_min}
    , db_max_{db_max}
    , spectrum_scale_{spectrum_scale}
    , spectrum_fraction_{spectrum_fraction} {}

void SpectrumAxisWidget::draw(const FrameData& frame) {
  const float fb_w        = static_cast<float>(frame.framebuffer_width);
  const float fb_h        = static_cast<float>(frame.framebuffer_height);
  const float spectrum_h  = fb_h * spectrum_fraction_;
  const float panel_top_y = fb_h - spectrum_h;  // ImGui y of spectrum panel top

  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;

  ImGui::SetNextWindowPos({0.0f, panel_top_y}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({fb_w, spectrum_h}, ImGuiCond_Always);
  ImGui::Begin("##SpectrumAxis", nullptr, kFlags);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // dB grid lines (horizontal) — every 20 dB within [db_min_, db_max_].
  const float db_start = std::ceil(db_min_ / 20.0f) * 20.0f;
  for (float db = db_start; db <= db_max_; db += 20.0f) {
    const float y_norm   = (db - db_min_) / (db_max_ - db_min_) * spectrum_scale_;
    const float screen_y = fb_h - y_norm * spectrum_h;
    if (screen_y < panel_top_y || screen_y > fb_h) {
      continue;
    }
    dl->AddLine({0.0f, screen_y}, {fb_w, screen_y},
                IM_COL32(180, 180, 180, kGridAlpha), 1.0f);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%+.0f", db);
    dl->AddText({3.0f, screen_y - ImGui::GetTextLineHeight() - 1.0f},
                IM_COL32(200, 200, 200, kLabelAlpha), buf);
  }

  // Frequency tick lines (vertical) — piano notes within [f_min_, f_max_].
  for (const auto& tick : kFreqTicks) {
    if (tick.hz < f_min_ || tick.hz > f_max_) {
      continue;
    }
    const float t        = std::log2(tick.hz / f_min_) / log_range_;
    const float screen_x = t * fb_w;
    dl->AddLine({screen_x, panel_top_y}, {screen_x, panel_top_y + 8.0f},
                IM_COL32(200, 200, 200, kTickAlpha), 1.0f);
    dl->AddText({screen_x + 2.0f, panel_top_y + 10.0f},
                IM_COL32(220, 220, 220, kLabelAlpha), tick.label);
  }

  ImGui::End();
}

}  // namespace ui
