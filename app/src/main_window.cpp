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
#include "licenses.h"
#include "log_reader.h"
#include "math.h"
#include "rapidcsv.h"
#include "settings.h"
#include "layout.h"

std::vector<double> Decimate(const std::vector<double>& input, int time_min_idx, int time_max_idx,
                             int m);
void ManageSubplot(int subplot_index,
                   std::unordered_map<std::string, std::vector<double>>* const signals,
                   std::vector<std::unordered_map<std::string, bool>>* subplots_map);

std::vector<double> Decimate(const std::vector<double>& input, int time_min_idx, int time_max_idx,
                             int m) {
    std::vector<double> result;
    result.push_back(input[0]);

    size_t i;
    for (i = std::max(time_min_idx, 1); i < std::min(input.size() - 1, (size_t)time_max_idx);
         i += m) {
        result.push_back(input[i]);
    }

    if (i <= input.size() - 1) {
        result.push_back(input[input.size() - 1]);
    }

    return result;
}

void ManageSubplot(int subplot_index,
                   std::unordered_map<std::string, std::vector<double>>* const signals,
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
        if (subplot_index >= 0 && (size_t)subplot_index < subplots_map_loc->size()) {
            subplots_map_loc->erase(subplots_map_loc->begin() + subplot_index);
        }
    }

    if (ImGui::Button("Insert Subplot")) {
        if (subplot_index >= 0 && (size_t)subplot_index < subplots_map_loc->size()) {
            auto insert_at_it = subplots_map_loc->begin() + subplot_index + 1;
            int new_index = std::distance(subplots_map_loc->begin(), insert_at_it);

            subplots_map_loc->insert(insert_at_it, std::unordered_map<std::string, bool>{});

            for (const auto& [signal_name, _] : (*signals)) {
                (*subplots_map_loc)[new_index][signal_name] = false;
            }
        }
    }
    return;
}

void MainWindow(ImGuiIO& io) {
    static ImPlotRange x_range;

    const int id_vline_1_base = 100;
    const int id_vline_2_base = 200;
    const int id_vline_1 = id_vline_1_base;
    const int id_vline_2 = id_vline_2_base;

    ImGui::BeginMainMenuBar();
    LogReadButton();
    // settingsButton();
    settings::ShowSettingsButton();

    if (ImGui::BeginMenu("Layout")) {
        layout::LayoutMenuButton();
        ImGui::EndMenu();
    }
    layout::GuiUpdate();

    Licenses();
    ImGui::EndMainMenuBar();

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetCursorPosY() - 10));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("FullScreenWindow", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

    float const value_column_size = 400;

    double samples_in_range =
        (x_range.Max - x_range.Min) / (GetData()->time[1] - GetData()->time[0]);
    double max_samples_in_view = 10e3;
    double decimation_factor = std::max(samples_in_range / max_samples_in_view, 1.0);
    auto visible_min_it =
        std::lower_bound(GetData()->time.begin(), GetData()->time.end(), x_range.Min);
    int visible_min_idx = std::distance(GetData()->time.begin(), visible_min_it);
    visible_min_idx = std::max(visible_min_idx - 1, 0);
    auto visible_max_it =
        std::upper_bound(GetData()->time.begin(), GetData()->time.end(), x_range.Max);
    int visible_max_idx = std::distance(GetData()->time.begin(), visible_max_it);
    // ImGui::Text("X Min: %.2f, X Max: %.2f, minIdx: %d, maxIdx: %d, decimationFac: %.2f",
    // x_range.Min, x_range.Max, visible_min_idx, visible_max_idx, decimation_factor);

    ImPlotSubplotFlags const subplot_flags = ImPlotSubplotFlags_LinkAllX;
    uint8_t const len = layout::subplots_map.size();
    ImPlot::BeginSubplots("", len, 1,
                          ImVec2(io.DisplaySize.x - value_column_size, io.DisplaySize.y - 35),
                          subplot_flags);

    std::vector<float> plot_y_pos;
    for (int i = 0; i < len; ++i) {
        std::string title = "";  //"Plot " + std::to_string(i);

        ImPlotAxisFlags_ x_flags = ImPlotAxisFlags_None;
        ImPlotAxisFlags_ y_flags = ImPlotAxisFlags_None;
        if (i < len - 1) {
            x_flags = ImPlotAxisFlags_NoTickLabels;
        }

        if (settings::GetSettings()->auto_size_y) {
            y_flags = ImPlotAxisFlags_AutoFit;
        }

        plot_y_pos.push_back(ImGui::GetCursorPosY());

        ImPlot::BeginPlot(title.c_str(), ImVec2(-1, 0), 0);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxis(ImAxis_X1, nullptr, x_flags);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, y_flags);

        ImPlot::DragLineX(id_vline_1, &v_line_1_pos, ImVec4(1, 1, 0, 1), 2.0f);
        ImPlot::DragLineX(id_vline_2, &v_line_2_pos, ImVec4(0.5, 1, 0.5, 1), 2.0f);

        if (i == 0) {
            ImVec2 label_pos =
                ImPlot::PlotToPixels(ImVec2(v_line_1_pos, 0));  // y doesn't matter for label
            label_pos.y = plot_y_pos[0] + 15 + 10.0f;           // offset label above the line
            label_pos.x += 5.0f;
            ImGui::GetWindowDrawList()->AddText(label_pos, IM_COL32_WHITE, "1");
            label_pos =
                ImPlot::PlotToPixels(ImVec2(v_line_2_pos, 0));  // y doesn't matter for label
            label_pos.y = plot_y_pos[0] + 15 + 10.0f;           // offset label above the line
            label_pos.x += 5.0f;
            ImGui::GetWindowDrawList()->AddText(label_pos, IM_COL32_WHITE, "2");
        }

        // for (auto signal_nameIt = subplots[i].begin(); signal_nameIt != subplots[i].end();
        // signal_nameIt++)
        for (auto& [signal_name, is_enabled] : layout::subplots_map[i]) {
            if (is_enabled) {
                // ImPlot::PlotStairs((*signal_nameIt).c_str(), time.data(),
                // signals[*signal_nameIt].data(), samples); std::vector<double> time_dec =
                // Decimate(timeVec, timeVec, x_range.Min, x_range.Max,
                // (int)(decimation_factor+0.5f)); std::vector<double> val     =
                // Decimate(signals[signal_name], timeVec, x_range.Min, x_range.Max,
                // (int)(decimation_factor+0.5f));
                std::vector<double> time_dec =
                    Decimate(GetData()->time, visible_min_idx, visible_max_idx,
                             (int)(decimation_factor + 0.5f));
                std::vector<double> val =
                    Decimate(GetData()->signals[signal_name], visible_min_idx, visible_max_idx,
                             (int)(decimation_factor + 0.5f));
                // std::vector<double> time_dec = GetData()->time;
                // std::vector<double> val     = signals[signal_name];
                ImPlot::PlotStairs(signal_name.c_str(), time_dec.data(), val.data(), val.size());
            }
        }

        // ImPlot::PlotStairs("", time, signals[subplots[i][0]], samples);
        // ImPlot::PlotStairs("", time, signalValues[(i+1)&0x1], samples);
        x_range = ImPlot::GetPlotLimits().X;
        ImPlot::EndPlot();
    }

    ImPlot::EndSubplots();

    auto vline_1_it =
        std::upper_bound(GetData()->time.begin(), GetData()->time.end(), v_line_1_pos);
    if (vline_1_it != GetData()->time.begin()) --vline_1_it;
    int vline_1_idx = std::distance(GetData()->time.begin(), vline_1_it);

    auto vline_2_it =
        std::upper_bound(GetData()->time.begin(), GetData()->time.end(), v_line_2_pos);
    if (vline_2_it != GetData()->time.begin()) --vline_2_it;
    int vline_2_idx = std::distance(GetData()->time.begin(), vline_2_it);

    ImGui::SameLine();
    ImGui::BeginGroup();
    for (int i = 0; (size_t)i < layout::subplots_map.size(); ++i) {
        std::unordered_map<std::string, bool> subplots_map_i = layout::subplots_map[i];
        ImGui::SetCursorPosY(plot_y_pos[i]);
        ImGui::BeginTable(("Table##" + std::to_string(i)).c_str(), 3);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed,
                                value_column_size / 2.5f);
        ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthFixed,
                                (value_column_size - value_column_size / 2.0f) / 2.0f);
        ImGui::TableSetupColumn("Value 2",
                                ImGuiTableColumnFlags_WidthStretch);  // fills remaining space

        // Header row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Signal");

        ImGui::SameLine();
        std::string popup_id = "ManagePopup##" + std::to_string(i);
        ImGui::PushID(i);
        if (ImGui::Button("Manage")) {
            ImGui::OpenPopup(popup_id.c_str());
        }
        if (ImGui::BeginPopup(popup_id.c_str())) {
            // ImGui::Text("Managing subplot %d", i);
            ManageSubplot(i, &(GetData()->signals), &layout::subplots_map);
            ImGui::EndPopup();
        }
        ImGui::PopID();

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Value 1");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Value 2");

        // Data rows
        // for (const std::string& name : subplots[i])
        for (auto& [signal_name, is_enabled] : subplots_map_i) {
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

    ImGui::End();
}