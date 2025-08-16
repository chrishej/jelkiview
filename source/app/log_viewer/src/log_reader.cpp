#include "log_reader.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ImGuiFileDialog.h"
#include "data_cursors.h"
#include "imgui.h"
#include "rapidcsv.h"
#include "settings.h"
#include "layout.h"
#include "data_logger.h"
#include "serial_back.h"
#include "data_logger.h"

namespace {
Data data = {
    .time = {0},    // time
    .signals = {},  // signals
};

LogSource log_source = LOG_SOURCE_CSV;

std::string ReadAndCleanCSV(const std::string& file_path) {
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
float ParseCommaDecimal(const std::string& str) {
    float float_val;
    std::string modified;

    modified = str;
    std::replace(modified.begin(), modified.end(), ',', '.');  // NOLINT(modernize-use-ranges)

    try {
        float_val = std::stof(modified);
    } catch (const std::exception& e) {
        float_val = std::numeric_limits<float>::quiet_NaN();
    }

    return float_val;
}

void ReadCSV(std::string const& file) {
    log_source = LOG_SOURCE_CSV;
    std::string const raw_csv = ReadAndCleanCSV(file);
    std::stringstream csv_stream(raw_csv);

    rapidcsv::Document const doc(
        csv_stream, rapidcsv::LabelParams(settings::GetSettings()->header_line_idx, -1),
        rapidcsv::SeparatorParams(settings::GetSettings()->separator[0]));

    data.signals.clear();
    layout::subplots_map.clear();
    data.time.clear();

    for (std::string const& str : doc.GetColumnNames()) {
        std::vector<std::string> const read_str = doc.GetColumn<std::string>(str);

        if (str == std::string(settings::GetSettings()->time_name)) {
            for (const auto& val : read_str) {
                data.time.push_back(ParseCommaDecimal(val));
            }
        } else {
            for (const auto& val : read_str) {
                data.signals[str].push_back(ParseCommaDecimal(val));
            }
        }
    }

   layout::SetMapToLayout();

    v_line_1_pos = data.time[data.time.size() >> 2];
    v_line_2_pos = data.time[data.time.size() - (data.time.size() >> 2)];
}

void GetStreamingData() {
    std::unordered_map<std::string, std::vector<double>> log = 
        data_logger::GetLogVariables();

    data.time = log["Time"];
    data.signals.clear();
    for (const auto& [var_name, var_vec] : log) {
        if (var_name != "Time") {
            data.signals[var_name] = log[var_name];
        }
    }
}
}  // anonymous namespace

Data* GetData() {
    if (log_source == LOG_SOURCE_SERIAL) {
        GetStreamingData();
    }
    return &data; 
}

LogSource GetLogSource() {
    return log_source;
}

void ClearData() {
    data.signals.clear();
    data.time.clear();
}

void InitSerialStream(std::unordered_map<std::string, VarStruct> log_variables) {
    log_source = LOG_SOURCE_SERIAL;
    ClearData();
    layout::subplots_map.clear();

    data.time = std::vector<double>();
    for (const auto& [var_name, var_struct] : log_variables) {
        if (var_name != "Time") {
            data.signals[var_name] = std::vector<double>();
        }
    }

    layout::SetMapToLayout();
}

void LogReadButton() {
    if (ImGui::MenuItem("Open Log")) {
        IGFD::FileDialogConfig config;
        config.path = settings::GetSettings()->file_path;
        config.flags = ImGuiFileDialogFlags_Modal;
        const char* filters = "All Files{.*}";
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", filters, config);
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string const file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();
            settings::SetLogFilePath(ImGuiFileDialog::Instance()->GetCurrentPath());
            ReadCSV(file_path_name);
        }

        // close
        ImGuiFileDialog::Instance()->Close();
    }
}