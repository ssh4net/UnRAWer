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

#include <limits>

#ifdef _WIN32
#    include "../resource.h"
#else
extern const unsigned char unrawer_embedded_font[];
extern const unsigned int unrawer_embedded_font_size;
#endif

#define VERSION_MAJOR 2
#define VERSION_MINOR 1
#define VERSION_PATCH 1

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

static bool
loadEmbeddedGuiFont(ImGuiIO& io, const ImWchar* glyphRanges)
{
#ifdef _WIN32
    HRSRC fontResource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_FIRA_SANS_REGULAR), MAKEINTRESOURCEW(10));
    if (fontResource == nullptr) {
        return false;
    }
    HGLOBAL fontHandle = LoadResource(nullptr, fontResource);
    if (fontHandle == nullptr) {
        return false;
    }
    const DWORD fontSize = SizeofResource(nullptr, fontResource);
    const void* fontData = LockResource(fontHandle);
    if (fontData == nullptr || fontSize == 0 || fontSize > static_cast<DWORD>(std::numeric_limits<int>::max())) {
        return false;
    }
    void* fontBytes         = const_cast<void*>(fontData);
    const int fontByteCount = static_cast<int>(fontSize);
#else
    if (unrawer_embedded_font_size == 0
        || unrawer_embedded_font_size > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        return false;
    }
    void* fontBytes         = const_cast<unsigned char*>(unrawer_embedded_font);
    const int fontByteCount = static_cast<int>(unrawer_embedded_font_size);
#endif

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(fontBytes, fontByteCount, 16.0f, &fontConfig, glyphRanges);
    if (font == nullptr) {
        return false;
    }
    io.FontDefault = font;
    return true;
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

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Window configuration
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Fixed size
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);    // Always on top

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(500, 500, "UnRAWer ToolBox", nullptr, nullptr);
    if (window == nullptr) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Center window
    GLFWmonitor* monitor    = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode != nullptr) {
        int windowWidth  = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
    }

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
    ImGuiPlatformIO& platform_io                          = ImGui::GetPlatformIO();
    static void (*s_originalCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;
    platform_io.Platform_CreateWindow                     = [](ImGuiViewport* viewport) {
        if (s_originalCreateWindow) {
            s_originalCreateWindow(viewport);
        }

        // Apply GLFW_FLOATING to all platform windows created by ImGui
        GLFWwindow* glfw_window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (glfw_window != nullptr) {
            glfwSetWindowAttrib(glfw_window, GLFW_FLOATING, GLFW_TRUE);
        }
    };

    // Load Fonts
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesDefault();
    if (!loadEmbeddedGuiFont(io, glyphRanges)) {
        spdlog::warn("Failed to load embedded FiraSans font; using default ImGui font.");
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

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Call our UI rendering function
        RenderUI();

        // Rendering
        ImGui::Render();
        ImDrawData* drawData     = ImGui::GetDrawData();
        const bool mainMinimized = drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f;

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        if (!mainMinimized && display_w > 0 && display_h > 0) {
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
        }

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        if (!mainMinimized && display_w > 0 && display_h > 0) {
            glfwSwapBuffers(window);
        }
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
