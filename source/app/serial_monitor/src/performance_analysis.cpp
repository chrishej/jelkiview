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
    
    PerformanceData SerialMonitoringTaskData;
    PerformanceData FunLogFrameData;
    PerformanceData FunDeserializeLogData;
    PerformanceData TaskMainGui;

    uint16_t buffer_size = 2000;

    AnalysisStruct list[AnalysisIndex::NUM_ANALYSIS] = {
        {"SerialMonitoringTask",    &SerialMonitoringTaskData},
        {"FunLogFrame",             &FunLogFrameData},
        {"FunDeserializeLog",       &FunDeserializeLogData},
        {"TaskMainGui",             &TaskMainGui},
    };

    void LowPassFilter(PerformanceData& data) {
        const double alpha = 0.05;
        data.elapsed_time_LP = static_cast<std::chrono::milliseconds::rep>(
            alpha * data.elapsed_time + (1 - alpha) * data.elapsed_time_LP);
        data.tick_time_LP = static_cast<std::chrono::milliseconds::rep>(
            alpha * data.tick_time + (1 - alpha) * data.tick_time_LP);
    }

    void SaveToBuffer(PerformanceData& data) {
        data.elapsed_time_vec.push_back(data.elapsed_time);
        data.tick_time_vec.push_back(data.tick_time);

        // Only save last 100 samples
        if (data.elapsed_time_vec.size() > buffer_size) {
            data.elapsed_time_vec.erase(data.elapsed_time_vec.begin());
        }
        if (data.tick_time_vec.size() > buffer_size) {
            data.tick_time_vec.erase(data.tick_time_vec.begin());
        }
    }


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

            LowPassFilter(*data);
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
//!            if (ImGui::BeginTable("PerformanceTable", 3, 
//!                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
//!                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
//!                ImGui::TableSetupColumn("Elapsed Time [us]", ImGuiTableColumnFlags_WidthFixed, 150.0f);
//!                ImGui::TableSetupColumn("Tick Time [us]", ImGuiTableColumnFlags_WidthFixed, 150.0f);
//!                ImGui::TableHeadersRow();

//!                for (const auto& analysis : list) {
//!                    ImGui::TableNextRow();
//!                    ImGui::TableSetColumnIndex(0);
//!                    ImGui::TextUnformatted(analysis.name.c_str());
//!                    ImGui::TableSetColumnIndex(1);
//!                    ImGui::Text("%8lld", analysis.data->elapsed_time_LP);
//!                    ImGui::TableSetColumnIndex(2);
//!                    ImGui::Text("%8lld", analysis.data->tick_time_LP);
//!                }

//!                ImGui::EndTable();
//!            }

            if (ImPlot::BeginSubplots("##PerformanceSubplots", 2, 1, ImVec2(-1,-1), ImPlotSubplotFlags_LinkAllX)) {
                ImPlot::BeginPlot("Elapsed Time");
                for (const auto& analysis : list) {
                    if (!analysis.data->elapsed_time_vec.empty()) {
                        ImPlot::PlotLine(analysis.name.c_str(), 
                            analysis.data->elapsed_time_vec.data(), 
                            analysis.data->elapsed_time_vec.size());
                    }
                }
                ImPlot::EndPlot();

                ImPlot::SetNextAxesLimits(ImAxis_X1, 0, buffer_size, ImPlotCond_Always);
                ImPlot::BeginPlot("Tick Time");
                for (const auto& analysis : list) {
                    if (!analysis.data->tick_time_vec.empty()) {
                        ImPlot::PlotLine(analysis.name.c_str(), 
                            analysis.data->tick_time_vec.data(), 
                            analysis.data->tick_time_vec.size());
                    }
                }
                ImPlot::EndPlot();

                ImPlot::EndSubplots();
            }
        }
        ImGui::End();
    }

} // namespace performance_analysis