#ifndef PERFORMANCE_ANALYSIS_H_
#define PERFORMANCE_ANALYSIS_H_

#include <chrono>

namespace performance_analysis {
    typedef struct {
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> previous_start_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
        std::chrono::milliseconds::rep elapsed_time;
        std::chrono::milliseconds::rep tick_time;
    } PerformanceData;

    typedef enum {
        TASK_SERIAL_MONITORING,
        FUNC_LOG_FRAME,
        FUNC_DESERIALIZE_LOG,
        NUM_ANALYSIS,
    } AnalysisIndex;

    void Start(AnalysisIndex index);
    void End(AnalysisIndex index);
    PerformanceData * GetData(AnalysisIndex index);
    void PrintPerformanceData(AnalysisIndex index);
} // namespace performance_analysis
#endif  // PERFORMANCE_ANALYSIS_H_