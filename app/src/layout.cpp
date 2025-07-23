#include "layout.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "json.hpp"
#include "log_reader.h"

using Json = nlohmann::json;

namespace layout {
std::vector<std::unordered_map<std::string, bool>> subplots_map = {{}};
std::vector<std::vector<std::string>> layout = {{}};

namespace {
bool should_open_save_dialog = false;
bool should_open_open_dialog = false;

void Save();
void Open();
void SetLayout(const std::string& file_path_name);
}  // namespace

void SetMapToLayout() {
    subplots_map.clear();
    for (size_t i = 0; i < layout::layout.size(); i++) {
        layout::subplots_map.emplace_back();
        for (const auto& [signalName, dummy] : GetData()->signals) {
            layout::subplots_map[i][signalName] =
                std::find(layout::layout[i].begin(), layout::layout[i].end(), signalName) !=
                layout::layout[i].end();
        }
    }
}

void UpdateLayout() {
    layout::layout.clear();

    for (size_t i = 0; i < layout::subplots_map.size(); i++) {
        layout::layout.push_back({});
        for (const auto& [signal_name, is_enabled] : layout::subplots_map[i]) {
            if (is_enabled) {
                layout::layout[i].push_back(signal_name);
            }
        }
    }
}

void LayoutMenuButton() {
    if (ImGui::MenuItem("Save Layout")) {
        should_open_save_dialog = true;
    }
    if (ImGui::MenuItem("Open Layout")) {
        should_open_open_dialog = true;
    }
}

void GuiUpdate() {
    Save();
    Open();
}

namespace {
void SetLayout(const std::string& file_path_name) {
    // Open file and parse JSON
    std::ifstream in_file(file_path_name);
    if (!in_file.is_open()) {
        std::cerr << "Failed to open layout file: " << file_path_name << "\n";
        return;
    }

    Json jsn;
    try {
        in_file >> jsn;

        // Assuming you want to load into this structure:
        layout = jsn.get<std::vector<std::vector<std::string>>>();

        SetMapToLayout();

    } catch (const Json::exception& e) {
        std::cerr << "JSON error: " << e.what() << "\n";
    }
}

void Open() {
    if (should_open_open_dialog) {
        IGFD::FileDialogConfig config;
        config.path = "./resources/layouts/";
        config.flags = ImGuiFileDialogFlags_Modal;
        const char* filters = ".json";
        ImGuiFileDialog::Instance()->OpenDialog("openlayout", "Choose File", filters, config);
        should_open_open_dialog = false;
    }

    if (ImGuiFileDialog::Instance()->Display("openlayout")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string const file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();

            std::cout << file_path_name << "\n";
            SetLayout(file_path_name);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void Save() {
    if (should_open_save_dialog) {
        IGFD::FileDialogConfig config;
        config.path = "./resources/layouts/";
        config.flags = ImGuiFileDialogFlags_Modal;
        const char* filters = ".json";
        ImGuiFileDialog::Instance()->OpenDialog("savelayout", "Choose File", filters, config);
        should_open_save_dialog = false;
    }

    if (ImGuiFileDialog::Instance()->Display("savelayout")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string const file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();

            Json const jsn = layout;
            // close
            ImGuiFileDialog::Instance()->Close();
            // Save to file
            std::ofstream out(file_path_name);
            out << jsn.dump(4);  // Pretty-print with 4-space indentation
            out.close();
        }
        ImGuiFileDialog::Instance()->Close();
    }
}
}  // namespace

}  // namespace layout