#include "data_logger.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <filesystem>

#include "log_reader.h"
#include "serial_back.h"
#include "serial_front.h"
#include "performance_analysis.h"

namespace {
Data log_data;
std::string log_file_path;
std::mutex log_mutex;
float base_time = 0.0F;

Data DeepCopyLogVariables() {
    Data copy;
    const std::lock_guard<std::mutex> lock(log_mutex);
    copy.time = log_data.time;
    copy.signals = log_data.signals;
    return copy;
}

double TypeCast(uint32_t rx_val, VariableType type) {
    switch (type) {
        case VariableType::TYPE_UINT8:
            return static_cast<double>(static_cast<uint8_t>(rx_val));
        case VariableType::TYPE_UINT16:
            return static_cast<double>(static_cast<uint16_t>(rx_val));
        case VariableType::TYPE_UINT32:
            return static_cast<double>(rx_val);
        case VariableType::TYPE_INT8:
            return static_cast<double>(static_cast<int8_t>(rx_val));
        case VariableType::TYPE_INT16:
            return static_cast<double>(static_cast<int16_t>(rx_val));
        case VariableType::TYPE_INT32:
            return static_cast<double>(static_cast<int32_t>(rx_val));
        case VariableType::TYPE_FLOAT:
            return static_cast<double>(*reinterpret_cast<float*>(&rx_val));
        case VariableType::TYPE_DOUBLE:
            return static_cast<double>(*reinterpret_cast<double*>(&rx_val));
        default:
            return static_cast<double>(rx_val); // Default for unknown types
    }
}

}  // namespace

namespace data_logger {
void init(const std::unordered_map<std::string, VarStruct>& variables) {
    log_data.time.clear();
    log_data.signals.clear();
    // Set up log variables based on the provided variables.
    // Streaming to trace window needs to know at init which variables to log.
    for (const auto& var : variables) {
        log_data.signals[var.first] = std::vector<double>();
    }

    auto time = std::time(nullptr);
    auto time_local = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&time_local, "%Y-%m-%d_%H'%M''%S");
    log_file_path = "logs/" + oss.str() + ".csv";

    InitSerialStream(variables);
}

Data* GetLogData() {
    return &log_data;
}

void LogFrame(const FrameStruct& frame) {
    performance_analysis::Start(performance_analysis::AnalysisIndex::FUNC_LOG_FRAME);
    std::lock_guard<std::mutex> const lock(log_mutex);
    const float us_to_sec = 1e6F;

    if (log_data.time.empty()) {
        // The time stamp from CU is probably a free running timer, sp the first frame will most
        // likely not start at 0.
        // TODO(chejd): time scaling from some config with CU
        base_time = static_cast<float>(frame.latest_timestamp) / us_to_sec;  // Convert to seconds
    }

    float log_time = (static_cast<float>(frame.latest_timestamp) / us_to_sec) - base_time;
    // If the log is empty, set first value for all variables to 0.0
    // Else if the time is new, append new time and copy the last value for all variables
    if (log_data.time.empty()) {
        log_data.time.push_back(log_time);
        for (auto& [name, vec] : log_data.signals) {
            vec.push_back(0.0);
        }
    } else if (log_time > log_data.time.back()) {
        log_data.time.push_back(log_time);
        for (auto& [name, vec] : log_data.signals) {
            vec.push_back(vec.back());
        }
    }

    // Replace the last value for variables in frame
    for (const auto& var : frame.variables) {
        double const value = TypeCast(var.latest_rx, var.type);
        auto it = log_data.signals.find(var.name);
        if (it != log_data.signals.end() && !it->second.empty()) {
            it->second.back() = value;
        }
    }

    performance_analysis::End(performance_analysis::AnalysisIndex::FUNC_LOG_FRAME);
}

/*
 * Saves log data to a CSV file.
 */
void SaveLog() {
    // TODO(chejd): currently writes all data in a batch. Make it so it can write data
    // incrementally so it can be done continuously while logging. Do a deepcopy of
    // log_variables to avoid issues with concurrent access
    Data log_variables_copy = DeepCopyLogVariables();

    // Check if path exists, if not create it
    if (!std::filesystem::exists("logs")) {
        std::filesystem::create_directory("logs");
    }

    std::ofstream log_file(log_file_path);
    if (!log_file.is_open()) {
        serial_front::AddLog("%s ERROR: Could not open log file %s\n", ERROR_CHAR,
                             log_file_path.c_str());
        return;
    }

    log_file << "Time,";
    for (const auto& var : log_variables_copy.signals) {
        log_file << var.first << ",";
    }
    log_file << "\n";

    // Write data
    size_t const num_rows = log_variables_copy.signals.begin()->second.size();
    for (size_t i = 0; i < num_rows; ++i) {
        log_file << log_variables_copy.time[i] << ",";
        for (const auto& var : log_variables_copy.signals) {
            log_file << var.second[i];
            // No comma for last variable column
            //if (i < num_rows - 1) {
                log_file << ",";
            //}
        }
        log_file << "\n";
    }

    log_file.close();
}
}  // namespace data_logger