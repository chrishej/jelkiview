#include <GLFW/glfw3.h>
#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <unordered_map>
#include <vector>

#include "ImGuiFileDialog.h"
#include "data_cursors.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "log_reader.h"
#include "main_window.h"
#include "math.h"
#include "rapidcsv.h"
#include "settings.h"

static void GlfwErrorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void RunInitFunctions() { settings::init(); }

static int sleep_time;

// Main code
int main(int, char**) {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(
        glfwGetPrimaryMonitor());  // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale),
                                          "Jelkiview ", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(
        main_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style
                      // scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi =
        main_scale;  // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this
                     // unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple
    // fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
    // font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those
    // errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher
    // quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to
    // write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the
    // "fonts/" folder. See Makefile.emscripten for details.
    // style.FontSizeBase = 16.0f;
    // ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/MonaspaceNeonFrozen-Regular.ttf", 18.0f);
    ImFont* font = io.Fonts->AddFontFromFileTTF("resources\\MonaspaceArgonFrozen-Regular.ttf", 18.0f);
    if (!font) {
        std::cerr << "Failed to load font!" << std::endl;
    }

    // Our state
    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    RunInitFunctions();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
        // wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
        // application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
        // application, or clear/overwrite your copy of the keyboard data. Generally you may always
        // pass all inputs to dear imgui, and hide them from your application based on those two
        // flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        glfwSetCursorEnterCallback(window, [](GLFWwindow* win, int entered) {
            (void)win;
            if (entered)
                sleep_time = 10;
            else
                sleep_time = 100;
        });

        ImGui_ImplGlfw_Sleep(sleep_time);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        MainWindow(io);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
