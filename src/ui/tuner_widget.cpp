#include <imgui.h>

#include "tuner_widget.hpp"

#include "music_theory.hpp"

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

// Horizontal padding as a fraction of window width on each side.
constexpr float kBarPadFraction  = 0.08f;
// Bar sits at this vertical fraction of the panel height.
constexpr float kBarYFraction    = 0.72f;
constexpr float kBarThickness    = 6.0f;
// Tick marks drawn below the bar.
constexpr float kTickGap         = 2.0f;
constexpr float kTickLength      = 8.0f;
// Needle extends this many pixels above and below the bar.
constexpr float kNeedleOverhang  = 8.0f;
constexpr float kNeedleThickness = 2.5f;
// Note name drawn at this vertical fraction of the panel.
constexpr float kNoteYFraction   = 0.08f;
// Readout line drawn at this vertical fraction.
constexpr float kReadoutYFraction = 0.38f;
// Font scale for the note name.
constexpr float kNoteFontScale   = 1.6f;
// Triangle marker for the spectrum overlay.
constexpr float kTriangleHalfWidth = 7.0f;
constexpr float kTriangleHeight    = 11.0f;
// Tuner bar background alpha.
constexpr float kBgAlpha           = 0.88f;

// Returns an ImU32 colour interpolating blue→green→red across [-50, 50] cents.
ImU32 cents_color(float cents) {
  const float t = std::clamp(cents / 50.0f, -1.0f, 1.0f);
  float red, green, blue;
  if (t < 0.0f) {
    // blue → green
    const float u = 1.0f + t;
    red   = 0.0f;
    green = u;
    blue  = 1.0f - u;
  } else {
    // green → red
    red   = t;
    green = 1.0f - t;
    blue  = 0.0f;
  }
  return IM_COL32(
      static_cast<int>(red   * 255.0f),
      static_cast<int>(green * 255.0f),
      static_cast<int>(blue  * 255.0f),
      255);
}

}  // namespace

TunerWidget::TunerWidget(float spectrum_fraction, float tuner_fraction)
    : spectrum_fraction_{spectrum_fraction}
    , tuner_fraction_{tuner_fraction} {}

void TunerWidget::draw(const FrameData& frame) {
  const float fb_w = static_cast<float>(frame.framebuffer_width);
  const float fb_h = static_cast<float>(frame.framebuffer_height);

  // The tuner band sits between the waterfall (top) and spectrum (bottom).
  // In ImGui screen coordinates (y=0 at top):
  //   waterfall: [0,              fb_h * waterfall_fraction)
  //   tuner:     [fb_h * waterfall_fraction, fb_h * (waterfall_fraction + tuner_fraction))
  //   spectrum:  [fb_h * (1 - spectrum_fraction), fb_h)
  const float tuner_h    = fb_h * tuner_fraction_;
  const float spectrum_h = fb_h * spectrum_fraction_;
  const float tuner_y    = fb_h - spectrum_h - tuner_h;

  // --- Main tuner panel ---
  constexpr ImGuiWindowFlags kPanelFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNav;

  ImGui::SetNextWindowPos({0.0f, tuner_y}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({fb_w, tuner_h}, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(kBgAlpha);
  ImGui::Begin("##Tuner", nullptr, kPanelFlags);

  const ImVec2 win_pos  = ImGui::GetWindowPos();
  const ImVec2 win_size = ImGui::GetWindowSize();
  ImDrawList*  dl       = ImGui::GetWindowDrawList();

  if (!frame.pitch.has_value() || frame.pitch->peaks.empty()) {
    const char* kPlaceholder = "--";
    const ImVec2 ts = ImGui::CalcTextSize(kPlaceholder);
    ImGui::SetCursorPos({(win_size.x - ts.x) * 0.5f, (win_size.y - ts.y) * 0.5f});
    ImGui::TextUnformatted(kPlaceholder);
  } else {
    const auto& peak  = frame.pitch->peaks[0];
    const auto  note  = audio::freq_to_note(peak.frequency);
    const float cents = std::clamp(frame.smoothed_cents, -50.0f, 50.0f);

    // Layout
    const float pad_x   = win_size.x * kBarPadFraction;
    const float bar_x0  = win_pos.x + pad_x;
    const float bar_x1  = win_pos.x + win_size.x - pad_x;
    const float bar_w   = bar_x1 - bar_x0;
    const float bar_cy  = win_pos.y + win_size.y * kBarYFraction;
    const float bar_y0  = bar_cy - kBarThickness * 0.5f;
    const float bar_y1  = bar_cy + kBarThickness * 0.5f;
    const float bar_mid = (bar_x0 + bar_x1) * 0.5f;

    // Gradient bar: blue→green (left half), green→red (right half)
    dl->AddRectFilledMultiColor(
        {bar_x0, bar_y0}, {bar_mid, bar_y1},
        IM_COL32(0, 100, 255, 160), IM_COL32(50, 200, 50, 160),
        IM_COL32(50, 200, 50, 160), IM_COL32(0, 100, 255, 160));
    dl->AddRectFilledMultiColor(
        {bar_mid, bar_y0}, {bar_x1, bar_y1},
        IM_COL32(50, 200, 50, 160), IM_COL32(255, 60, 60, 160),
        IM_COL32(255, 60, 60, 160), IM_COL32(50, 200, 50, 160));

    // Tick marks at 0, ±25, ±50 cents
    const float tick_y0 = bar_y1 + kTickGap;
    const float tick_y1 = tick_y0 + kTickLength;
    constexpr float kTickCents[] = {-50.0f, -25.0f, 0.0f, 25.0f, 50.0f};
    for (const float tc : kTickCents) {
      const float tx = bar_x0 + (0.5f + tc / 100.0f) * bar_w;
      dl->AddLine({tx, tick_y0}, {tx, tick_y1}, IM_COL32(200, 200, 200, 200), 1.0f);
    }
    // Labels at ±50 and 0
    dl->AddText({bar_x0,         tick_y1 + 2.0f}, IM_COL32(180, 180, 180, 220), "-50");
    dl->AddText({bar_x1 - 14.0f, tick_y1 + 2.0f}, IM_COL32(180, 180, 180, 220), "+50");
    {
      const ImVec2 zero_sz = ImGui::CalcTextSize("0");
      dl->AddText({bar_mid - zero_sz.x * 0.5f, tick_y1 + 2.0f},
                  IM_COL32(180, 180, 180, 220), "0");
    }

    // Needle
    const float needle_x = bar_x0 + (0.5f + cents / 100.0f) * bar_w;
    dl->AddLine({needle_x, bar_y0 - kNeedleOverhang},
                {needle_x, bar_y1 + kTickGap},
                cents_color(cents), kNeedleThickness);

    // Note name (larger font)
    ImGui::SetWindowFontScale(kNoteFontScale);
    {
      const ImVec2 name_sz = ImGui::CalcTextSize(note.name.c_str());
      ImGui::SetCursorPos({(win_size.x - name_sz.x) * 0.5f, win_size.y * kNoteYFraction});
      ImGui::TextUnformatted(note.name.c_str());
    }
    ImGui::SetWindowFontScale(1.0f);

    // Hz + cents readout
    char readout[48];
    std::snprintf(readout, sizeof(readout), "%.1f Hz  %+.0f \xC2\xA2",
                  peak.frequency, frame.smoothed_cents);
    {
      const ImVec2 rd_sz = ImGui::CalcTextSize(readout);
      ImGui::SetCursorPos({(win_size.x - rd_sz.x) * 0.5f, win_size.y * kReadoutYFraction});
      ImGui::TextUnformatted(readout);
    }
  }

  ImGui::End();

  // --- Spectrum frequency marker overlay ---
  if (!frame.pitch.has_value() || frame.pitch->peaks.empty()) {
    return;
  }

  const auto& peak  = frame.pitch->peaks[0];
  const auto  note  = audio::freq_to_note(peak.frequency);
  const float cents = std::clamp(frame.smoothed_cents, -50.0f, 50.0f);

  constexpr ImGuiWindowFlags kOverlayFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;

  // The overlay covers the spectrum panel (bottom band).
  ImGui::SetNextWindowPos({0.0f, fb_h - spectrum_h}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({fb_w, spectrum_h}, ImGuiCond_Always);
  ImGui::Begin("##SpectrumOverlay", nullptr, kOverlayFlags);

  {
    const ImVec2 ov_pos  = ImGui::GetWindowPos();
    const ImVec2 ov_size = ImGui::GetWindowSize();
    ImDrawList*  odl     = ImGui::GetWindowDrawList();

    const float marker_x = ov_pos.x + peak.bin_normalized * ov_size.x;
    const float base_y   = ov_pos.y + ov_size.y;
    odl->AddTriangleFilled(
        {marker_x,                      base_y - kTriangleHeight},
        {marker_x - kTriangleHalfWidth, base_y},
        {marker_x + kTriangleHalfWidth, base_y},
        cents_color(cents));
  }

  ImGui::End();
}

}  // namespace ui
