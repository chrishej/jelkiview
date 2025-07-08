#include "imgui.h"
#include <stdint.h>
#include "ImGuiFileDialog.h"
#include "settings.h"

static Settings settings = {
    "./example_logs",   // file_path
    3,                  // header_line_idx
    "Time",             // time_name
    ";",                // separator
    ".*,.csv,.txt",     // file_filter
    false               // auto_size_y
};

Settings const * GetSettings(void)
{
    return &settings;
}

void ShowSettingsButton(void)
{
    static bool showSettingsWindow = false;

    if (ImGui::MenuItem("Settings"))
    {
        showSettingsWindow = true;
    }

    if (showSettingsWindow)
    {
        ImGui::Begin("Settings", &showSettingsWindow);  // Close button toggles flag
        if (ImGui::CollapsingHeader("Log Import"))
        {
            uint8_t step = 1;
            ImGui::TextUnformatted("Header Line in log at row: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputScalar("##hidden_label", ImGuiDataType_S8, &(settings.header_line_idx), &step);

            ImGui::TextUnformatted("Time column name: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputText("##hidden_label1", settings.time_name, IM_ARRAYSIZE(settings.time_name));

            ImGui::TextUnformatted("CSV Separator: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputText("##hidden_label2", settings.separator, IM_ARRAYSIZE(settings.separator));

            ImGui::TextUnformatted("File filter: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputText("##hidden_label3", settings.file_filter, IM_ARRAYSIZE(settings.file_filter));

            if (ImGui::MenuItem("Choose default log path"))
            {
                IGFD::FileDialogConfig config;
	            config.path = ".";
                config.flags = ImGuiFileDialogFlags_Modal;
                ImGuiFileDialog::Instance()->OpenDialog("ChooseDirDlgKey", "Choose Directory", nullptr, config);
            }
            if (ImGuiFileDialog::Instance()->Display("ChooseDirDlgKey")) 
            {
                if (ImGuiFileDialog::Instance()->IsOk()) 
                {
                    settings.file_path = ImGuiFileDialog::Instance()->GetCurrentPath();
                }
                
                // close
                ImGuiFileDialog::Instance()->Close();
            }
        }

        if (ImGui::CollapsingHeader("Plot Settings"))
        {
            ImGui::Checkbox("Auto size y-axis", &(settings.auto_size_y));
        }

        ImGui::End();
    }

    return;
}