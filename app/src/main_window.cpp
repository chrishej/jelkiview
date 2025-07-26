#include "main_window.h"

#include <GLFW/glfw3.h>
#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <unordered_map>
#include <vector>

#include "ImGuiFileDialog.h"
#include "data_cursors.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "layout.h"
#include "licenses.h"
#include "log_reader.h"
#include "math.h"
#include "rapidcsv.h"
#include "settings.h"

typedef struct {
    double decimation_factor;
    int visible_min_idx;
    int visible_max_idx;

} DecimationData;

std::vector<double> Decimate(const std::vector<double>& input, int time_min_idx, int time_max_idx,
                             int m);
void ManageSubplot(int subplot_index,
                   std::unordered_map<std::string, std::vector<double>>* const signals,
                   std::vector<std::unordered_map<std::string, bool>>* subplots_map);

static ImPlotRange x_range;
bool keep_range = false;

bool remove_subplot = false;
int subplot_idx = 0;


// Decimates the input vector between indices [time_min_idx, time_max_idx] with step m.
std::vector<double> Decimate(const std::vector<double>& input, int time_min_idx, int time_max_idx, int m) {
  std::vector<double> result;
  if (input.empty()) return result;
  result.push_back(input[0]);
  size_t i;
  for (i = std::max(time_min_idx, 1); i < std::min(input.size() - 1, static_cast<size_t>(time_max_idx)); i += m) {
    result.push_back(input[i]);
  }
  if (i <= input.size() - 1) {
    result.push_back(input[input.size() - 1]);
  }
  return result;
}


// Handles the management UI for a subplot (signals, insert/remove).
void ManageSubplot(int subplot_index,
                   std::unordered_map<std::string, std::vector<double>>* signals,
                   std::vector<std::unordered_map<std::string, bool>>* subplots_map_loc) {
  ImGui::Text("Managing subplot %d", subplot_index);
  if (ImGui::Button("Signals")) {
    ImGui::OpenPopup("SignalsPopup");
  }
  if (ImGui::BeginPopup("SignalsPopup")) {
    auto& signal_map = (*subplots_map_loc)[subplot_index];
    for (auto& [signal_name, is_enabled] : signal_map) {
      if (ImGui::Checkbox(signal_name.c_str(), &is_enabled)) {
        layout::UpdateLayout();
      }
    }
    ImGui::EndPopup();
  }

  if (ImGui::Button("Remove Subplot")) {
    if (subplot_index >= 0 && static_cast<size_t>(subplot_index) < subplots_map_loc->size()) {
      subplots_map_loc->erase(subplots_map_loc->begin() + subplot_index);
      remove_subplot = true;
      subplot_idx = subplot_index;
    }
    keep_range = true;
  }

  if (ImGui::Button("Insert Subplot")) {
    if (subplot_index >= 0 && static_cast<size_t>(subplot_index) < subplots_map_loc->size()) {
      auto insert_at_it = subplots_map_loc->begin() + subplot_index + 1;
      int new_index = std::distance(subplots_map_loc->begin(), insert_at_it);
      subplots_map_loc->insert(insert_at_it, std::unordered_map<std::string, bool>{});
      for (const auto& [signal_name, _] : (*signals)) {
        (*subplots_map_loc)[new_index][signal_name] = false;
      }
    }
    keep_range = true;
  }
}


// Draws the main menu bar.
static void MenuBar() {
  ImGui::BeginMainMenuBar();
  LogReadButton();
  ImGui::Text("|");
  settings::ShowSettingsButton();
  ImGui::Text("|");
  if (ImGui::BeginMenu("Layout")) {
    layout::LayoutMenuButton();
    ImGui::EndMenu();
  }
  layout::GuiUpdate();
  ImGui::Text("|");
  if (ImGui::MenuItem("Cursors to view")) {
    v_line_1_pos = x_range.Min + (x_range.Max - x_range.Min) / 3.0;
    v_line_2_pos = x_range.Min + 2.0 * (x_range.Max - x_range.Min) / 3.0;
  }
  ImGui::Text("|");
  Licenses();
  ImGui::EndMainMenuBar();
}


// Calculates decimation factor and visible indices for the current x range.
static void CalculateDecimationData(const ImPlotRange& x_range_loc, DecimationData& decimation_data) {
  const auto& time = GetData()->time;
  if (time.size() < 2) {
    decimation_data.decimation_factor = 1.0;
    decimation_data.visible_min_idx = 0;
    decimation_data.visible_max_idx = 0;
    return;
  }
  double samples_in_range = (x_range_loc.Max - x_range_loc.Min) / (time[1] - time[0]);
  double max_samples_in_view = 1e4;
  decimation_data.decimation_factor = std::max(samples_in_range / max_samples_in_view, 1.0);
  auto visible_min_it = std::lower_bound(time.begin(), time.end(), x_range_loc.Min);
  decimation_data.visible_min_idx = std::max(static_cast<int>(std::distance(time.begin(), visible_min_it)) - 1, 0);
  auto visible_max_it = std::upper_bound(time.begin(), time.end(), x_range_loc.Max);
  decimation_data.visible_max_idx = static_cast<int>(std::distance(time.begin(), visible_max_it));
}


// Draws labels for drag lines at the given y position.
static void DragLineShowLabel(float y_pos) {
  ImVec2 label_pos = ImPlot::PlotToPixels(ImVec2(v_line_1_pos, 0));
  label_pos.y = y_pos + 25.0f;
  label_pos.x += 5.0f;
  ImGui::GetWindowDrawList()->AddText(label_pos, IM_COL32_WHITE, "1");
  label_pos = ImPlot::PlotToPixels(ImVec2(v_line_2_pos, 0));
  label_pos.y = y_pos + 25.0f;
  label_pos.x += 5.0f;
  ImGui::GetWindowDrawList()->AddText(label_pos, IM_COL32_WHITE, "2");
}


// Draws the cursor data table for each subplot.
static void CursorDataTable(const std::vector<float>& plot_y_pos, float value_column_size) {
  const auto& time = GetData()->time;
  auto vline_1_it = std::upper_bound(time.begin(), time.end(), v_line_1_pos);
  if (vline_1_it != time.begin()) --vline_1_it;
  int vline_1_idx = static_cast<int>(std::distance(time.begin(), vline_1_it));
  auto vline_2_it = std::upper_bound(time.begin(), time.end(), v_line_2_pos);
  if (vline_2_it != time.begin()) --vline_2_it;
  int vline_2_idx = static_cast<int>(std::distance(time.begin(), vline_2_it));

  ImGui::SameLine();
  ImGui::BeginGroup();
  for (size_t i = 0; i < layout::subplots_map.size(); ++i) {
    const auto& subplots_map_i = layout::subplots_map[i];
    ImGui::SetCursorPosY(plot_y_pos[i]);
    ImGui::BeginTable(("Table##" + std::to_string(i)).c_str(), 3);
    ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, value_column_size / 2.5f);
    ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthFixed, (value_column_size - value_column_size / 2.0f) / 2.0f);
    ImGui::TableSetupColumn("Value 2", ImGuiTableColumnFlags_WidthStretch);

    // Header row
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Signal");
    ImGui::SameLine();
    std::string popup_id = "ManagePopup##" + std::to_string(i);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Button("Manage")) {
      ImGui::OpenPopup(popup_id.c_str());
    }
    if (ImGui::BeginPopup(popup_id.c_str())) {
      ManageSubplot(static_cast<int>(i), &(GetData()->signals), &layout::subplots_map);
      ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Value 1");
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("Value 2");

    // Data rows
    for (const auto& [signal_name, is_enabled] : subplots_map_i) {
      if (is_enabled) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", signal_name.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.4f", GetData()->signals[signal_name][vline_1_idx]);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.4f", GetData()->signals[signal_name][vline_2_idx]);
      }
    }
    ImGui::EndTable();
  }
  ImGui::EndGroup();
}


// Main window rendering and event loop.
void MainWindow(ImGuiIO& io) {
  const int kVLine1Base = 100;
  const int kVLine2Base = 200;
  const int id_vline_1 = kVLine1Base;
  const int id_vline_2 = kVLine2Base;
  const float kValueColumnSize = 400.0f;
  const ImPlotSubplotFlags kSubplotFlags = ImPlotSubplotFlags_LinkAllX;
  const size_t subplot_count = layout::subplots_map.size();

  double keep_x_min = 0.0;
  double keep_x_max = 2.0;
  std::vector<float> plot_y_pos;
  DecimationData decimation_data;

  // Set up and start the full-screen window
  ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetCursorPosY() - 10));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("FullScreenWindow", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

  MenuBar();
  CalculateDecimationData(x_range, decimation_data);

  ImPlot::BeginSubplots("", static_cast<int>(subplot_count), 1,
                        ImVec2(io.DisplaySize.x - kValueColumnSize, io.DisplaySize.y - 35),
                        kSubplotFlags);

  if (keep_range) {
    keep_x_min = x_range.Min;
    keep_x_max = x_range.Max;
  }

  for (size_t i = 0; i < subplot_count; ++i) {
    std::string title;
    ImPlotAxisFlags_ x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags_ y_flags = ImPlotAxisFlags_None;
    if (i < subplot_count - 1) {
      x_flags = ImPlotAxisFlags_NoTickLabels;
    }
    if (settings::GetSettings()->auto_size_y) {
      y_flags = ImPlotAxisFlags_AutoFit;
    }
    plot_y_pos.push_back(ImGui::GetCursorPosY());
    if (keep_range) {
      ImPlot::SetNextAxisLimits(ImAxis_X1, keep_x_min, keep_x_max, ImPlotCond_Always);
      x_flags = static_cast<ImPlotAxisFlags_>(x_flags | ImPlotAxisFlags_Lock);
    }
    ImPlot::BeginPlot(title.c_str(), ImVec2(-1, 0), 0);
    ImPlot::SetupLegend(ImPlotLocation_NorthEast);
    ImPlot::SetupAxis(ImAxis_X1, nullptr, x_flags);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, y_flags);
    // Draglines
    ImPlot::DragLineX(id_vline_1, &v_line_1_pos, ImVec4(1, 1, 0, 1), 2.0f);
    ImPlot::DragLineX(id_vline_2, &v_line_2_pos, ImVec4(0.5, 1, 0.5, 1), 2.0f);
    if (i == 0) {
      DragLineShowLabel(plot_y_pos[i]);
    }
    // Plot signals
    for (const auto& [signal_name, is_enabled] : layout::subplots_map[i]) {
      if (is_enabled) {
        std::vector<double> time_dec = Decimate(GetData()->time, decimation_data.visible_min_idx, decimation_data.visible_max_idx,
                                                static_cast<int>(decimation_data.decimation_factor + 0.5f));
        std::vector<double> val = Decimate(GetData()->signals[signal_name], decimation_data.visible_min_idx, decimation_data.visible_max_idx,
                                           static_cast<int>(decimation_data.decimation_factor + 0.5f));
        // replace the first value with the first visible value, and same for last value, so that auto-range works
        if (val.size() > 4) {
            val[0] = val[1];
            val[val.size()-1] = val[val.size()-2];
        }
        ImPlot::PlotStairs(signal_name.c_str(), time_dec.data(), val.data(), val.size());
      }
    }
    x_range = ImPlot::GetPlotLimits().X;
    ImPlot::EndPlot();
  }

  // Reset freeze x range flag so it is only done once
  keep_range = false;

  ImPlot::EndSubplots();
  CursorDataTable(plot_y_pos, kValueColumnSize);
  ImGui::End();
}