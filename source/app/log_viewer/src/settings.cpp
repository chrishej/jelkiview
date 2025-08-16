#include "settings.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "imgui.h"
#include "json.hpp"

using Json = nlohmann::json;

namespace settings {

namespace {
Settings settings = {
    // NOLINT(cert-err58-cpp,fuchsia-statically-constructed-objects,cppcoreguidelines-avoid-non-const-global-variables)
    .file_path = "./example_logs",  // file_path
    .header_line_idx = 0,           // header_line_idx
    .time_name = "time",            // time_name
    .separator = ",",               // separator
    .auto_size_y = false,           // auto_size_y
    .cursor_value_table_size = 400  // obvi
};

std::string settings_path = "resources/settings.json";

void SaveSettings() {
    Json settings_out;

    settings_out["time_name"] = std::string(settings.time_name);
    settings_out["separator"] = std::string(settings.separator);
    settings_out["file_path"] = std::string(settings.file_path);

    settings_out["header_line_idx"] = settings.header_line_idx;
    settings_out["auto_size_y"] = settings.auto_size_y;

    settings_out["cursor_value_table_size"] = settings.cursor_value_table_size;

    std::ofstream out(settings_path);
    out << settings_out.dump(4);
    out.close();
}
}  // anonymous namespace

Settings const* GetSettings() { return &settings; }

void init() {
    if (!std::filesystem::exists(settings_path)) {
        settings::SaveSettings();
    } else {
        std::ifstream settings_file(settings_path);
        Json settings_json = Json::parse(settings_file);  // NOLINT(misc-const-correctness)

        settings.header_line_idx = settings_json["header_line_idx"].get<uint8_t>();
        if (settings_json.contains("cursor_value_table_size")) {
            settings.cursor_value_table_size = settings_json["cursor_value_table_size"].get<int>();
        }
        settings.auto_size_y     = settings_json["auto_size_y"].get<bool>();

        std::string temp;
        temp = settings_json["time_name"].get<std::string>();
        std::strncpy(settings.time_name, temp.c_str(), kSettingsCharBufLen);
        temp = settings_json["separator"].get<std::string>();
        std::strncpy(settings.separator, temp.c_str(), kSettingsCharBufLen);

        settings.file_path = settings_json["file_path"].get<std::string>();
    }
}

void SetLogFilePath(std::string path) {
    settings.file_path = std::move(path);
    settings::SaveSettings();
}

void ShowSettingsButton() {
    static bool show_settings_window = false;
    static bool save_settings = false;
    const float width = 80.0F;

    if (ImGui::MenuItem("Settings")) {
        show_settings_window = true;
    }

    if (show_settings_window) {
        save_settings = true;
        ImGui::Begin("Settings", &show_settings_window);  // Close button toggles flag
        if (ImGui::CollapsingHeader("Log Import")) {
            uint8_t step = 1;
            ImGui::TextUnformatted("Header Line in log at row: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(width);
            ImGui::InputScalar("##hidden_label", ImGuiDataType_S8, &(settings.header_line_idx),
                               &step);

            ImGui::TextUnformatted("Time column name: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(width);
            ImGui::InputText("##hidden_label1", settings.time_name,
                             IM_ARRAYSIZE(settings.time_name));

            ImGui::TextUnformatted("CSV Separator: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(width);
            ImGui::InputText("##hidden_label2", settings.separator,
                             IM_ARRAYSIZE(settings.separator));
        }

        if (ImGui::CollapsingHeader("Plot Settings")) {
            ImGui::Checkbox("Auto size y-axis", &(settings.auto_size_y));

            ImGui::Text("Cursor table size");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0F);
            ImGui::InputInt("##cursor_table_size", &(settings.cursor_value_table_size), 10, 20);
        }

        ImGui::End();
    }

    if (save_settings && !show_settings_window) {
        save_settings = false;
        settings::SaveSettings();
    }
}
}  // namespace settings