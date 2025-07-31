#ifndef LOG_READER_H_
#define LOG_READER_H_

#include <string>
#include <unordered_map>
#include <vector>
#include "serial_back.h"

struct Data {
    std::vector<double> time;
    std::unordered_map<std::string, std::vector<double>> signals;
};

typedef enum {
    LOG_SOURCE_CSV,
    LOG_SOURCE_SERIAL,
} LogSource;

Data* GetData(void);
LogSource GetLogSource();
void ClearData(void);
void LogReadButton(void);
void InitSerialStream(std::unordered_map<std::string, VarStruct> log_variables);
#endif  // LOG_READER_H_