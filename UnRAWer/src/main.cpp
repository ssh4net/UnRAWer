/*
 * UnRAWer - camera raw batch processor
 * Copyright (c) 2024 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "settings.h"
#include "cli.h"
#include "do_process.h"
#include "gui.h"

#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 0

static void
glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}", error, description);
}

// dnd_glfw callbacks
void
onDragEnter(GLFWwindow* window, const dnd_glfw::DragEvent& event, void* userData)
{
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        spdlog::info("Drag Enter detected");
        SetDragging(true);
    }
}

void
onDragLeave(GLFWwindow* window, void* userData)
{
    spdlog::info("Drag Leave detected");
    SetDragging(false);
}

void
onDrop(GLFWwindow* window, const dnd_glfw::DropEvent& event, void* userData)
{
    spdlog::info("Drop detected");
    SetDragging(false);
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        std::vector<std::string> dropped_files = event.paths;
        spdlog::info("Dropped {} files.", dropped_files.size());
        StartProcessing(dropped_files);
    }
}

int
main(int argc, char* argv[])
{
    // --- Initial Setup ---
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%^[%l]%$<%t> %v");

    time_t timestamp;
    time(&timestamp);

    spdlog::info("UnRAWer {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    spdlog::info("Build from: {} {}", __DATE__, __TIME__);
    spdlog::info("Log started at: {}", ctime(&timestamp), "%Y-%m-%d %H:%M:%S");

    // --- CLI or GUI Branch ---
    if (argc > 1) {
        return cli_main(argc, argv);
    }

    // --- GUI Mode ---
    if (!loadSettings(settings, "unrw_config.toml")) {
        spdlog::error("Can not load [unrw_config.toml]. Using default settings.");
        settings.reSettings();
    }
    printSettings(settings);

    // --- GLFW and ImGui Initialization ---
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        return 1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Window configuration
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Fixed size
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);    // Always on top

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(500, 500, "UnRAWer ToolBox", NULL, NULL);
    if (window == NULL) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Center window
    GLFWmonitor* monitor    = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style      = ImGui::GetStyle();
    style.FrameBorderSize  = 0.0f;
    style.PopupBorderSize  = 0.0f;
    style.WindowBorderSize = 0.0f;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Hook to ensure all ImGui platform windows (popups, menus) are also floating/always-on-top
    // This fixes the issue where menu popups render behind the main window
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_originalCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;
    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport) {
        if (s_originalCreateWindow)
            s_originalCreateWindow(viewport);

        // Apply GLFW_FLOATING to all platform windows created by ImGui
        GLFWwindow* glfw_window = (GLFWwindow*)viewport->PlatformHandle;
        if (glfw_window) {
            glfwSetWindowAttrib(glfw_window, GLFW_FLOATING, GLFW_TRUE);
        }
    };

    // Load Fonts
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/FiraSans-Regular.otf", 16.0f);
    if (font == nullptr) {
        spdlog::warn("Failed to load font: fonts/FiraSans-Regular.otf");
    }

    // Initialize drag and drop via dnd_glfw
    dnd_glfw::Callbacks dnd_cbs;
    dnd_cbs.dragEnter  = onDragEnter;
    dnd_cbs.dragLeave  = onDragLeave;
    dnd_cbs.drop       = onDrop;
    dnd_cbs.dragCancel = onDragLeave;
    if (!dnd_glfw::init(window, dnd_cbs, nullptr)) {
        spdlog::error("Failed to initialize dnd_glfw");
    } else {
        spdlog::info("dnd_glfw initialized successfully");
    }

    // --- Main Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Call our UI rendering function
        RenderUI();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    dnd_glfw::shutdown(window);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}