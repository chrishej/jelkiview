#include "performance_analysis.h"
#include "imgui.h"
#include "implot.h"
#include <iostream>

namespace {
    using performance_analysis::AnalysisIndex;
    using performance_analysis::PerformanceData;

    typedef struct {
        std::string name;
        PerformanceData* data;
    } AnalysisStruct;

    std::deque<int> recieved_bytes_buffer;
    
    PerformanceData SerialMonitoringTaskData;
    PerformanceData FunLogFrameData;
    PerformanceData FunDeserializeLogData;
    PerformanceData TaskMainGui;

    uint16_t buffer_size = 10000;

    AnalysisStruct list[AnalysisIndex::NUM_ANALYSIS] = {
        {"SerialMonitoringTask",    &SerialMonitoringTaskData},
        {"FunLogFrame",             &FunLogFrameData},
        {"FunDeserializeLog",       &FunDeserializeLogData},
        {"TaskMainGui",             &TaskMainGui},
    };

    void SaveToBuffer(PerformanceData& data) {
        data.elapsed_time_deque.push_back(data.elapsed_time);
        data.tick_time_deque.push_back(data.tick_time);

        // Only save last 100 samples
        if (data.elapsed_time_deque.size() > buffer_size) {
            data.elapsed_time_deque.pop_front();
        }
        if (data.tick_time_deque.size() > buffer_size) {
            data.tick_time_deque.pop_front();
        }
    }


} // anonymous namespace

namespace performance_analysis {
    void RecievedBytes(uint16_t bytes) {
        recieved_bytes_buffer.push_back(bytes);
        if (recieved_bytes_buffer.size() > buffer_size) {
            recieved_bytes_buffer.pop_front();
        }
    }

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

            SaveToBuffer(*data);
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

    void PerformanceWindow(bool& open) {
        ImGui::SetNextWindowSize(ImVec2(600,400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Performance Analysis", &open)) {
            if (ImPlot::BeginSubplots("##PerformanceSubplots", 3, 1, ImVec2(-1,-1), ImPlotSubplotFlags_LinkAllX)) {
                ImPlot::BeginPlot("Elapsed Time");
                for (const auto& analysis : list) {
                    if (!analysis.data->elapsed_time_deque.empty()) {
                        std::vector<long long> temp(analysis.data->elapsed_time_deque.begin(), analysis.data->elapsed_time_deque.end());
                        ImPlot::PlotLine(analysis.name.c_str(),
                            temp.data(),
                            temp.size());
                    }
                }
                ImPlot::EndPlot();

                ImPlot::SetNextAxesLimits(ImAxis_X1, 0, buffer_size, ImPlotCond_Always);
                ImPlot::BeginPlot("Tick Time");
                for (const auto& analysis : list) {
                    if (!analysis.data->tick_time_deque.empty()) {
                        std::vector<long long> temp(analysis.data->tick_time_deque.begin(), analysis.data->tick_time_deque.end());
                        ImPlot::PlotLine(analysis.name.c_str(),
                            temp.data(),
                            temp.size());
                    }
                }
                ImPlot::EndPlot();

                ImPlot::SetNextAxesLimits(ImAxis_X1, 0, buffer_size, ImPlotCond_Always);
                ImPlot::BeginPlot("Recieved Bytes");
                if (!recieved_bytes_buffer.empty()) {
                    std::vector<int> temp(recieved_bytes_buffer.begin(), recieved_bytes_buffer.end());
                    ImPlot::PlotLine("Recieved Bytes",
                        temp.data(),
                        temp.size());
                }

                ImPlot::EndSubplots();
            }
        }
        ImGui::End();
    }

} // namespace performance_analysis