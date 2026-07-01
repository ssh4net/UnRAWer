#include "pch.h"
#include "gui.h"
#include "app_console.h"
#include "app_paths.h"
#include "settings.h"
#include "do_process.h"
#include "fileProcessor.h"
#include "pathutils.h"
#include "presets.h"
#include "preview.h"

#include <cctype>
#include <cstdio>

#ifdef UNRAWER_WITH_NFD
#    include <nfd.h>
#endif

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
static std::vector<PresetEntry> g_presets;
static bool g_presetsLoaded = false;
static char g_presetNameBuffer[128] = "Preset";
static bool g_nativeFileDialogsReady = false;
static bool g_openEncoderSettings = false;

enum class PendingPresetPopup {
    None,
    SaveLocal,
};

static PendingPresetPopup g_pendingPresetPopup = PendingPresetPopup::None;

static void PreviewSinkEnqueue(void* user, const char* out_file_path, int file_index1, int total_files);

bool
InitializeNativeFileDialogs()
{
#ifdef UNRAWER_WITH_NFD
    const nfdresult_t result = NFD_Init();
    if (result == NFD_OKAY) {
        g_nativeFileDialogsReady = true;
        return true;
    }

    const char* err = NFD_GetError();
    spdlog::warn("Native file dialog init failed: {}", err != nullptr ? err : "unknown error");
#endif
    g_nativeFileDialogsReady = false;
    return false;
}

void
ShutdownNativeFileDialogs()
{
#ifdef UNRAWER_WITH_NFD
    if (g_nativeFileDialogsReady) {
        NFD_Quit();
    }
#endif
    g_nativeFileDialogsReady = false;
}

bool
NativeFileDialogsAvailable()
{
    return g_nativeFileDialogsReady;
}

static GLFWwindow*
CurrentMainGlfwWindow()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->PlatformHandle == nullptr) {
        return nullptr;
    }
    return static_cast<GLFWwindow*>(viewport->PlatformHandle);
}

static void
SetMainWindowFloating(bool enabled)
{
    GLFWwindow* window = CurrentMainGlfwWindow();
    if (window != nullptr) {
        glfwSetWindowAttrib(window, GLFW_FLOATING, enabled ? GLFW_TRUE : GLFW_FALSE);
    }
}

static constexpr const char* kTiffCompressionLabels[] = { "ZIP/Deflate", "LZW", "PackBits", "None" };
static constexpr const char* kExrCompressionLabels[]  = {
    "ZIP", "ZIPS", "PIZ", "PXR24", "RLE", "B44", "B44A", "DWAA", "DWAB", "HTJ2K256", "HTJ2K32", "None",
};
static constexpr const char* kPngStrategyLabels[] = {
    "Default", "Filtered", "Huffman", "RLE", "Fixed", "PNG fast", "None",
};
static constexpr const char* kJpegSubsamplingLabels[] = { "4:4:4", "4:2:2", "4:2:0", "4:1:1" };

static void
DisabledDash()
{
    ImGui::BeginDisabled();
    ImGui::TextUnformatted("-");
    ImGui::EndDisabled();
}

static void
SetControlWidth(float width)
{
    ImGui::SetNextItemWidth(width);
}

static void
RenderEncoderSettingsWindow()
{
    if (!g_openEncoderSettings) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(820.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Encoder Settings", &g_openEncoderSettings)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("EncoderSettingsTable", 4,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Codec", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Extra", ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("TIFF");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##tiffCompression", &settings.tiffCompression, kTiffCompressionLabels,
                     IM_ARRAYSIZE(kTiffCompressionLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.tiffCompression == TiffCompression_Zip) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##tiffZipLevel", &settings.tiffZipLevel, 1, 9);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("OpenEXR");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##exrCompression", &settings.exrCompression, kExrCompressionLabels,
                     IM_ARRAYSIZE(kExrCompressionLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.exrCompression == ExrCompression_Zip || settings.exrCompression == ExrCompression_Zips) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##exrZipLevel", &settings.exrZipLevel, 1, 9);
        } else if (settings.exrCompression == ExrCompression_Dwaa || settings.exrCompression == ExrCompression_Dwab) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##exrDwaLevel", &settings.exrDwaLevel, 1, 100);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("PNG");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##pngStrategy", &settings.pngStrategy, kPngStrategyLabels, IM_ARRAYSIZE(kPngStrategyLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.pngStrategy != PngCompression_Fast && settings.pngStrategy != PngCompression_None) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##pngLevel", &settings.pngCompressionLevel, 0, 9);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("JPEG");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##jpegQuality", &settings.jpegQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        SetControlWidth(150.0f);
        ImGui::Combo("##jpegSubsampling", &settings.jpegSubsampling, kJpegSubsamplingLabels,
                     IM_ARRAYSIZE(kJpegSubsamplingLabels));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG-2000");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("OpenJPEG");
        ImGui::TableSetColumnIndex(2);
        DisabledDash();
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("HTJ2K");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("OpenJPH");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::DragFloat("##jpeg2000QStep", &settings.jpeg2000QStep, 0.001f, -1.0f, 10.0f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 or less uses reversible output.");
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG XL");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("JPEG XL");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##jpegxlQuality", &settings.jpegxlQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        SetControlWidth(92.0f);
        ImGui::SliderInt("Effort##jpegxlEffort", &settings.jpegxlEffort, 1, 9);
        ImGui::SameLine();
        SetControlWidth(92.0f);
        ImGui::SliderInt("Speed##jpegxlSpeed", &settings.jpegxlSpeed, 0, 4);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("HEIC");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("HEIC");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##heicQuality", &settings.heicQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::EndTable();
    }

    settings.tiffZipLevel        = std::clamp(settings.tiffZipLevel, 1, 9);
    settings.exrZipLevel         = std::clamp(settings.exrZipLevel, 1, 9);
    settings.exrDwaLevel         = std::clamp(settings.exrDwaLevel, 1, 100);
    settings.pngCompressionLevel = std::clamp(settings.pngCompressionLevel, 0, 9);
    settings.jpegQuality         = std::clamp(settings.jpegQuality, 1, 100);
    settings.jpeg2000QStep       = std::clamp(settings.jpeg2000QStep, -1.0f, 10.0f);
    settings.heicQuality         = std::clamp(settings.heicQuality, 1, 100);
    settings.jpegxlQuality       = std::clamp(settings.jpegxlQuality, 1, 100);
    settings.jpegxlEffort        = std::clamp(settings.jpegxlEffort, 1, 9);
    settings.jpegxlSpeed         = std::clamp(settings.jpegxlSpeed, 0, 4);

    ImGui::End();
}

static bool
SavePortablePresetDialogPath(const std::string& presetName, std::filesystem::path& outputPath)
{
#ifdef UNRAWER_WITH_NFD
    if (!g_nativeFileDialogsReady) {
        return false;
    }

    const std::string default_dir  = pathToUtf8(appRuntimePaths().user_preset_dir);
    const std::string default_name = presetName + ".unrwpreset";
    const nfdfilteritem_t filters[] = { { "UnRAWer portable preset", "unrwpreset" } };

    nfdchar_t* path = nullptr;
    SetMainWindowFloating(false);
    const nfdresult_t result
        = NFD_SaveDialog(&path, filters, 1, default_dir.c_str(), default_name.c_str());
    SetMainWindowFloating(true);

    if (result == NFD_OKAY && path != nullptr) {
        outputPath = pathFromUtf8(path);
        NFD_FreePath(path);
        return true;
    }
    if (path != nullptr) {
        NFD_FreePath(path);
    }
    if (result == NFD_ERROR) {
        const char* err = NFD_GetError();
        spdlog::error("Save preset dialog failed: {}", err != nullptr ? err : "unknown error");
    }
#endif
    return false;
}

static std::string
PresetNameFromCurrentSettings()
{
    std::string name = settings.dLutPreset.empty() ? "Preset" : settings.dLutPreset;
    for (char& c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '_' && c != '-' && c != ' ') {
            c = '_';
        }
    }
    while (!name.empty() && name.front() == ' ') {
        name.erase(name.begin());
    }
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    if (name.empty()) {
        name = "Preset";
    }
    return name;
}

static bool
OpenPresetDialogPath(std::filesystem::path& presetPath)
{
#ifdef UNRAWER_WITH_NFD
    if (!g_nativeFileDialogsReady) {
        return false;
    }

    const std::string default_dir = pathToUtf8(appRuntimePaths().user_preset_dir);
    const nfdfilteritem_t filters[] = { { "UnRAWer presets", "unrwpreset,toml" } };

    nfdchar_t* path = nullptr;
    SetMainWindowFloating(false);
    const nfdresult_t result = NFD_OpenDialog(&path, filters, 1, default_dir.c_str());
    SetMainWindowFloating(true);

    if (result == NFD_OKAY && path != nullptr) {
        presetPath = pathFromUtf8(path);
        NFD_FreePath(path);
        return true;
    }
    if (path != nullptr) {
        NFD_FreePath(path);
    }
    if (result == NFD_ERROR) {
        const char* err = NFD_GetError();
        spdlog::error("Open preset dialog failed: {}", err != nullptr ? err : "unknown error");
    }
#endif
    return false;
}

static void
SetStatusText(std::string text)
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_statusText = std::move(text);
}

static void
RefreshPresetList()
{
    g_presets       = ListPresets();
    g_presetsLoaded = true;
}

static void
QueuePresetNamePopup(PendingPresetPopup popup)
{
    if (!settings.dLutPreset.empty()) {
        std::snprintf(g_presetNameBuffer, sizeof(g_presetNameBuffer), "%s", settings.dLutPreset.c_str());
    } else {
        std::snprintf(g_presetNameBuffer, sizeof(g_presetNameBuffer), "%s", "Preset");
    }
    g_pendingPresetPopup = popup;
}

static void
RenderPresetPopups()
{
    if (g_pendingPresetPopup == PendingPresetPopup::SaveLocal) {
        ImGui::OpenPopup("Save Local Preset");
        g_pendingPresetPopup = PendingPresetPopup::None;
    }

    if (ImGui::BeginPopupModal("Save Local Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", g_presetNameBuffer, IM_ARRAYSIZE(g_presetNameBuffer));
        if (ImGui::Button("Save")) {
            if (SaveLocalPreset(settings, g_presetNameBuffer)) {
                RefreshPresetList();
                SetStatusText("Local preset saved.");
                ImGui::CloseCurrentPopup();
            } else {
                SetStatusText("Local preset save failed.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

}

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
    auto getter = [](void* data, int idx) -> const char* {
        const std::string* items = (const std::string*)data;
        return items[idx].c_str();
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
    settings.fileFormat                 = 8;  // PPM (index in list)
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
                if (loadSettings(settings, appRuntimePaths().user_config_file_string)) {
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

        // Presets
        if (ImGui::BeginMenu("Presets")) {
            if (!g_presetsLoaded) {
                RefreshPresetList();
            }

            if (ImGui::BeginMenu("Load")) {
                if (g_presets.empty()) {
                    ImGui::MenuItem("(No presets found)", NULL, false, false);
                } else {
                    for (const PresetEntry& preset : g_presets) {
                        const std::string label
                            = preset.name
                              + (preset.storage == PresetStorage::PortableZip ? "  [portable]" : "  [local]");
                        if (ImGui::MenuItem(label.c_str())) {
                            if (LoadPreset(preset, settings)) {
                                SetStatusText("Preset loaded: " + preset.name);
                                printSettings(settings);
                            } else {
                                SetStatusText("Preset load failed: " + preset.name);
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Save Local Preset...")) {
                QueuePresetNamePopup(PendingPresetPopup::SaveLocal);
            }
            if (ImGui::MenuItem("Export Portable Preset...", nullptr, false, NativeFileDialogsAvailable())) {
                const std::string presetName = PresetNameFromCurrentSettings();
                std::filesystem::path outputPath;
                if (SavePortablePresetDialogPath(presetName, outputPath)
                    && ExportPortablePresetToFile(settings, presetName, outputPath)) {
                    RefreshPresetList();
                    SetStatusText("Portable preset exported.");
                } else {
                    SetStatusText("Portable preset export canceled or failed.");
                }
            }
            if (ImGui::MenuItem("Load Preset File...", nullptr, false, NativeFileDialogsAvailable())) {
                std::filesystem::path presetPath;
                if (OpenPresetDialogPath(presetPath) && LoadPresetFile(presetPath, settings)) {
                    SetStatusText("Preset loaded: " + pathToUtf8(presetPath.filename()));
                    printSettings(settings);
                } else {
                    SetStatusText("Preset import canceled or failed.");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Refresh Presets")) {
                RefreshPresetList();
            }
            if (ImGui::MenuItem("Open Presets Folder")) {
                if (!OpenPresetsFolder()) {
                    SetStatusText("Could not open presets folder.");
                }
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
                MenuRadio("HTJ2K", settings.fileFormat, 5);
                MenuRadio("JPEG-XL", settings.fileFormat, 6);
                MenuRadio("HEIC", settings.fileFormat, 7);
                MenuRadio("PPM", settings.fileFormat, 8);
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
                SetAppConsoleEnabled(settings.conEnable);
            }
            if (ImGui::MenuItem("Encoder Settings...")) {
                g_openEncoderSettings = true;
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

    RenderPresetPopups();

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
        RenderEncoderSettingsWindow();

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
