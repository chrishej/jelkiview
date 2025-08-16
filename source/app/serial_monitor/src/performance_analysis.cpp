#include "performance_analysis.h"
#include <iostream>

namespace {
    using performance_analysis::AnalysisIndex;
    using performance_analysis::PerformanceData;

    typedef struct {
        std::string name;
        PerformanceData* data;
    } AnalysisStruct;
    
    PerformanceData SerialMonitoringTaskData;
    PerformanceData FunLogFrameData;
    PerformanceData FunDeserializeLogData;

    AnalysisStruct list[AnalysisIndex::NUM_ANALYSIS] = {
        {"SerialMonitoringTask",    &SerialMonitoringTaskData},
        {"FunLogFrame",             &FunLogFrameData},
        {"FunDeserializeLog",       &FunDeserializeLogData}
    };



} // anonymous namespace

namespace performance_analysis {

    void Start(AnalysisIndex index) {
        if (index < AnalysisIndex::NUM_ANALYSIS) {
            PerformanceData * data = list[index].data;

            data->start_time = std::chrono::high_resolution_clock::now();

            auto time_diff = data->start_time - data->previous_start_time;
            data->tick_time = 
                std::chrono::duration_cast<std::chrono::microseconds>(time_diff).count();

            data->previous_start_time = data->start_time;
        }
    }

    void End(AnalysisIndex index) {
        if (index < AnalysisIndex::NUM_ANALYSIS) {
            PerformanceData * data = list[index].data;

            data->end_time = std::chrono::high_resolution_clock::now();
            auto elapsed_time = data->end_time - data->start_time;
            data->elapsed_time = 
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed_time).count();
        }
    }

    PerformanceData * GetData(AnalysisIndex index) {
        if (index < AnalysisIndex::NUM_ANALYSIS) {
            return list[index].data;
        }
        return nullptr;
    }

    void PrintPerformanceData(AnalysisIndex index) {
        int largest_name_length = 0;
        for (const auto& analysis : list) {
            largest_name_length = std::max(largest_name_length, static_cast<int>(analysis.name.length()));
        }
        if (index < AnalysisIndex::NUM_ANALYSIS) {
            AnalysisStruct AnalysisStr = list[index];
            std::cout << std::setw(largest_name_length) << AnalysisStr.name
                      << " | Elapsed Time: " << std::setw(8) << AnalysisStr.data->elapsed_time  << " us"
                      << " | Tick Time: "    << std::setw(8) << AnalysisStr.data->tick_time     << " us";
        }
    }

} // namespace performance_analysis