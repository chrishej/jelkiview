#include "serial_back.h"
#include "simple_uart.h"
#include "serial_front.h"
#include <iostream>
#include <windows.h>
#include <thread>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <filesystem>
#include "json.hpp"
#include "data_logger.h"
#include "performance_analysis.h"
#include "elf_parser.h"

using Json = nlohmann::json;

typedef enum {
    CMD_INVALID         = 0x00,
    CMD_TOGGLE_LED      = 0x01,
    CMD_SETUP_FRAME     = 0x02,
    CMD_SET_FRAME_SIZE  = 0x03,
    CMD_START_LOG       = 0x04,
    CMD_STOP_LOG        = 0x05,
    CMD_NR_OF_CMDS      = 0x06,
} CommandIndex;
typedef struct {
    std::string name;
    uint8_t cmd;
}CommandStruct;

std::unordered_map<std::string, FrameStruct> frames;

namespace {
    DWORD WINAPI SerialMonitoringTask(LPVOID lpParam);
    HANDLE serial_monitor_handle = nullptr;

    struct simple_uart* uart_instance           = nullptr;
    FileSymbolMap       parsed_map;
    bool                serial_thread_running   = false;
    bool                serial_thread_exit      = false;
    bool                log_running             = false;
    bool                parsing_elf_file        = false;
    int                 baud_rate               = 250000;
    uint8_t             buffer[0xFFFF+1];
    std::string         port_name               = "COM5";
    SerialBack_Settings settings = {.nm = "", .addr2line = "", .elf_file_path = ""};

    uint16_t DBG_recieved_bytes = 0;
    std::chrono::milliseconds::rep DBG_serial_monitor_elapsed_time;
    std::chrono::milliseconds::rep DBG_serial_monitor_tick_time;

    // Indexed with <enum>CommandIndex
    std::vector<CommandStruct> command_list = {
        {"invalid",         0x00},
        {"toggle_led",      0x01},
        {"setup_frame",     0x02},
        {"set_frame_size",  0x03},
        {"start_log",       0x04},
        {"stop_log",        0x05},
    };

    void BuildFramesCommand(std::vector<uint8_t>& command_buffer, const std::unordered_map<std::string, VarStruct>& log_variables) {
        command_buffer.push_back(command_list[CMD_SETUP_FRAME].cmd);

        int nr_of_frames = 3;
        // address size address size address size ...
        // Send everything msb first
        std::vector<std::vector<uint8_t>> frames_command(nr_of_frames);

        frames.clear();

        // Set up preamble for all frames
        for (int i=0; i<nr_of_frames; i++) {
            frames_command[i].push_back(i & 0xFF );
            frames_command[i].push_back(0); // Padding to send 32-bits
            frames_command[i].push_back(0); // Padding to send 32-bits
            frames_command[i].push_back(0); // Padding to send 32-bits

            frames_command[i].push_back(0); // Size of the frame
            frames_command[i].push_back(0); // Padding to send 32-bits
            frames_command[i].push_back(0); // Padding to send 32-bits
            frames_command[i].push_back(0); // Padding to send 32-bits
        }

        for (const auto& [varName, varStruct] : log_variables) {
            int frame_id = varStruct.frame;
            
            // Increment number of variables
            frames_command[frame_id][4]++;

            if (varStruct.size > 0xFF) {
                serial_front::AddLog("%s ERROR: Variable %s has size larger than 256, which is not supported.\n", ERROR_CHAR, varName.c_str());
                continue;
            }

            FrameVarStruct frame_var = {.name=varName, .size=varStruct.size, .latest_rx=0, .type=varStruct.type};
            frames[std::to_string(frame_id)].variables.push_back(frame_var);
            frames[std::to_string(frame_id)].id = frame_id;

            // LSB First
            frames_command[frame_id].push_back( varStruct.address        & 0xFF);
            frames_command[frame_id].push_back((varStruct.address >> 8 ) & 0xFF);
            frames_command[frame_id].push_back((varStruct.address >> 16) & 0xFF);
            frames_command[frame_id].push_back((varStruct.address >> 24) & 0xFF);

            frames_command[frame_id].push_back(varStruct.size & 0xFF);
            frames_command[frame_id].push_back(0); // Padding to send 32-bits
            frames_command[frame_id].push_back(0); // Padding to send 32-bits
            frames_command[frame_id].push_back(0); // Padding to send 32-bits
        }

        for (int i=0; i<nr_of_frames; i++) {
            for (size_t j=0; j<frames_command[i].size(); j++) {
                command_buffer.push_back(frames_command[i][j]);
            }
        }

        command_buffer.push_back(0xFF);
    }


    typedef enum {
        LOG_WAITING_START,
        LOG_FRAME_ID,
        LOG_RECIEVE_TIME,
        LOG_RECIEVE_VARIABLES,
    } DeserializeLogState;

    void DeserializeLog(uint8_t rx_byte) {
        static DeserializeLogState  deserialize_log_state = LOG_WAITING_START;
        static int                  rx_cnt          = 0;
        static uint16_t             variable_cnt    = 0;
        static int                  frame_id        = 0;
        static FrameStruct*         current_frame   = nullptr;

        const bool log = false;
        
        int var_size;

        performance_analysis::Start(performance_analysis::AnalysisIndex::FUNC_DESERIALIZE_LOG);

        if (!log_running) {
            // Reset state if not logging
            deserialize_log_state = LOG_WAITING_START;
            rx_cnt = 0;
            variable_cnt = 0;
            return;
        }

        switch (deserialize_log_state)
        {
        case LOG_WAITING_START:
            if (rx_byte == 0xFF) {
                if (log) serial_front::AddLog("%s Log Started Recieved.\n", RX_CHAR, rx_byte);
                deserialize_log_state = LOG_FRAME_ID;
            }
            break;

        case LOG_FRAME_ID: {
            if (log) serial_front::AddLog("%s Frame ID: %d.\n", RX_CHAR, rx_byte);
            frame_id = rx_byte;

            auto it = frames.find(std::to_string(frame_id));

            if (it == frames.end()) {
                serial_front::AddLog("%s ERROR: Received frame ID %d, but it was not configured. Stopping log.\n", ERROR_CHAR, frame_id);
                serial_back::StopLog();
                deserialize_log_state = LOG_WAITING_START;
                rx_cnt = 0;
                variable_cnt = 0;
                return;
            }

            current_frame = &it->second;

            deserialize_log_state = LOG_RECIEVE_TIME;
            current_frame->latest_timestamp = 0;
            rx_cnt = 0;
            break;
        }
        
        case LOG_RECIEVE_TIME:
            // next 4 bytes are time, with most significant byte first
            current_frame->latest_timestamp |= (static_cast<uint64_t>(rx_byte) << (8 * (3 - rx_cnt)));
            rx_cnt++;
            if (rx_cnt >= 4) {
                // After 4 bytes, we expect to receive variables
                deserialize_log_state = LOG_RECIEVE_VARIABLES;
                rx_cnt = 0;
                variable_cnt = 0;
                current_frame->latest_timestamp *= 10;
                if (log) serial_front::AddLog("%s     Frame Time: %lu us.\n", RX_CHAR, current_frame->latest_timestamp);
            }
            break;

        case LOG_RECIEVE_VARIABLES:
            var_size = current_frame->variables[variable_cnt].size;
            if (rx_cnt == 0) {
                // Mask to 0.
                current_frame->variables[variable_cnt].latest_rx = 0;
            }
            current_frame->variables[variable_cnt].latest_rx |= 
                (static_cast<uint32_t>(rx_byte) << (8 * (var_size - 1 - rx_cnt)));
            rx_cnt++;
            if (rx_cnt >= var_size) {
                if (log) serial_front::AddLog("%s     Variable %s, Value: 0x%08X\n",
                    RX_CHAR, current_frame->variables[variable_cnt].name.c_str(),
                    current_frame->variables[variable_cnt].latest_rx);
                variable_cnt++;
                if (variable_cnt < current_frame->variables.size()) {
                    // More variables to decode
                    rx_cnt = 0;
                } else {
                    // Done
                    deserialize_log_state = LOG_FRAME_ID;
                    data_logger::LogFrame(*current_frame);
                }
            }
            break;
        
        default:
            //TODO error, restart log
            break;
        }

        performance_analysis::End(performance_analysis::AnalysisIndex::FUNC_DESERIALIZE_LOG);
    }

    /*
     * Helper function.
     * Inserts recieved bytes into buffer and runs log deserialization.
     */
    void HandleRx(const int read_bytes) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < read_bytes; i++) {
            // Always run the state machine, 
            // mostly so it can reset when log is turned off.
            DeserializeLog(buffer[i]);

            // When log runs a lot of data will be recieved.
            // Don't print anything.
            if (!log_running || true) {
                oss << std::setw(2) << static_cast<int>(buffer[i]) << " ";
            }
        }

        if (!oss.str().empty() && !log_running) {
            std::lock_guard<std::mutex> lock(serial_front::mtx);
            serial_front::AddLog("> %s", oss.str().c_str());
        }
    }

    /*
     * Fast task to handle reading of the serial port 
     * and deserialization of the log data.
     */
    DWORD WINAPI SerialMonitoringTask(LPVOID lpParam) {
        int             available;
        int             read_bytes          = 0;
        uint32_t        sleep_time          = 10;
        uint32_t const  sleep_time_logging  = 0;
        uint32_t const  sleep_time_normal   = 10;
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> previous_start_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> end_time;

        serial_front::AddLog("%s Serial backend thread started.\n", COMMAND_CHAR);

        serial_thread_running = true;
        while(!serial_thread_exit) {
            // get start time for loop time calculation
            start_time = std::chrono::high_resolution_clock::now();
            previous_start_time = start_time;

            // Don't check instance, simple_uart returns 0 if it is not open.
            available = simple_uart_has_data(uart_instance);

            if (available >= sizeof(buffer)) {
                serial_front::AddLog("%s Overflow, %d bytes available to read, but buffer is only %d bytes.\n", ERROR_CHAR, available, sizeof(buffer));
            }
            if (available > 0) {
                performance_analysis::Start(performance_analysis::TASK_SERIAL_MONITORING);

                read_bytes = simple_uart_read(uart_instance, buffer, available);
                //std::cout << "Read bytes: " << read_bytes << "\n";
                if (read_bytes > 0) {
                    HandleRx(read_bytes);
                } else {
                    serial_front::AddLog("%s ERROR: Failed to read from %s.\n", ERROR_CHAR, port_name.c_str());
                }
                DBG_recieved_bytes = read_bytes*0.1 + DBG_recieved_bytes*0.9;

                performance_analysis::End(performance_analysis::TASK_SERIAL_MONITORING);
            } else {
                DBG_recieved_bytes = available*0.1 + DBG_recieved_bytes*0.9;
            }


            // Faster update when log running. CPU!!!
            if (log_running) {
                sleep_time = sleep_time_logging;
            } else {
                sleep_time = sleep_time_normal;
            }
            // Calculate elapsed time for the loop
            end_time = std::chrono::high_resolution_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();


            // If the elapsed time is less than sleep_time, sleep for the remaining time
            if (elapsed_time < sleep_time) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time - elapsed_time));
            }
        }
        serial_thread_running = false;
        std::cout << "\nSerial backend thread exiting.\n";

        return 0;
    }


/* -------------------------------------------------------------------------- */
/*                              Slow update tasks                             */
/* -------------------------------------------------------------------------- */
// These tasks are run in a slow task, to keep housekeeping duties 
// (which can be heavy) away from the fast task which reads from the serial etc.

    void SaveSettings() {
        std::string settings_path = "resources/serial_settings.json";
        Json settings_out;
        settings_out["baud_rate"] = baud_rate;
        settings_out["port_name"] = port_name;
        settings_out["elf_file_path"] = settings.elf_file_path;
        settings_out["nm"] = settings.nm;
        settings_out["addr2line"] = settings.addr2line;
        std::ofstream out(settings_path);
        out << settings_out.dump(4);
        out.close();
    }

    void LoadSettings() {
        std::string settings_path = "resources/serial_settings.json";
        if (!std::filesystem::exists(settings_path)) {
            return;
        }
        std::ifstream settings_file(settings_path);
        Json settings_json = Json::parse(settings_file);  // NOLINT(misc-const-correctness)

        settings_file.close();
        baud_rate = settings_json.value("baud_rate", 250000);
        port_name = settings_json.value("port_name", "COM5");

        if (settings_json.contains("elf_file_path")) {
            settings.elf_file_path = settings_json["elf_file_path"];
            // if empty, set to "-"
            if (settings.elf_file_path.empty()) {
                settings.elf_file_path = "-";
            }
        } else {
            settings.elf_file_path = "-";
        }
        if (settings_json.contains("nm")) {
            settings.nm = settings_json["nm"];
            // if empty, set to "-"
            if (settings.nm.empty()) {
                settings.nm = "-";
            }
        } else {
            settings.nm = "-";
        }
        if (settings_json.contains("addr2line")) {
            settings.addr2line = settings_json["addr2line"];
            // if empty, set to "-"
            if (settings.addr2line.empty()) {
                settings.addr2line = "-";
            }
        } else {
            settings.addr2line = "-";
        }
    }

    void SlowTask() {
        static bool elf_file_changed_old = false;
        bool elf_file_changed;

        while(!serial_thread_exit) {
            elf_file_changed = elf_parser::ElfFileChanged(settings.elf_file_path);

            if ( (elf_file_changed_old && !elf_file_changed) ) {
                parsing_elf_file = true;
                elf_parser::ParseElfFile(parsed_map);
                parsing_elf_file = false;
            }

            performance_analysis::PerformanceData * data = 
                performance_analysis::GetData(performance_analysis::TASK_SERIAL_MONITORING);
            if (data) {
                DBG_serial_monitor_elapsed_time = data->elapsed_time;
                DBG_serial_monitor_tick_time    = data->tick_time;
            }

//            if (log_running) {
//                std::cout << "Rx bytes: "         << std::setw(8)     << DBG_recieved_bytes;
//                performance_analysis::PrintPerformanceData(performance_analysis::TASK_SERIAL_MONITORING);
//                std::cout << " | ";
//                performance_analysis::PrintPerformanceData(performance_analysis::FUNC_LOG_FRAME);
//                //std::cout << " | ";
//                //performance_analysis::PrintPerformanceData(performance_analysis::FUNC_DESERIALIZE_LOG);
//                std::cout << "\n";
//            }


            elf_file_changed_old = elf_file_changed;

            SaveSettings();

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
/* -------------------------------------------------------------------------- */
/*                            End Slow update tasks                           */
/* -------------------------------------------------------------------------- */
} // namespace anonymous

namespace serial_back {
    void SetElfFilePath(const std::string& path) {
        settings.elf_file_path = path;
    }

    bool Send(const std::vector<uint8_t>& data) {
        static uint8_t tx_byte = 0;
        const int tx_wait_ms = 1;
        if (uart_instance) {
            ssize_t written;
            for (size_t i = 0; i < data.size(); i++) {
                tx_byte = data[i];
                std::this_thread::sleep_for(std::chrono::milliseconds(tx_wait_ms));
                written = simple_uart_write(uart_instance, &tx_byte, 1);
                if (written < 0) {
                    serial_front::AddLog("%s ERROR: Failed to write to %s.\n", ERROR_CHAR, port_name.c_str());
                    return false;
                }
            }
        } else {
            serial_front::AddLog("%s ERROR: Port %s is not opened.\n", ERROR_CHAR, port_name.c_str());
            return false;
        }
        return true;
    }
    
    void GetPorts(std::vector<std::string>& ports) {
        char **names;
        ssize_t nuarts = simple_uart_list(&names);
        ports.clear();
        for (ssize_t i = 0; i < nuarts; i++) {
            if (names[i]) {
                ports.push_back(names[i]);
                free(names[i]);
            }
        }
        free(names);
    }

    bool SetPortName(const std::string& port) {
        // Only if port is not opened
        if (!uart_instance) {
            port_name = port;
            return true;
        }
        return false;
    }
    void GetPortName(std::string& port)       {port      = port_name;}
    void SetBaudRate(int baud)                {baud_rate = baud;}
    int  GetBaudRate()                        {return baud_rate;}
    
    bool OpenSerialPort() {
        uart_instance = simple_uart_open(port_name.c_str(), baud_rate, "8N1");
        if (!uart_instance) {
            serial_front::AddLog("%s ERROR: Failed to open %s.\n",ERROR_CHAR, port_name.c_str());
            return false;
        }
        StopLog();
        serial_front::AddLog("%s Opened\n%s    Port:      %s\n%s    Baud Rate: %d\n", COMMAND_CHAR, COMMAND_CHAR, port_name.c_str(), COMMAND_CHAR, baud_rate);
        return true;
    }

    bool CloseSerialPort() {
        if (uart_instance) {
            simple_uart_close(uart_instance);
            uart_instance = nullptr;
            serial_front::AddLog("%s Closed port %s.\n", COMMAND_CHAR, port_name.c_str());
            return true;
        } else {
            serial_front::AddLog("%s Port %s was not initialized.\n", COMMAND_CHAR, port_name.c_str());
            return false;
        }
    }

    bool IsPortOpen() {
        return uart_instance != nullptr;
    }

    void SerialInit() {
        LoadSettings();

        serial_monitor_handle = CreateThread(NULL, 0, SerialMonitoringTask, NULL, 0, NULL);
        SetThreadPriority(serial_monitor_handle, THREAD_PRIORITY_TIME_CRITICAL);

        std::thread slow_task_thread(SlowTask);
        slow_task_thread.detach();
    }

    void DeInit() {
        if (uart_instance) {
            StopLog();
            simple_uart_close(uart_instance);
            uart_instance = nullptr;
        }
        
        if (log_running) {
            data_logger::SaveLog();
        }

        serial_thread_exit = true;
        // Just to get graceful closing of port
        while (serial_thread_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void StartLog(std::unordered_map<std::string, VarStruct> log_variables) {
        std::vector<uint8_t> command_buffer;

        //command_buffer.push_back(command_list[CMD_SET_FRAME_SIZE].cmd);
        //command_buffer.push_back(log_variables.size() & 0xFF); // Size of the frame

        BuildFramesCommand(command_buffer, log_variables);

        command_buffer.push_back(command_list[CMD_START_LOG].cmd);

        // print command to console as hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const auto& byte : command_buffer) {
            oss << std::setw(2) << static_cast<int>(byte) << " ";
        }
        serial_front::AddLog("%s %s\n", TX_CHAR, oss.str().c_str());

        Send(command_buffer);

        data_logger::init(log_variables);
        log_running = true;
    }

    void StopLog() {
        if (log_running) {
            data_logger::SaveLog();
        }
        std::vector<uint8_t> command_buffer;
        command_buffer.push_back(command_list[CommandIndex::CMD_STOP_LOG].cmd);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        oss << std::setw(2) << static_cast<int>(command_list[CommandIndex::CMD_STOP_LOG].cmd);
        serial_front::AddLog("%s %s\n", TX_CHAR, oss.str().c_str());
        Send(command_buffer);
        log_running = false;
    }

    bool IsLogRunning() {
        return log_running;
    }

    bool IsParsingElfFile() {
        return parsing_elf_file;
    }

    FileSymbolMap GetParsedMap() {
        return parsed_map;
    }

    SerialBack_Settings* GetSettings() {
        return &settings;
    }
} // namespace serial_back