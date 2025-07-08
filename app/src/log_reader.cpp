#include "imgui.h"
#include <stdint.h>
#include "ImGuiFileDialog.h"
#include "settings.h"
#include "rapidcsv.h"
#include "log_reader.h"
#include "data_cursors.h"


std::vector<std::unordered_map<std::string, bool>> subplotsMap = {{}};
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

static std::string readAndCleanCSV(const std::string& filePath) {
    std::ifstream file(filePath);
    std::ostringstream cleaned;
    std::string line;

    while (std::getline(file, line)) {
        // Remove trailing separator
        while (!line.empty() && line.back() == GetSettings()->separator[0]) {
            line.pop_back();
        }
        cleaned << line << "\n";
    }

    return cleaned.str();
}

// Custom converter for float with comma as decimal
static float parseCommaDecimal(const std::string& str) {
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

static void readCSV(std::string file)
{
    std::string rawCSV = readAndCleanCSV(file);
    std::stringstream csvStream(rawCSV);

    rapidcsv::Document doc(csvStream, rapidcsv::LabelParams(GetSettings()->header_line_idx, -1), rapidcsv::SeparatorParams(GetSettings()->separator[0]));

    data.signals.clear();
    subplotsMap.clear();// = {{}};
    data.time.clear();

    for (std::string& str : doc.GetColumnNames())
    {
        std::vector<std::string> readStr = doc.GetColumn<std::string>(str);
        if (str == std::string(GetSettings()->time_name))
        {
            for (const auto& val : readStr) data.time.push_back(parseCommaDecimal(val));
        }
        else
        {
            for (const auto& val : readStr) data.signals[str].push_back(parseCommaDecimal(val));
        }
    }


    for (size_t i = 0; i < layout.size(); i++)
    {
        subplotsMap.push_back({});
        for (const auto& [signalName, _] : data.signals) 
        {
            subplotsMap[i][signalName] = 
                std::find(layout[i].begin(), layout[i].end(), signalName) != layout[i].end();
        }
    }

    vLine1Pos = data.time[data.time.size()>>2];
    vLine2Pos = data.time[data.time.size()-(data.time.size()>>2)];

    return;
}

void LogReadButton()
{
    if (ImGui::MenuItem("Open Log"))
    {
        IGFD::FileDialogConfig config;
	    config.path = GetSettings()->file_path;
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", GetSettings()->file_filter, config);
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) 
    {
        if (ImGuiFileDialog::Instance()->IsOk()) 
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            readCSV(filePathName);
        }
                
        // close
        ImGuiFileDialog::Instance()->Close();
    }
    return;
}