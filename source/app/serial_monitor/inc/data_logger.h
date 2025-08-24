#ifndef DATA_LOGGER_H_
#define DATA_LOGGER_H_

#include "serial_back.h"
#include "log_reader.h"

namespace data_logger {
void init(const std::unordered_map<std::string, VarStruct>& variables);
void LogFrame(const FrameStruct& frame);
void SaveLog();
Data* GetLogData();

} // namespace data_logger

#endif // DATA_LOGGER_H_