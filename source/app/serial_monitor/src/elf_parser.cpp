#include "elf_parser.h"

#include "serial_front.h"
#include "serial_back.h"

#include <filesystem>
#include <array>
#include <thread>
#include <regex>
#include <fstream>
#include <iostream>

namespace {
    typedef struct {
        std::string     file;
        VariableType    type;
        std::string     raw_line;
    } SourceFileInfo;
    std::vector<std::string> variable_types = {
        "uint8_t", 
        "uint16_t", 
        "uint32_t", 
        "int8_t", 
        "int16_t",
        "int32_t",
        "float", 
        "double", 
        "char", 
        "bool"
    };

    bool CheckSettings(const SerialBack_Settings& settings) {
        // Check if elf file path is set
        if (settings.elf_file_path.empty()) {
            serial_front::AddLog("%s ERROR: ELF file path is not set.\n", ERROR_CHAR);
            return false;
        }
        // check elf file exists
        if (!std::filesystem::exists(settings.elf_file_path)) {
            serial_front::AddLog("%s ERROR: ELF file %s does not exist.\n", ERROR_CHAR, settings.elf_file_path.c_str());
            return false;
        }
        // Check if nm is set
        if (settings.nm.empty()) {
            serial_front::AddLog("%s ERROR: nm command is not set.\n", ERROR_CHAR);
            return false;
        }
        // check if nm exists
        if (!std::filesystem::exists(settings.nm)) {
            serial_front::AddLog("%s ERROR: nm command %s does not exist.\n", ERROR_CHAR, settings.nm.c_str());
            return false;
        }
        // Check if addr2line is set
        if (settings.addr2line.empty()) {
            serial_front::AddLog("%s ERROR: addr2line command is not set.\n", ERROR_CHAR);
            return false;
        }
        // check if addr2line exists
        if (!std::filesystem::exists(settings.addr2line)) {
            serial_front::AddLog("%s ERROR: addr2line command %s does not exist.\n", ERROR_CHAR, settings.addr2line.c_str());
            return false;
        }
        return true;
    }

    VariableType GetTypeFromFile(const std::string& file_path) {
        // string looks like: C:/path/to/file.c:72
        // open the file and read the line number
        // have to clean the path to remove the line number
        std::regex path_regex(R"((.*):\d+)");
        std::smatch match;
        std::string cleaned_path;
        if (std::regex_search(file_path, match, path_regex)) {
            cleaned_path = match[1].str();
        } else {
            return VariableType::TYPE_UINT32; // Default type if no match found
        }

        std::ifstream file(cleaned_path);
        if (!file.is_open()) {
            serial_front::AddLog("%s ERROR: Could not open file %s\n", ERROR_CHAR, cleaned_path.c_str());
            return VariableType::TYPE_UINT32;
        }

        // Get the line to read from the path
        std::regex line_regex(R"(:(\d+)$)");
        uint32_t line_number = 0;
        if (std::regex_search(file_path, match, line_regex)) {
            line_number = std::stoul(match[1].str());
        } else {
            serial_front::AddLog("%s ERROR: Could not extract line number from file path %s\n", ERROR_CHAR, file_path.c_str());
            return VariableType::TYPE_UINT32; // Default type if no line number found
        }

        // Read the specified line
        std::string line;
        for (uint32_t i = 0; i < line_number && std::getline(file, line); ++i) {
            // Skip lines until we reach the desired line number
        }
        file.close();

        // Go though each word in the line and check if it matches a type
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            for (size_t i = 0; i < VariableType::TYPE_NUM_OF_TYPES; ++i) {
                if (word == variable_types[i]) {
                    std::cout << "Found type: " << variable_types[i] << " in file: " << cleaned_path << ", line: " << line_number << "\n";
                    std::cout << "      " << line << "\n";
                    return static_cast<VariableType>(i); // Return the first matching type
                }
            }
        }

        return VariableType::TYPE_UINT32;
    }

    bool GetSourceFileInfo(const std::string& address_str, SourceFileInfo& info) {
        SerialBack_Settings * settings = serial_back::GetSettings();
        
        std::string addr2line_cmd = 
            settings->addr2line + " -e " + settings->elf_file_path + " " + address_str;

        // Run addr2line
        std::array<char, 128> addr2line_buffer;
        std::shared_ptr<FILE> addr2line_pipe(popen(addr2line_cmd.c_str(), "r"), pclose);
        if (!addr2line_pipe) {
            serial_front::AddLog("%s ERROR: Failed to run command: %s\n", ERROR_CHAR, addr2line_cmd.c_str());
            return false;
        }

        // Process addr2line output
        if (fgets(addr2line_buffer.data(), addr2line_buffer.size(), addr2line_pipe.get()) != nullptr) {
            info.raw_line = std::string(addr2line_buffer.data());
            // Remove trailing newline
            info.raw_line.erase(std::remove(info.raw_line.begin(), info.raw_line.end(), '\n'), info.raw_line.end());
        } else {
            serial_front::AddLog("%s ERROR: Failed to get file path for address: %s\n", ERROR_CHAR, address_str.c_str());
            return false;
        }

        // file_path looks like: /path/to/file.c:72
        // use regex to extract the file name
        std::regex file_regex(R"(([^/\\]+):\d+)");
        std::smatch file_match;
        if (std::regex_search(info.raw_line, file_match, file_regex)) {
            info.type = GetTypeFromFile(info.raw_line);
            info.file = file_match[1]; // Get the file name without the line number
        } else {
            info.file = "-- (declared static?)";
            info.type = VariableType::TYPE_UINT32;
        }

        return true;
    }

} // anonymous namespace

namespace elf_parser {
    /*
     * Checks if the timestamp of the current map file is newer than the 
     * timestamp from when the file was parsed.
     */
    bool ElfFileChanged(std::string elf_file_path) {
        static std::filesystem::file_time_type last_write_time;
        static bool error_thrown = false; // to not get error msg many times
        std::filesystem::path file_path(elf_file_path);
        if (elf_file_path.empty()) {
            return false; // No path provided
        }
        if (!std::filesystem::exists(file_path)) {
            if (!error_thrown)
                serial_front::AddLog("%s ERROR: elf file %s does not exist.\n", ERROR_CHAR, elf_file_path.c_str());
            error_thrown = true;
            //elf_file_path = "";
            //parsed_map.clear();
            return false; // File does not exist
        }
        error_thrown = false;
        auto current_write_time = std::filesystem::last_write_time(file_path);
        if (current_write_time != last_write_time) {
            last_write_time = current_write_time;
            return true; // File has changed
        }
        return false; // No change detected
    }

    bool ParseElfFile(FileSymbolMap& grouped_variables) {
        //FileSymbolMap grouped_variables;
        SerialBack_Settings * settings = serial_back::GetSettings();

        if (!CheckSettings(*settings)) {
            return false;
        }

        std::string cmd = settings->nm + " -S " + settings->elf_file_path;
        std::array<char, 0xFF+1> res_buffer;

        std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            serial_front::AddLog("%s ERROR: Failed to run command: %s\n", ERROR_CHAR, cmd.c_str());
            return false;
        }

        // Go through result of nm
        while (fgets(res_buffer.data(), res_buffer.size(), pipe.get()) != nullptr) {
            std::string line(res_buffer.data());
            std::istringstream iss(line);
            std::string symbol;
            std::string size_str;
            std::string type;
            std::string address_str;

            // Parse the line
            if (!(iss >> address_str >> size_str >> type >> symbol)) {
                continue; // Skip lines that do not match the expected format
            }

            if (type != "B" && type != "b" && type != "D" && type != "d") {
                continue;
            }

            SourceFileInfo file_info;
            if (!GetSourceFileInfo(address_str, file_info)) {
                file_info.file = "-- (decared static?)";
                file_info.type = VariableType::TYPE_UINT32; // Default type if addr2line fails
            }

            grouped_variables[file_info.file][symbol] = {
                .address = std::stoul(address_str, nullptr, 16),
                .size    = std::stoul(size_str, nullptr, 16),
                .frame   = 0, // Frame is not used in this context
                .type    = file_info.type
            };
        }
        return true;
    }

    std::vector<std::string> GetVariableTypes() {
        return variable_types;
    }
} // namespace elf_parser