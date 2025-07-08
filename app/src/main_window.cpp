#include <stdio.h>
#include <GLFW/glfw3.h>
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "math.h"
#include <numbers>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include "rapidcsv.h"
#include <filesystem>
#include "ImGuiFileDialog.h"
#include "settings.h"
#include <limits>
#include "log_reader.h"
#include "data_cursors.h"
#include "main_window.h"
#include "licenses.h"

std::vector<double> decimate(
    const std::vector<double>& input, 
    int timeMinIdx,
    int timeMaxIdx, 
    int M);
void updateLayout();
void manageSubplot(
    int subplotIndex, 
    std::unordered_map<std::string, std::vector<double>> * const signals,
    std::vector<std::unordered_map<std::string, bool>> * subplotsMap );



std::vector<double> decimate(
    const std::vector<double>& input, 
    int timeMinIdx,
    int timeMaxIdx, 
    int M) 
{
    std::vector<double> result;
    result.push_back(input[0]);
    
    size_t i;
    for (i = std::max(timeMinIdx, 1); i < std::min(input.size()-1, (size_t)timeMaxIdx); i += M) 
    {
        result.push_back(input[i]);
    }

    if (i <= input.size()-1)
    {
        result.push_back(input[input.size()-1]);
    }

    return result;
}

void updateLayout()
{
    layout.clear();

    for (size_t i = 0; i < subplotsMap.size(); i++)
    {
        layout.push_back({});
        for (const auto& [signalName, isEnabled] : subplotsMap[i]) 
        {
            if (isEnabled)
            {
                layout[i].push_back(signalName);
            }
        }
    }

    return;
}

void manageSubplot(
    int subplotIndex, 
    std::unordered_map<std::string, std::vector<double>> * const signals,
    std::vector<std::unordered_map<std::string, bool>> * subplotsMapLoc )
{
    ImGui::Text("Managing subplot %d", subplotIndex);
    if (ImGui::Button("Signals"))
    {
        ImGui::OpenPopup("SignalsPopup");
    }
    if (ImGui::BeginPopup("SignalsPopup")) 
    {
        auto& signalMap = (*subplotsMapLoc)[subplotIndex];
        for (auto& [signalName, isEnabled] : signalMap) {
            if (ImGui::Checkbox(signalName.c_str(), &isEnabled))
            {
                updateLayout();
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::Button("Remove Subplot"))
    {
        if (subplotIndex >= 0 && (size_t)subplotIndex < subplotsMapLoc->size()) 
        {
            subplotsMapLoc->erase(subplotsMapLoc->begin() + subplotIndex);
        }
    }

    if (ImGui::Button("Insert Subplot"))
    {
        if (subplotIndex >= 0 && (size_t)subplotIndex < subplotsMapLoc->size()) 
        {
            auto insertAtIt = subplotsMapLoc->begin() + subplotIndex+1;
            int  newIndex   = std::distance(subplotsMapLoc->begin(), insertAtIt);

            subplotsMapLoc->insert(insertAtIt, std::unordered_map<std::string, bool>{});

            for (const auto& [signalName, _] : (*signals)) 
            {
                (*subplotsMapLoc)[newIndex][signalName] = false;
            }
        }
    }
    return;
}


void MainWindow(ImGuiIO& io)
{
    static ImPlotRange xRange;

    const int id_vline_1_base = 100;
    const int id_vline_2_base = 200;
    const int ID_VLINE_1 = id_vline_1_base;
    const int ID_VLINE_2 = id_vline_2_base;

    ImGui::BeginMainMenuBar();
    LogReadButton();
    //settingsButton();
    ShowSettingsButton();
    Licenses();
    ImGui::EndMainMenuBar();


    ImGui::SetNextWindowPos(ImVec2(0,ImGui::GetCursorPosY()-10));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("FullScreenWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

    float const valueColumnSize = 400;

    double samplesInRange = (xRange.Max-xRange.Min)/(GetData()->time[1]-GetData()->time[0]);
    double maxSamplesInView = 10e3;
    double decimationFactor = std::max(samplesInRange/maxSamplesInView, 1.0);
    auto visibleMinIt = std::lower_bound(GetData()->time.begin(), GetData()->time.end(), xRange.Min);
    int visibleMinIdx = std::distance(GetData()->time.begin(), visibleMinIt);
    visibleMinIdx = std::max(visibleMinIdx-1, 0);
    auto visibleMaxIt = std::upper_bound(GetData()->time.begin(), GetData()->time.end(), xRange.Max);
    int visibleMaxIdx = std::distance(GetData()->time.begin(), visibleMaxIt);
    //ImGui::Text("X Min: %.2f, X Max: %.2f, minIdx: %d, maxIdx: %d, decimationFac: %.2f", xRange.Min, xRange.Max, visibleMinIdx, visibleMaxIdx, decimationFactor);

    ImPlotSubplotFlags const subplotFlags = ImPlotSubplotFlags_LinkAllX;
    uint8_t const len = subplotsMap.size();
    ImPlot::BeginSubplots("",len,1, ImVec2(io.DisplaySize.x-valueColumnSize, io.DisplaySize.y-35), subplotFlags);
            
    std::vector<float> plotYPos;
    for (int i = 0; i < len; ++i) 
    {
        std::string title = "";//"Plot " + std::to_string(i);

        ImPlotAxisFlags_ xFlags = ImPlotAxisFlags_None;
        ImPlotAxisFlags_ yFlags = ImPlotAxisFlags_None;
        if (i < len-1)
        {
            xFlags = ImPlotAxisFlags_NoTickLabels;
        }

        if (GetSettings()->auto_size_y)
        {
            yFlags = ImPlotAxisFlags_AutoFit;
        }

        plotYPos.push_back(ImGui::GetCursorPosY());

        ImPlot::BeginPlot(title.c_str(), ImVec2(-1,0), 0);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxis(ImAxis_X1, nullptr, xFlags);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, yFlags);

        ImPlot::DragLineX(ID_VLINE_1, &vLine1Pos, ImVec4(1, 1, 0, 1), 2.0f);
        ImPlot::DragLineX(ID_VLINE_2, &vLine2Pos, ImVec4(0.5, 1, 0.5, 1), 2.0f);

        if (i == 0)
        {
            ImVec2 labelPos = ImPlot::PlotToPixels(ImVec2(vLine1Pos, 0)); // y doesn't matter for label
            labelPos.y = plotYPos[0] + 15 + 10.0f; // offset label above the line
            labelPos.x += 5.0f;
            ImGui::GetWindowDrawList()->AddText(labelPos, IM_COL32_WHITE, "1");
            labelPos = ImPlot::PlotToPixels(ImVec2(vLine2Pos, 0)); // y doesn't matter for label
            labelPos.y = plotYPos[0] + 15 + 10.0f; // offset label above the line
            labelPos.x += 5.0f;
            ImGui::GetWindowDrawList()->AddText(labelPos, IM_COL32_WHITE, "2");
        }
                
        //for (auto signalNameIt = subplots[i].begin(); signalNameIt != subplots[i].end(); signalNameIt++)
        for (auto& [signalName, isEnabled] : subplotsMap[i])
        {
            if (isEnabled)
            {
                //ImPlot::PlotStairs((*signalNameIt).c_str(), time.data(), signals[*signalNameIt].data(), samples);
                //std::vector<double> timeDec = decimate(timeVec, timeVec, xRange.Min, xRange.Max, (int)(decimationFactor+0.5f));
                //std::vector<double> val     = decimate(signals[signalName], timeVec, xRange.Min, xRange.Max, (int)(decimationFactor+0.5f));
                std::vector<double> timeDec = decimate(GetData()->time, visibleMinIdx, visibleMaxIdx, (int)(decimationFactor+0.5f));
                std::vector<double> val     = decimate(GetData()->signals[signalName], visibleMinIdx, visibleMaxIdx, (int)(decimationFactor+0.5f));
                //std::vector<double> timeDec = GetData()->time;
                //std::vector<double> val     = signals[signalName];
                ImPlot::PlotStairs(
                    signalName.c_str(), 
                    timeDec.data(), 
                    val.data(), 
                    val.size());
                }
        }

        //ImPlot::PlotStairs("", time, signals[subplots[i][0]], samples);
        //ImPlot::PlotStairs("", time, signalValues[(i+1)&0x1], samples);
        xRange = ImPlot::GetPlotLimits().X;
        ImPlot::EndPlot();
    }

    ImPlot::EndSubplots();

    auto vline1It = std::upper_bound(GetData()->time.begin(), GetData()->time.end(), vLine1Pos);
    if (vline1It != GetData()->time.begin()) --vline1It;
    int vline1Idx = std::distance(GetData()->time.begin(), vline1It);

    auto vline2It = std::upper_bound(GetData()->time.begin(), GetData()->time.end(), vLine2Pos);
    if (vline2It != GetData()->time.begin()) --vline2It;
    int vline2Idx = std::distance(GetData()->time.begin(), vline2It);

    ImGui::SameLine();
    ImGui::BeginGroup();
    for (int i = 0; (size_t)i < subplotsMap.size(); ++i) 
    {
        std::unordered_map<std::string, bool> subplotsMapI = subplotsMap[i];
        ImGui::SetCursorPosY(plotYPos[i]);
        ImGui::BeginTable(("Table##" + std::to_string(i)).c_str(), 3);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, valueColumnSize/2.5f);
        ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthFixed, (valueColumnSize-valueColumnSize/2.0f)/2.0f);
        ImGui::TableSetupColumn("Value 2", ImGuiTableColumnFlags_WidthStretch); // fills remaining space

        // Header row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("Signal");
                
        ImGui::SameLine();
        std::string popupId = "ManagePopup##" + std::to_string(i);
        ImGui::PushID(i);
        if (ImGui::Button("Manage"))
        {
            ImGui::OpenPopup(popupId.c_str());
        }
        if (ImGui::BeginPopup(popupId.c_str())) 
        {
            //ImGui::Text("Managing subplot %d", i);
            manageSubplot(i, &(GetData()->signals), &subplotsMap);
            ImGui::EndPopup();
        }
        ImGui::PopID(); 

        ImGui::TableSetColumnIndex(1); ImGui::Text("Value 1");
        ImGui::TableSetColumnIndex(2); ImGui::Text("Value 2");

        // Data rows
        //for (const std::string& name : subplots[i])
        for (auto& [signalName, isEnabled] : subplotsMapI)
        {
            if (isEnabled)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", signalName.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", GetData()->signals[signalName][vline1Idx]);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", GetData()->signals[signalName][vline2Idx]);
            }
        }

        ImGui::EndTable();
    }


    ImGui::EndGroup();

    ImGui::End();

}