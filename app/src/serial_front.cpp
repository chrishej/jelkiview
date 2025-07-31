#include "serial_front.h"
#include "serial_back.h"
#include "imgui.h"
#include <stdarg.h>
#include <stdio.h>
#include <mutex>
#include <vector>
#include "string.h"
#include <iostream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include "ImGuiFileDialog.h"
#include "data_logger.h"
#include <regex>

namespace {
    bool show_console = true;
    bool show_map_parser = true;
    bool show_port_menu = false;
    bool port_opened = false;

    static ImVector<char*> Items;
    static char InputBuf[256];
    static bool AutoScroll = true;
    static FileSymbolMap parsed_map;
    static std::string map_file_path;
    std::unordered_map<std::string, VarStruct> log_variables;
    std::unordered_map<std::string, std::vector<double>> log;

    void MenuBar() {
        std::vector<std::string> ports;
        static int selected_index = 0;
        static int baud_rate;
        ImGui::BeginMenuBar();

        if (ImGui::BeginMenu("Widgets")) {
            if (ImGui::MenuItem("Console")) {
                show_console = true;
            }
            if (ImGui::MenuItem("Map Parser")) {
                show_map_parser = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Port")) {
            serial_back::GetPorts(ports);

            // First time initialization to sync selected_index with current port in backend
            if (!show_port_menu) {
                std::string current_port;
                serial_back::GetPortName(current_port);
                if (!current_port.empty()) {
                    for (size_t i = 0; i < ports.size(); ++i) {
                        if (ports[i] == current_port) {
                            selected_index = static_cast<int>(i);
                            break;
                        }
                    }
                }

                baud_rate = serial_back::GetBaudRate();
            }
            show_port_menu = true;

            // Convert to array of const char* for ImGui::Combo
            std::vector<const char*> cstr_items;
            for (const auto& item : ports) {
                cstr_items.push_back(item.c_str());
            }

            if (ImGui::BeginTable("PortTable", 2)) {
                ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Port");
                ImGui::TableSetColumnIndex(1);
                int selected_index_old = selected_index;
                if (ImGui::Combo("##PortCombo", &selected_index, cstr_items.data(), cstr_items.size())) {
                    if (selected_index >= 0 && selected_index < static_cast<int>(ports.size())) {
                        std::string port_name = ports[selected_index];
                        if(!serial_back::SetPortName(port_name)) {
                            selected_index = selected_index_old;
                        }
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Baud Rate");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::InputInt("##BaudRate", &baud_rate, 0, 0)) {
                    if (baud_rate > 0) {
                        serial_back::SetBaudRate(baud_rate);
                    }
                }

                ImGui::EndTable();

                port_opened = serial_back::IsPortOpen();
                if (ImGui::Checkbox("Port Opened", &port_opened)) {
                    if (port_opened) {
                        if (!serial_back::OpenSerialPort()) {
                            port_opened = false; // Reset if opening failed
                        }
                    } else {
                        if (!serial_back::CloseSerialPort()) {
                            port_opened = true; // Reset if closing failed
                        }
                    }
                }
            }

            ImGui::EndMenu();
        } else {
            show_port_menu = false;
        }

        ImGui::EndMenuBar();
    }

    bool HexStringToBytes_SpaceSafe(const char* input, std::vector<uint8_t>& out_bytes) {
        out_bytes.clear();
        std::string hex_digits;

        // Strip spaces and validate characters
        for (size_t i = 0; input[i] != '\0'; ++i) {
            char c = input[i];
            if (std::isspace(static_cast<unsigned char>(c))) continue;
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                serial_front::AddLog("%s Error: Invalid hex character '%c' at index %zu\n", ERROR_CHAR, c, i);
                return false;
            }
            hex_digits += c;
        }

        if (hex_digits.length() % 2 != 0) {
            serial_front::AddLog("%s Error: Incomplete byte - odd number of hex digits\n", ERROR_CHAR);
            return false;
        }

        // Parse hex pairs
        for (size_t i = 0; i < hex_digits.length(); i += 2) {
            char byte_str[3] = { hex_digits[i], hex_digits[i + 1], '\0' };
            uint8_t byte = static_cast<uint8_t>(std::strtoul(byte_str, nullptr, 16));
            out_bytes.push_back(byte);
        }

        return true;
    }

    void Console() {
        if (show_console) {
            ImGui::Begin("Console", &show_console, ImGuiWindowFlags_NoScrollbar);
            if (ImGui::Button("Clear")) {
            for (int i = 0; i < Items.Size; i++) free(Items[i]);
                Items.clear();
            }

            ImGui::Separator();

            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()-5), false,
                            ImGuiWindowFlags_HorizontalScrollbar);
            for (int i = 0; i < Items.Size; i++)
                ImGui::TextUnformatted(Items[i]);
            if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::Separator();

            if (ImGui::InputText("Send", InputBuf, IM_ARRAYSIZE(InputBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                serial_front::AddLog("< %s", InputBuf);
                std::vector<uint8_t> bytes;
                if (HexStringToBytes_SpaceSafe(InputBuf, bytes)) {
                    serial_back::Send(bytes);
                }

                InputBuf[0] = '\0';
            }
                ImGui::End();
        }
    }


    void MapParser() {
        static bool map_file_dialog = false;
        if (show_map_parser) {
            ImGui::Begin("Map Parser", &show_map_parser, ImGuiWindowFlags_NoScrollbar);

            /* --------------------------- TOP BAR -------------------------- */
            ImGui::SameLine();
            // Open file dialog to select map file
            if (ImGui::Button("Open")) {
                map_file_dialog = true;
            }
            if (map_file_dialog) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileMap", "Choose File", ".map");
                map_file_dialog = false;
            }
            if (ImGuiFileDialog::Instance()->Display("ChooseFileMap")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    map_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                    serial_back::SetMapFilePath(map_file_path);
                }
                ImGuiFileDialog::Instance()->Close();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Deselect All")) {
                log_variables.clear();
            }
            // serachbox
            ImGui::SameLine();
            static char search_buf[256] = "";
            ImGui::Text("Search: ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##mapsearch", search_buf, IM_ARRAYSIZE(search_buf));
            ImGui::SameLine();

            if (!serial_back::IsLogRunning()) {
                if (ImGui::Button("Start")) {
                    if (!log_variables.empty()) {
                        serial_back::StartLog(log_variables);
                    } else {
                        serial_front::AddLog("%s No variables selected for logging.", ERROR_CHAR);
                    }
                }
            } else {
                if (ImGui::Button("Stop")) {
                    serial_back::StopLog();
                }
            }
            /* ------------------------- END TOP BAR ------------------------ */

            if (ImGui::BeginChild("MapContent", ImVec2(0, 0), false,
                                ImGuiWindowFlags_HorizontalScrollbar)) {
                // table with 
                // Selected | File name | Variable Name | Address | Size
                if (!parsed_map.empty()) {
                    if (ImGui::BeginTable("MapTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 25.0f);
                        ImGui::TableSetupColumn("File Name", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Variable Name", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        log = data_logger::GetLogVariables();

                        bool selected = false;
                        std::string check_label;
                        for (const auto& [file, vars] : parsed_map) {
                            for (const auto& [varName, varStruct] : vars) {
                                if (strlen(search_buf) > 0 && 
                                    (file.find(search_buf) == std::string::npos && 
                                     varName.find(search_buf) == std::string::npos)) {
                                    continue; // Skip if search term is not found
                                }
                                if (log_variables.find(varName) != log_variables.end()) {
                                    selected = true;
                                } else {
                                    selected = false;
                                }
                                if (serial_back::IsLogRunning() && !selected) {
                                    continue;
                                }
                                // Get value from log if it exists
                                std::string value = "-";
                                if (log.find(varName) != log.end()) {
                                    auto& values = log[varName];
                                    if (!values.empty()) {
                                        std::regex rx(".*_([A-Za-z0-9]+)$");
                                        std::smatch match;
                                        if (std::regex_match(varName, match, rx)) {
                                            std::string post_fix = match[1];
                                            std::transform(post_fix.begin(), post_fix.end(), post_fix.begin(),
                                                [](unsigned char c){ return std::tolower(c); });
                                                 if (post_fix == "s08") value = std::to_string(static_cast<int8_t>(static_cast<uint8_t>(values.back())));
                                            else if (post_fix == "u08") value = std::to_string(static_cast<uint8_t >(values.back()));
                                            else if (post_fix == "s16") value = std::to_string(static_cast<int16_t>(static_cast<uint16_t>(values.back())));
                                            else if (post_fix == "u16") value = std::to_string(static_cast<uint16_t>(values.back()));
                                            else if (post_fix == "s32") value = std::to_string(static_cast<int32_t>(static_cast<uint32_t>(values.back())));
                                            else if (post_fix == "u32") value = std::to_string(static_cast<uint32_t>(values.back()));
                                            else if (post_fix == "f32") value = std::to_string(values.back());
                                            else                        value = std::to_string(static_cast<uint32_t>(values.back()));
                                        } else {
                                            value = std::to_string(static_cast<uint32_t>(values.back()));
                                        }
                                        
                                    }
                                }

                                check_label = "##" + varName + "_" + "selected_label";
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Checkbox(check_label.c_str(), &selected);
                                if (!serial_back::IsLogRunning()) {
                                    if (selected) {
                                        log_variables[varName] = varStruct; // Add to log variables
                                    } else {
                                        log_variables.erase(varName); // Remove from log variables
                                    }
                                }
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%s", file.c_str());
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%s", varName.c_str());
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("0x%lx", static_cast<unsigned long>(varStruct.address));
                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("%zu", varStruct.size);
                                ImGui::TableSetColumnIndex(5);
                                ImGui::Text("%s", value.c_str());
                            }
                        }
                        ImGui::EndTable();
                    }
                } else {
                    ImGui::Text("No map data parsed yet.");
                }
                ImGui::EndChild();
            }

            ImGui::End();
        }
        
    }
} // namespace anonymous

namespace serial_front {
    std::mutex mtx;

    void SerialWindow() {
        MenuBar();
        Console();
        MapParser();
        parsed_map = serial_back::GetParsedMap();
        // remove entries in log_variables that are not in parsed_map
        for (auto it = log_variables.begin(); it != log_variables.end(); it++) {
            // check in each file in parsed_map
            bool found = false;
            for (const auto& [file, vars] : parsed_map) {
                if (vars.find(it->first) != vars.end()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                log_variables.erase(it); // Remove if not found
                AddLog("%s Removed variable %s from log variables, not found in parsed map.", COMMAND_CHAR, it->first.c_str());
            }
        }
    }

    void AddLog(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        va_end(args);
        Items.push_back(strdup(buf));
    }

} // namespace serial_front