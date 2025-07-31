#ifndef SERIAL_BACK_H_
#define SERIAL_BACK_H_
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

typedef struct {
        uint32_t address;
        size_t size;
} VarStruct;

typedef struct {
    std::string name;
    size_t size;
    uint32_t latest_rx;
} FrameVarStruct;
typedef struct {
    int id;
    uint64_t latest_timestamp;
    std::vector<FrameVarStruct> variables;
} FrameStruct;

// {file : {variable_name : .address, .size}}
using FileSymbolMap = std::unordered_map<std::string, std::unordered_map<std::string, VarStruct>>;
namespace serial_back {
    void SetMapFilePath(const std::string& path);
    bool Send(const std::vector<uint8_t>& data);
    void GetPorts(std::vector<std::string>& ports);
    bool SetPortName(const std::string& port);
    void GetPortName(std::string& port);
    void SetBaudRate(int baud);
    int  GetBaudRate();
    bool OpenSerialPort();
    bool CloseSerialPort();
    bool IsPortOpen();
    void SerialInit();
    void DeInit();
    void StartLog(std::unordered_map<std::string, VarStruct> log_variables);
    void StopLog();
    bool IsLogRunning();
    FileSymbolMap GetParsedMap();

} // namespace serial_back

#endif // SERIAL_BACK_H_