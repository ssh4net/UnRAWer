#include "pch.h"
#include "gui.h"
#include "settings.h"
#include "do_process.h"
#include "fileProcessor.h"
#include "preview.h"

#include <cstdio>

#ifndef GL_CLAMP_TO_EDGE
#    define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RGBA16F
#    define GL_RGBA16F 0x881A
#endif
#ifndef GL_HALF_FLOAT
#    define GL_HALF_FLOAT 0x140B
#endif

// Global state for processing
static std::atomic<bool> g_isProcessing = false;
static std::atomic<bool> g_inProcessingStage = false;
static std::atomic<float> g_progress    = 0.0f;
static std::mutex g_statusMutex;
static std::string g_statusText = "Waiting for user inputs...";
static std::atomic<bool> g_dragging     = false;
static std::atomic<bool> g_previewEnabled { true };
static PreviewQueue g_previewQueue;
static GLuint g_previewTexture = 0;
static int g_previewTextureW   = 0;
static int g_previewTextureH   = 0;
static double g_previewLastSwapTime = 0.0;
static int g_previewFileIndex1      = 0;
static int g_previewTotalFiles      = 0;
static int g_previewShownCount      = 0;

static void PreviewSinkEnqueue(void* user, const char* out_file_path, int file_index1, int total_files);

static void
ApplyPreviewSettingsFromConfig()
{
    const bool enabled = settings.previewEnable;
    g_previewEnabled   = enabled;
    g_previewQueue.SetEnabled(enabled);
    g_previewQueue.SetRequestQueueMaxSize(settings.previewQueueMax);
    g_previewQueue.SetReadyQueueMaxSize(settings.previewQueueMax);
    if (!enabled) {
        procGlobals.previewSink.enqueue.store(nullptr, std::memory_order_release);
        procGlobals.previewSink.user.store(nullptr, std::memory_order_release);
    } else if (g_inProcessingStage.load()) {
        procGlobals.previewSink.user.store(&g_previewQueue, std::memory_order_release);
        procGlobals.previewSink.enqueue.store(&PreviewSinkEnqueue, std::memory_order_release);
    }
    spdlog::debug("Preview: apply config enable={} queueMax={} minTimeMs={}",
                  enabled ? "true" : "false", settings.previewQueueMax, settings.previewMinTimeMs);
}

static void
PreviewSinkEnqueue(void* user, const char* out_file_path, int file_index1, int total_files)
{
    if (user == nullptr) {
        return;
    }
    spdlog::trace("Preview: sink enqueue '{}' ({}/{})", out_file_path != nullptr ? out_file_path : "",
                  file_index1, total_files);
    PreviewQueue* queue = static_cast<PreviewQueue*>(user);
    queue->EnqueuePath(out_file_path, file_index1, total_files);
}

static void
UploadPreviewTextureRGBA16F(const PreviewThumbnailRGBA16F& thumb)
{
    if (thumb.width <= 0 || thumb.height <= 0) {
        return;
    }
    if (thumb.pixels_rgba16f.empty()) {
        return;
    }

    spdlog::trace("Preview: upload {}x{} from '{}'", thumb.width, thumb.height, thumb.source_path);

    if (g_previewTexture == 0) {
        glGenTextures(1, &g_previewTexture);
        glBindTexture(GL_TEXTURE_2D, g_previewTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, g_previewTexture);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, thumb.width, thumb.height, 0, GL_RGBA, GL_HALF_FLOAT,
                 thumb.pixels_rgba16f.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    g_previewTextureW = thumb.width;
    g_previewTextureH = thumb.height;
    g_previewFileIndex1 = thumb.file_index1;
    g_previewTotalFiles = thumb.total_files;
}

static void
ResetToAwaitingState()
{
    g_inProcessingStage = false;
    g_progress          = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = "Waiting for user inputs...";
    }

    g_previewQueue.Clear();
    g_previewTextureW = 0;
    g_previewTextureH = 0;
    g_previewLastSwapTime = 0.0;
    g_previewFileIndex1   = 0;
    g_previewTotalFiles   = 0;
    g_previewShownCount   = 0;
    procGlobals.previewSink.enqueue.store(nullptr, std::memory_order_release);
    procGlobals.previewSink.user.store(nullptr, std::memory_order_release);

    spdlog::debug("Preview: reset to awaiting state");
}

void
SetDragging(bool dragging)
{
    g_dragging = dragging;
    if (dragging) {
        const bool busy           = g_isProcessing.load();
        const bool inProcessStage = g_inProcessingStage.load();
        if (!busy && inProcessStage) {
            ResetToAwaitingState();
        }
    }
}
bool
IsDragging()
{
    return g_dragging;
}

// Helper for ImGui::Combo with std::string array
bool
Combo(const char* label, int* current_item, const std::string* items, int items_count)
{
    auto getter = [](void* data, int idx, const char** out_text) {
        const std::string* items = (const std::string*)data;
        *out_text                = items[idx].c_str();
        return true;
    };
    return ImGui::Combo(label, current_item, getter, (void*)items, items_count);
}

void
StartProcessing(const std::vector<std::string>& files)
{
    if (g_isProcessing)
        return;

    g_isProcessing = true;
    g_inProcessingStage = true;
    g_progress     = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = "Processing " + std::to_string(files.size()) + " files...";
    }

    const bool previewEnabled = g_previewEnabled.load();
    g_previewQueue.SetEnabled(previewEnabled);
    g_previewQueue.SetRequestQueueMaxSize(settings.previewQueueMax);
    g_previewQueue.SetReadyQueueMaxSize(settings.previewQueueMax);
    g_previewQueue.Clear();
    g_previewTextureW = 0;
    g_previewTextureH = 0;
    g_previewLastSwapTime = 0.0;
    g_previewFileIndex1   = 0;
    g_previewTotalFiles   = 0;
    g_previewShownCount   = 0;
    if (previewEnabled) {
        procGlobals.previewSink.user.store(&g_previewQueue, std::memory_order_release);
        procGlobals.previewSink.enqueue.store(&PreviewSinkEnqueue, std::memory_order_release);
    } else {
        procGlobals.previewSink.enqueue.store(nullptr, std::memory_order_release);
        procGlobals.previewSink.user.store(nullptr, std::memory_order_release);
    }

    spdlog::debug("Preview: start processing (previewEnabled={})", previewEnabled ? "true" : "false");

    // Create a copy of the file list for the thread
    std::vector<std::string> taskFiles = files;

    std::thread([taskFiles]() {
        // In a real implementation, doProcessing would update progress via a callback
        // For now, we simulate progress or wait for it to finish
        bool success   = doProcessing(taskFiles, [](float p, std::string s) {
            g_progress   = p;
            {
                std::lock_guard<std::mutex> lock(g_statusMutex);
                g_statusText = std::move(s);
            }
        });
        g_isProcessing = false;
        g_progress     = success ? 1.0f : 0.0f;
        {
            std::lock_guard<std::mutex> lock(g_statusMutex);
            g_statusText = success ? "Everything Done!" : "Finished with errors.";
        }
        spdlog::debug("Preview: processing finished (success={})", success ? "true" : "false");
    }).detach();
}

// Helpers for Menu Radio Buttons
template<typename T>
void
MenuRadio(const char* label, T& variable, T value)
{
    if (ImGui::MenuItem(label, NULL, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, (int)value);
    }
}

// Special case for std::string vs c-string if needed, or just overload
void
MenuRadio(const char* label, std::string& variable, const std::string& value)
{
    if (ImGui::MenuItem(label, NULL, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, value);
    }
}

void
ZeroRaw()
{
    spdlog::info("Zeroing RAW processing and disable demosaic:");
    settings.rawRot                     = 0;   // Unrotated
    settings.dDemosaic                  = -2;  // RAW data
    settings.rawSpace                   = 0;   // RAW
    settings.denoise_mode               = 0;   // Disabled
    settings.rawParms.half_size         = false;
    settings.rawParms.use_auto_wb       = false;
    settings.rawParms.use_camera_wb     = false;
    settings.rawParms.use_camera_matrix = 0;  // Disabled
    settings.rawParms.highlight         = 1;  // Unclip
    settings.fileFormat                 = 7;  // PPM (index in list)
    settings.bitDepth                   = 1;  // 16 bits int
}

void
ZeroProc()
{
    settings.crop_mode  = -1;
    settings.perCamera  = false;
    settings.lutMode    = -1;  // Disabled
    settings.sharp_mode = -1;  // Disabled
}

void
AppMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    //ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

    if (ImGui::BeginMenuBar()) {
        // Files
        if (ImGui::BeginMenu("Files")) {
            if (ImGui::MenuItem("Reload Config")) {
                if (loadSettings(settings, "unrw_config.toml")) {
                    ApplyPreviewSettingsFromConfig();
                    printSettings(settings);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                exit(0);
            }
            ImGui::EndMenu();
        }

        // RAW
        if (ImGui::BeginMenu("RAW")) {
            if (ImGui::MenuItem("Disable RAW")) {
                ZeroRaw();
            }

            if (ImGui::BeginMenu("RAW Rotation")) {
                MenuRadio("Auto EXIF", settings.rawRot, -1);
                MenuRadio("0 Horizontal", settings.rawRot, 0);
                MenuRadio("180 Horizontal", settings.rawRot, 3);
                MenuRadio("-90 Vertical", settings.rawRot, 5);
                MenuRadio("+90 Vertical", settings.rawRot, 6);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Demosaic")) {
                MenuRadio("RAW data", settings.dDemosaic, -2);
                MenuRadio("none", settings.dDemosaic, -1);
                MenuRadio("linear", settings.dDemosaic, 0);
                MenuRadio("VNG", settings.dDemosaic, 1);
                MenuRadio("PPG", settings.dDemosaic, 2);
                MenuRadio("AHD", settings.dDemosaic, 3);
                MenuRadio("DCB", settings.dDemosaic, 4);
                MenuRadio("DHT", settings.dDemosaic, 11);
                MenuRadio("AAHD", settings.dDemosaic, 12);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("RAW ColorSpace")) {
                const char* spaces[] = { "Raw", "sRGB", "sRGB-linear", "Adobe",  "Wide", "ProPhoto", "ProPhoto-linear",
                                         "XYZ", "ACES", "DCI-P3",      "Rec2020" };
                for (int i = 0; i < 11; i++)
                    MenuRadio(spaces[i], settings.rawSpace, (uint)i);
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Denoise")) {
                MenuRadio("Disabled", settings.denoise_mode, (uint)0);
                MenuRadio("Wavelet", settings.denoise_mode, (uint)1);
                MenuRadio("FBDD", settings.denoise_mode, (uint)2);
                MenuRadio("Both", settings.denoise_mode, (uint)3);
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Auto WB", NULL, settings.rawParms.use_auto_wb)) {
                settings.rawParms.use_auto_wb = !settings.rawParms.use_auto_wb;
            }
            if (ImGui::MenuItem("Camera WB", NULL, settings.rawParms.use_camera_wb)) {
                settings.rawParms.use_camera_wb = !settings.rawParms.use_camera_wb;
            }

            if (ImGui::BeginMenu("Camera Matrix")) {
                MenuRadio("Don't use", settings.rawParms.use_camera_matrix, 0);
                MenuRadio("DNG Embedded", settings.rawParms.use_camera_matrix, 1);
                MenuRadio("Always", settings.rawParms.use_camera_matrix, 2);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Highlights")) {
                MenuRadio("Clip", settings.rawParms.highlight, 0);
                MenuRadio("Unclip", settings.rawParms.highlight, 1);
                MenuRadio("Blend", settings.rawParms.highlight, 2);
                MenuRadio("Rebuild", settings.rawParms.highlight, 3);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Half Resolution", NULL, settings.rawParms.half_size)) {
                settings.rawParms.half_size = !settings.rawParms.half_size;
            }

            ImGui::EndMenu();
        }

        // Processing
            if (ImGui::BeginMenu("Processing")) {
                if (ImGui::MenuItem("Disable Processing")) {
                    ZeroProc();
                }

            {
                bool previewEnabled = g_previewEnabled.load();
                if (ImGui::MenuItem("Enable Preview", NULL, previewEnabled)) {
                    previewEnabled   = !previewEnabled;
                    g_previewEnabled = previewEnabled;
                    g_previewQueue.SetEnabled(previewEnabled);
                    settings.previewEnable = previewEnabled;
                    if (previewEnabled && g_inProcessingStage.load()) {
                        procGlobals.previewSink.user.store(&g_previewQueue, std::memory_order_release);
                        procGlobals.previewSink.enqueue.store(&PreviewSinkEnqueue, std::memory_order_release);
                    } else {
                        procGlobals.previewSink.enqueue.store(nullptr, std::memory_order_release);
                        procGlobals.previewSink.user.store(nullptr, std::memory_order_release);
                    }
                }
            }

            if (ImGui::BeginMenu("Crop")) {
                MenuRadio("Disabled", settings.crop_mode, -1);
                MenuRadio("Auto", settings.crop_mode, 0);
                MenuRadio("Forced", settings.crop_mode, 1);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Per Camera", NULL, settings.perCamera)) {
                settings.perCamera = !settings.perCamera;
            }

            if (ImGui::BeginMenu("LUT transform")) {
                MenuRadio("Off", settings.lutMode, -1);
                MenuRadio("Smart", settings.lutMode, 0);
                MenuRadio("Forced", settings.lutMode, 1);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("LUT Presets")) {
                if (!settings.lut_Preset.empty()) {
                    for (auto& [key, value] : settings.lut_Preset) {
                        MenuRadio(key.c_str(), settings.dLutPreset, key);
                    }
                } else {
                    ImGui::MenuItem("(No presets found)", NULL, false, false);
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Unsharp")) {
                MenuRadio("Off", settings.sharp_mode, -1);
                MenuRadio("Smart", settings.sharp_mode, 0);
                MenuRadio("Forced", settings.sharp_mode, 1);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Unsharp kernel")) {
                for (int i = 0; i < 13; i++) {
                    if (ImGui::MenuItem(settings.sharp_kerns[i].c_str(), NULL, settings.sharp_kernel == i)) {
                        settings.sharp_kernel = i;
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        // Outputs
        if (ImGui::BeginMenu("Outputs")) {
            if (ImGui::BeginMenu("Floats type")) {
                MenuRadio("Unsigned", settings.rangeMode, (uint)0);
                MenuRadio("Signed", settings.rangeMode, (uint)1);
                MenuRadio("Signed > Unsigned", settings.rangeMode, (uint)2);
                MenuRadio("Unsigned > Signed", settings.rangeMode, (uint)3);
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Formats")) {
                MenuRadio("Original", settings.fileFormat, -1);
                MenuRadio("TIFF", settings.fileFormat, 0);
                MenuRadio("OpenEXR", settings.fileFormat, 1);
                MenuRadio("PNG", settings.fileFormat, 2);
                MenuRadio("JPEG", settings.fileFormat, 3);
                MenuRadio("JPEG2000", settings.fileFormat, 4);
                MenuRadio("JPEG-XL", settings.fileFormat, 5);
                MenuRadio("HEIC", settings.fileFormat, 6);
                MenuRadio("PPM", settings.fileFormat, 7);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Bits Depth")) {
                MenuRadio("Original", settings.bitDepth, -1);
                MenuRadio("8 bits int", settings.bitDepth, 0);
                MenuRadio("16 bits int", settings.bitDepth, 1);
                MenuRadio("32 bits int", settings.bitDepth, 2);
                MenuRadio("64 bits int", settings.bitDepth, 3);
                MenuRadio("16 bits float", settings.bitDepth, 4);
                MenuRadio("32 bits float", settings.bitDepth, 5);
                MenuRadio("64 bits float", settings.bitDepth, 6);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Export Subfolders", NULL, settings.useSbFldr)) {
                settings.useSbFldr = !settings.useSbFldr;
            }

            ImGui::EndMenu();
        }

        // Settings
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Enable Console", NULL, settings.conEnable)) {
                settings.conEnable = !settings.conEnable;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Print Settings")) {
                printSettings(settings);
            }
            ImGui::EndMenu();
        }

        // Debug
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::BeginMenu("Verbosity")) {
                MenuRadio("0 - Fatal", settings.verbosity, (uint)0);
                MenuRadio("1 - Error", settings.verbosity, (uint)1);
                MenuRadio("2 - Warning", settings.verbosity, (uint)2);
                MenuRadio("3 - Info", settings.verbosity, (uint)3);
                MenuRadio("4 - Debug", settings.verbosity, (uint)4);
                MenuRadio("5 - Trace", settings.verbosity, (uint)5);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::PopStyleVar(3);
}

void
RenderUI()
{
    const int verbosity = std::clamp<int>(static_cast<int>(settings.verbosity), 0, 5);
    spdlog::set_level(static_cast<spdlog::level::level_enum>(5 - verbosity));

    static bool s_previewInit = false;
    if (!s_previewInit) {
        s_previewInit = true;
        ApplyPreviewSettingsFromConfig();
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));   // #101010
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));  // #181818
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    if (ImGui::Begin("UnRAWer Main Window", nullptr, window_flags)) {
        AppMenuBar();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        // --- Main Content Area ---
        // Calculate remaining size for Drop Area and Footer
        ImVec2 regionSize = ImGui::GetContentRegionAvail();

        // Footer (ProgressBar + Status) height
        // ProgressBar (20) + Padding (4) + Text (FontSize * lines)
        const float pad               = 10.0f;
        const float space             = 8.0f;
        const float progressBarHeight = 16.0f;
        const float statusHeight      = 100.0f;
        const float content_width     = regionSize.x - 2.0f * pad;
        float cusror_y                = ImGui::GetFrameHeight() + pad * 3.0f;

        // Drop Area
        // Draw a background box for the drop area
        ImVec2 dropAreaSize = ImVec2(content_width,
                                     regionSize.y - progressBarHeight - statusHeight - 2.0f * pad - 2.0f * space);
        const bool inProcessingStage = g_inProcessingStage.load();
        const bool isBusy            = g_isProcessing.load();
        const bool previewEnabled    = g_previewEnabled.load();
        const int previewSquareSize = std::max(1, static_cast<int>(dropAreaSize.y + 0.5f));
        g_previewQueue.SetTargetSquareSize(previewSquareSize);
        if (inProcessingStage && previewEnabled) {
            const int minTimeMs      = std::max(0, settings.previewMinTimeMs);
            const double minTimeSec  = static_cast<double>(minTimeMs) / 1000.0;
            const double now         = ImGui::GetTime();
            const bool haveTexture   = g_previewTexture != 0 && g_previewTextureW > 0 && g_previewTextureH > 0;

            if (!isBusy) {
                PreviewThumbnailRGBA16F thumb;
                bool got = false;
                while (g_previewQueue.TryConsume(&thumb)) {
                    got = true;
                }
                if (got) {
                    spdlog::debug("Preview: flush to last '{}' {}x{}", thumb.source_path, thumb.width, thumb.height);
                    UploadPreviewTextureRGBA16F(thumb);
                    ++g_previewShownCount;
                    g_previewLastSwapTime = now;
                }
            } else if (!haveTexture || (now - g_previewLastSwapTime) >= minTimeSec) {
                PreviewThumbnailRGBA16F thumb;
                if (g_previewQueue.TryConsume(&thumb)) {
                    spdlog::debug("Preview: consume '{}' {}x{}", thumb.source_path, thumb.width, thumb.height);
                    UploadPreviewTextureRGBA16F(thumb);
                    ++g_previewShownCount;
                    g_previewLastSwapTime = now;
                }
            }
        }

        {
            ImGui::SetCursorPos(ImVec2(pad, cusror_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094f, 0.094f, 0.094f, 1.0f));
            ImGui::BeginChild("DropArea", dropAreaSize, false);
            if (!inProcessingStage) {
                // Centered Text
                const char* dropText = "Drag & drop files here";
                ImGui::SetWindowFontScale(1.5f);  // Larger text
                ImVec2 textSize  = ImGui::CalcTextSize(dropText);
                ImVec2 childSize = ImGui::GetWindowSize();
                ImGui::SetCursorPos(ImVec2((childSize.x - textSize.x) * 0.5f, (childSize.y - textSize.y) * 0.5f));
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", dropText);
                ImGui::SetWindowFontScale(1.0f);
            } else {
                const bool havePreview    = previewEnabled && g_previewTexture != 0 && g_previewTextureW > 0
                                         && g_previewTextureH > 0;
                if (havePreview) {
                    ImVec2 childSize         = ImGui::GetWindowSize();
                    const float squareSize   = childSize.y;
                    const float squareOffset = (childSize.x - squareSize) * 0.5f;

                    const float imgW  = static_cast<float>(g_previewTextureW);
                    const float imgH  = static_cast<float>(g_previewTextureH);
                    const float scale = std::min(squareSize / imgW, squareSize / imgH);
                    const ImVec2 drawSize { imgW * scale, imgH * scale };

                    const ImVec2 cursorPos { squareOffset + (squareSize - drawSize.x) * 0.5f,
                                             (squareSize - drawSize.y) * 0.5f };
                    ImGui::SetCursorPos(cursorPos);

                    ImTextureRef texRef((ImTextureID)(uintptr_t)g_previewTexture);
                    ImGui::Image(texRef, drawSize);

                    {
                        const int previewLabel = std::max(1, g_previewShownCount);
                        char label[32];
                        std::snprintf(label, sizeof(label), "%d", previewLabel);

                        const ImVec2 winPos = ImGui::GetWindowPos();
                        const float pad     = 10.0f;
                        const ImVec2 textPos { winPos.x + pad, winPos.y + pad };
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddText(textPos, IM_COL32(255, 255, 255, 220), label);
                    }
                } else {
                    // Centered Text
                    const char* procText = "Processing files...";
                    ImGui::SetWindowFontScale(1.5f);  // Larger text
                    ImVec2 textSize  = ImGui::CalcTextSize(procText);
                    ImVec2 childSize = ImGui::GetWindowSize();
                    ImGui::SetCursorPos(ImVec2((childSize.x - textSize.x) * 0.5f, (childSize.y - textSize.y) * 0.5f));
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", procText);
                    ImGui::SetWindowFontScale(1.0f);
                }
            }


            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        cusror_y += dropAreaSize.y + space;

        // --- Footer ---

        // Progress Bar
        {
            ImGui::SetCursorPos(ImVec2(pad, cusror_y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.02f, 0.72f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::ProgressBar(g_progress, ImVec2(content_width, progressBarHeight), "");
            ImGui::PopStyleColor(2);
        }

        cusror_y += progressBarHeight + space;

        // Status Text
        {
            ImGui::SetCursorPos(ImVec2(10.0f, cusror_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094f, 0.094f, 0.094f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
            ImGui::BeginChild("StatusArea", ImVec2(content_width, statusHeight),
                              ImGuiChildFlags_AlwaysUseWindowPadding);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.88f, 0.88f, 1.0f));
            std::string statusTextCopy;
            {
                std::lock_guard<std::mutex> lock(g_statusMutex);
                statusTextCopy = g_statusText;
            }
            ImGui::TextUnformatted(statusTextCopy.c_str());

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // Drag and Drop Overlay (DrawList based)
        if (g_dragging) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImVec2 pMin          = viewport->Pos;
            ImVec2 pMax          = ImVec2(pMin.x + viewport->Size.x, pMin.y + viewport->Size.y);

            drawList->AddRectFilled(pMin, pMax, IM_COL32(0, 0, 0, 192));  // Semi-transparent black

            const char* text = "DROP FILES TO START";
            ImFont* font     = ImGui::GetFont();
            float fontSize   = ImGui::GetFontSize() * 3.0f;

            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            ImVec2 textPos  = ImVec2(pMin.x + (viewport->Size.x - textSize.x) * 0.5f,
                                     pMin.y + (viewport->Size.y - textSize.y) * 0.5f);

            drawList->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 255), text);
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();  // End Main Window

    ImGui::PopStyleVar(2);    // Pop WindowPadding
    ImGui::PopStyleColor(2);  // Pop WindowBg and MenuBarBg
}
