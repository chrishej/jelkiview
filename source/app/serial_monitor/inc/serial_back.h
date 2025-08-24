#ifndef SERIAL_BACK_H_
#define SERIAL_BACK_H_
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
//#include "elf_parser.h"

typedef enum {
        TYPE_UINT8,
        TYPE_UINT16,
        TYPE_UINT32,
        TYPE_INT8,
        TYPE_INT16,
        TYPE_INT32,
        TYPE_FLOAT,
        TYPE_DOUBLE,
        TYPE_CHAR,
        TYPE_BOOL,
        TYPE_NUM_OF_TYPES,
        TYPE_UNKNOWN
} VariableType;

typedef struct {
        uint32_t address;
        size_t size;
        int frame;
        VariableType type;
} VarStruct;

typedef struct {
    std::string name;
    size_t size;
    uint32_t latest_rx;
    VariableType type;
} FrameVarStruct;
typedef struct {
    int id;
    uint64_t latest_timestamp;
    std::vector<FrameVarStruct> variables;
} FrameStruct;
typedef struct {
    std::string nm;
    std::string addr2line;
    std::string elf_file_path;
} SerialBack_Settings;

// {file : {variable_name : .address, .size, .frame, .type}}
typedef std::unordered_map<std::string, std::unordered_map<std::string, VarStruct>> FileSymbolMap;
namespace serial_back {
    void SetElfFilePath(const std::string& path);
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
    bool IsParsingElfFile();
    FileSymbolMap GetParsedMap();
    SerialBack_Settings* GetSettings();

} // namespace serial_back

#endif // SERIAL_BACK_H_