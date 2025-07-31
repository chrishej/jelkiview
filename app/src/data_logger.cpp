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

#include "log_reader.h"
#include "serial_back.h"
#include "serial_front.h"

namespace {
    std::unordered_map<std::string, std::vector<double>> log_variables;
    std::string log_file_path;
    std::mutex log_mutex;
    float base_time = 0.0F;

    std::unordered_map<std::string, std::vector<double>> DeepCopyLogVariables() {
        std::unordered_map<std::string, std::vector<double>> copy;
        const std::lock_guard<std::mutex> lock(log_mutex);
        for (const auto& var : log_variables) {
            copy[var.first] = var.second;
        }
        return copy;
    }

    /*
     * This function performs type casting based on the variable name's postfix.
     * It converts from a raw uint32_t value received from the CU to a double value.
     * Note that the rx_byte is a uint32_t, but this is only to be able to put all types in the same container.
     * The actual value recieved into the uint32_t might be a uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t or float.
     * By looking at the variable postfix, we can determine how to cast the value correctly.
     * If no postfix is found, it defaults to uint32_t. 
     */
    double PerformTypeCast(uint32_t rx_val, const std::string& variable_name) {
        double value;
        const std::regex regex(".*_([A-Za-z0-9]+)$");
        std::smatch match;
        if (std::regex_match(variable_name, match, regex)) {
            std::string post_fix = match[1];

            // Lower case transform
            std::transform(post_fix.begin(), post_fix.end(), post_fix.begin(),
                [](unsigned char chr){ return std::tolower(chr); });

            if (post_fix == "s08") {
                value = static_cast<double>(static_cast<int8_t>(static_cast<uint8_t>(rx_val)));
            } else if (post_fix == "u08") {
                value = static_cast<double>(static_cast<uint8_t>(rx_val));
            } else if (post_fix == "s16") {
                value = static_cast<double>(static_cast<int16_t>(static_cast<uint16_t>(rx_val)));
            } else if (post_fix == "u16") {
                value = static_cast<double>(static_cast<uint16_t>(rx_val));
            } else if (post_fix == "s32") {
                value = static_cast<double>(static_cast<int32_t>(rx_val));
            } else if (post_fix == "u32") {
                value = static_cast<double>(rx_val);
            } else if (post_fix == "f32") {
                value = static_cast<double>(rx_val);
            } else {
                value = static_cast<double>(rx_val);
            }
        } else {
            value = static_cast<double>(rx_val);
        }
        return value;
    }
    }  // namespace

namespace data_logger {
void init(const std::unordered_map<std::string, VarStruct>& variables) {
    log_variables.clear();
    log_variables["Time"] = std::vector<double>();
    // Set up log variables based on the provided variables.
    // Streaming to trace window needs to know at init which variables to log.
    for (const auto& var : variables) {
        log_variables[var.first] = std::vector<double>();
    }

    auto time = std::time(nullptr);
    auto time_local = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&time_local, "%Y-%m-%d_%H'%M''%S");
    log_file_path = "logs/" + oss.str() + ".csv";

    InitSerialStream(variables);
}

    std::unordered_map<std::string, std::vector<double>> GetLogVariables() {
        // This causes a deepcopy, and issues with concurrent access are avoided by using a mutex.
        // TODO(chejd): fix this so it is better.
        std::lock_guard<std::mutex> const lock(log_mutex);
        return log_variables;
    }

    void LogFrame(const FrameStruct& frame) {
        std::lock_guard<std::mutex> const lock(log_mutex);
        const float ms_to_sec = 1000.0F;
        if (log_variables["Time"].empty()) {
            // The time stamp from CU is probably a free running timer, sp the first frame will most
            // likely not start at 0.
            // TODO(chejd): time scaling from some config with CU
            base_time = static_cast<float>(frame.latest_timestamp) / ms_to_sec;  // Convert to seconds
        }
        log_variables["Time"].push_back((static_cast<float>(frame.latest_timestamp) / ms_to_sec) -
                                        base_time);  // Convert to seconds

        for (const auto& var : frame.variables) {
            double const value = PerformTypeCast(var.latest_rx, var.name);
            log_variables[var.name].push_back(value);
        }
    }

    /*
     * Saves log data to a CSV file.
     */
    void SaveLog() {
        // TODO(chejd): currently writes all data in a batch. Make it so it can write data
        // incrementally so it can be done continuously while logging. Do a deepcopy of
        // log_variables to avoid issues with concurrent access
        std::unordered_map<std::string, std::vector<double>> log_variables_copy = DeepCopyLogVariables();

        std::ofstream log_file(log_file_path);
        if (!log_file.is_open()) {
            serial_front::AddLog("%s ERROR: Could not open log file %s\n", ERROR_CHAR, log_file_path.c_str());
            return;
        }

        for (const auto& var : log_variables_copy) {
            log_file << var.first << ",";
        }
        log_file << "\n";

        // Write data
        size_t const num_rows = log_variables_copy.begin()->second.size();
        for (size_t i = 0; i < num_rows; ++i) {
            for (const auto& var : log_variables_copy) {
                log_file << var.second[i];
                // No comma for last variable column
                if (i < num_rows - 1) {
                    log_file << ",";
                }
            }
            log_file << "\n";
        }

        log_file.close();
    }
} // namespace data_logger