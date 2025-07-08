#ifndef LOG_READER_H_
#define LOG_READER_H_

#include <unordered_map>
#include <vector>
#include <string>

struct Data {
    std::vector<double> time;
    std::unordered_map<std::string, std::vector<double>> signals;
};

extern std::vector<std::unordered_map<std::string, bool>> subplotsMap;
extern std::vector< std::vector<std::string> > layout;

Data * GetData(void);
void ClearData(void);
void LogReadButton(void);
#endif // LOG_READER_H_