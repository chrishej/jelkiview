#ifndef LOG_READER_H_
#define LOG_READER_H_

#include <string>
#include <unordered_map>
#include <vector>

struct Data {
    std::vector<double> time;
    std::unordered_map<std::string, std::vector<double>> signals;
};

Data* GetData(void);
void ClearData(void);
void LogReadButton(void);
#endif  // LOG_READER_H_