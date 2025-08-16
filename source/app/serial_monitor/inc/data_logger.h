#ifndef DATA_LOGGER_H_
#define DATA_LOGGER_H_

#include "serial_back.h"

namespace data_logger {
void init(const std::unordered_map<std::string, VarStruct>& variables);
void LogFrame(const FrameStruct& frame);
void SaveLog();
std::unordered_map<std::string, std::vector<double>> GetLogVariables();

} // namespace data_logger

#endif // DATA_LOGGER_H_