#include "imgui.h"
#include <stdint.h>
#include "ImGuiFileDialog.h"
#include "settings.h"
#include "rapidcsv.h"
#include "log_reader.h"
#include "data_cursors.h"
#include <iostream>


std::vector<std::unordered_map<std::string, bool>> subplots_map = {{}};
std::vector< std::vector<std::string> > layout = {{}};

static Data data = {
    {0},  // time
    {}  // signals
};

Data * GetData(void)
{
    return &data;
}

void ClearData(void)
{
    data.signals.clear();
    data.time.clear();
    return;
}

static std::string ReadAndCleanCSV(const std::string& file_path) {
    std::ifstream file(file_path);
    std::ostringstream cleaned;
    std::string line;

    while (std::getline(file, line)) {
        // Remove trailing separator
        while (!line.empty() && line.back() == settings::GetSettings()->separator[0]) {
            line.pop_back();
        }
        cleaned << line << "\n";
    }

    return cleaned.str();
}

// Custom converter for float with comma as decimal
static float ParseCommaDecimal(const std::string& str) {
    float float_val;
    std::string modified;

    modified = str;
    std::replace(modified.begin(), modified.end(), ',', '.');

    try
    {
        float_val = std::stof(modified);
    }
    catch(const std::exception& e)
    {
        float_val = std::numeric_limits<float>::quiet_NaN();
    }
    
    return float_val;
}

static void ReadCSV(std::string file)
{
    std::string raw_csv = ReadAndCleanCSV(file);
    std::stringstream csv_stream(raw_csv);

    rapidcsv::Document doc(csv_stream, rapidcsv::LabelParams(settings::GetSettings()->header_line_idx, -1), rapidcsv::SeparatorParams(settings::GetSettings()->separator[0]));

    data.signals.clear();
    subplots_map.clear();// = {{}};
    data.time.clear();

    for (std::string& str : doc.GetColumnNames())
    {
        std::vector<std::string> read_str = doc.GetColumn<std::string>(str);
        if (str == std::string(settings::GetSettings()->time_name))
        {
            for (const auto& val : read_str) data.time.push_back(ParseCommaDecimal(val));
        }
        else
        {
            for (const auto& val : read_str) data.signals[str].push_back(ParseCommaDecimal(val));
        }
    }


    for (size_t i = 0; i < layout.size(); i++)
    {
        subplots_map.push_back({});
        for (const auto& [signalName, _] : data.signals) 
        {
            subplots_map[i][signalName] = 
                std::find(layout[i].begin(), layout[i].end(), signalName) != layout[i].end();
        }
    }

    v_line_1_pos = data.time[data.time.size()>>2];
    v_line_2_pos = data.time[data.time.size()-(data.time.size()>>2)];

    return;
}

void LogReadButton()
{
    if (ImGui::MenuItem("Open Log"))
    {
        IGFD::FileDialogConfig config;
	    config.path = settings::GetSettings()->file_path;
        config.flags = ImGuiFileDialogFlags_Modal;
        const char* filters = "All Files{.*}";
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", filters, config);
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) 
    {
        if (ImGuiFileDialog::Instance()->IsOk()) 
        {
            std::string file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();
            std::cout << ImGuiFileDialog::Instance()->GetCurrentPath() << "\n";
            settings::SetLogFilePath(ImGuiFileDialog::Instance()->GetCurrentPath());
            ReadCSV(file_path_name);
        }
                
        // close
        ImGuiFileDialog::Instance()->Close();
    }
    return;
}