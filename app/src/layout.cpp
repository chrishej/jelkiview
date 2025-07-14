#include "layout.h"
#include "log_reader.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <iostream>

namespace layout {
std::vector<std::unordered_map<std::string, bool>> subplots_map = {{}};
std::vector<std::vector<std::string>> layout = {{}};

void SetMapToLayout(){
    for (size_t i = 0; i < layout::layout.size(); i++) {
        layout::subplots_map.emplace_back();
        for (const auto& [signalName, dummy] : GetData()->signals) {
            layout::subplots_map[i][signalName] =
                std::find(layout::layout[i].begin(), layout::layout[i].end(), signalName) != layout::layout[i].end();
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


void SaveLayoutButton() {
    if (ImGui::MenuItem("Save Layout")) {
        IGFD::FileDialogConfig config;
        config.path = "./resources/layouts/";
        config.flags = ImGuiFileDialogFlags_Modal;
        const char* filters = ".json";
        ImGuiFileDialog::Instance()->OpenDialog("savelayout", "Choose File", filters, config);
    }
    if (ImGuiFileDialog::Instance()->Display("savelayout")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string const file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();
            std::cout << file_path_name << "\n";
        }
        // close
        ImGuiFileDialog::Instance()->Close();
    }
}

}