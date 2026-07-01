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
#include "app_paths.h"
#include "pathutils.h"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#    include <shlobj_core.h>
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#endif

#ifndef UNRAWER_INSTALL_DATADIR
#    define UNRAWER_INSTALL_DATADIR ""
#endif

namespace fs = std::filesystem;

static AppRuntimePaths g_app_paths;
static bool g_app_paths_initialized   = false;
static bool g_app_user_files_prepared = false;

static bool
pathExists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

static bool
pathIsRegularFile(const fs::path& path)
{
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

static bool
pathIsDirectory(const fs::path& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}

static fs::path
homeDirectory()
{
#ifdef _WIN32
    const char* user_profile = std::getenv("USERPROFILE");
    if (user_profile != nullptr && user_profile[0] != '\0') {
        return pathFromUtf8(user_profile);
    }
#endif
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return pathFromUtf8(home);
    }
    return {};
}

#ifdef _WIN32
static fs::path
knownFolderPath(REFKNOWNFOLDERID folder_id)
{
    PWSTR folder_path = nullptr;
    const HRESULT hr  = SHGetKnownFolderPath(folder_id, KF_FLAG_CREATE, nullptr, &folder_path);
    if (FAILED(hr) || folder_path == nullptr) {
        return {};
    }

    fs::path path(folder_path);
    CoTaskMemFree(folder_path);
    return path;
}
#endif

static fs::path
platformUserDirectory()
{
#ifdef _WIN32
    fs::path roaming_app_data = knownFolderPath(FOLDERID_RoamingAppData);
    if (!roaming_app_data.empty()) {
        return roaming_app_data / "UnRAWer";
    }
    fs::path home = homeDirectory();
    if (!home.empty()) {
        return home / "AppData" / "Roaming" / "UnRAWer";
    }
#elif defined(__APPLE__)
    fs::path home = homeDirectory();
    if (!home.empty()) {
        return home / "Library" / "Application Support" / "UnRAWer";
    }
#else
    const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != nullptr && xdg_config_home[0] != '\0') {
        return pathFromUtf8(xdg_config_home) / "unrawer";
    }
    fs::path home = homeDirectory();
    if (!home.empty()) {
        return home / ".config" / "unrawer";
    }
#endif
    return fs::current_path() / "UnRAWer";
}

static fs::path
executableDirectory()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0 && size < buffer.size()) {
        return fs::path(buffer.data()).parent_path();
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    if (size > 0) {
        std::vector<char> buffer(size + 1);
        if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
            std::error_code ec;
            fs::path path = fs::weakly_canonical(pathFromUtf8(buffer.data()), ec);
            if (ec) {
                path = pathFromUtf8(buffer.data());
            }
            return path.parent_path();
        }
    }
#else
    std::vector<char> buffer(4096);
    const ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (size > 0) {
        buffer[static_cast<size_t>(size)] = '\0';
        return pathFromUtf8(buffer.data()).parent_path();
    }
#endif
    return fs::current_path();
}

static fs::path
firstExistingFile(const std::vector<fs::path>& candidates)
{
    for (const fs::path& candidate : candidates) {
        if (!candidate.empty() && pathIsRegularFile(candidate)) {
            return candidate;
        }
    }
    return {};
}

static fs::path
firstExistingDirectory(const std::vector<fs::path>& candidates)
{
    for (const fs::path& candidate : candidates) {
        if (!candidate.empty() && pathIsDirectory(candidate)) {
            return candidate;
        }
    }
    return {};
}

static fs::path
installedDataDirectory()
{
    const char* install_data_dir = UNRAWER_INSTALL_DATADIR;
    if (install_data_dir != nullptr && install_data_dir[0] != '\0') {
        return pathFromUtf8(install_data_dir);
    }
    return {};
}

static unsigned long
currentProcessId()
{
#ifdef _WIN32
    return static_cast<unsigned long>(_getpid());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

static void
initializeAppRuntimePaths()
{
    if (g_app_paths_initialized) {
        return;
    }

    const fs::path exe_dir          = executableDirectory();
    const fs::path install_data_dir = installedDataDirectory();
    const fs::path prefix_share_dir = exe_dir.parent_path() / "share" / "unrawer";
#ifdef __APPLE__
    const fs::path macos_resources_dir = exe_dir.parent_path() / "Resources";
#else
    const fs::path macos_resources_dir;
#endif

    g_app_paths.user_dir         = platformUserDirectory();
    g_app_paths.user_config_file = g_app_paths.user_dir / "unrw_config.toml";
    g_app_paths.user_lut_dir     = g_app_paths.user_dir / "LUTs";
    g_app_paths.user_preset_dir  = g_app_paths.user_dir / "presets";
    g_app_paths.temp_preset_dir  = fs::temp_directory_path() / "UnRAWer" / "preset_cache"
                                  / std::to_string(currentProcessId());
    g_app_paths.imgui_ini_file   = g_app_paths.user_dir / "imgui.ini";

    std::vector<fs::path> config_candidates;
    config_candidates.push_back(exe_dir / "unrw_config.toml");
    if (!macos_resources_dir.empty()) {
        config_candidates.push_back(macos_resources_dir / "unrw_config.toml");
    }
    config_candidates.push_back(prefix_share_dir / "unrw_config.toml");
    if (!install_data_dir.empty()) {
        config_candidates.push_back(install_data_dir / "unrw_config.toml");
    }
    config_candidates.push_back(fs::current_path() / "unrw_config.toml");

    std::vector<fs::path> lut_candidates;
    lut_candidates.push_back(exe_dir / "LUTs");
    if (!macos_resources_dir.empty()) {
        lut_candidates.push_back(macos_resources_dir / "LUTs");
    }
    lut_candidates.push_back(prefix_share_dir / "LUTs");
    if (!install_data_dir.empty()) {
        lut_candidates.push_back(install_data_dir / "LUTs");
    }
    lut_candidates.push_back(fs::current_path() / "LUTs");

    g_app_paths.default_config_file     = firstExistingFile(config_candidates);
    g_app_paths.default_lut_dir         = firstExistingDirectory(lut_candidates);
    g_app_paths.user_config_file_string = pathToUtf8(g_app_paths.user_config_file);
    g_app_paths.imgui_ini_file_string   = pathToUtf8(g_app_paths.imgui_ini_file);
    g_app_paths_initialized             = true;
}

static bool
copyDefaultConfigIfMissing()
{
    if (pathIsRegularFile(g_app_paths.user_config_file)) {
        return true;
    }
    if (g_app_paths.default_config_file.empty()) {
        spdlog::warn("Default config file was not found; user config was not created");
        return false;
    }

    std::error_code ec;
    fs::copy_file(g_app_paths.default_config_file, g_app_paths.user_config_file, fs::copy_options::skip_existing, ec);
    if (ec) {
        spdlog::warn("Could not copy default config to {}: {}", pathToUtf8(g_app_paths.user_config_file), ec.message());
        return false;
    }
    spdlog::info("Created user config: {}", pathToUtf8(g_app_paths.user_config_file));
    return true;
}

static void
copyDefaultLutsIfMissing()
{
    if (pathIsDirectory(g_app_paths.user_lut_dir)) {
        return;
    }
    if (pathExists(g_app_paths.user_lut_dir)) {
        spdlog::warn("User LUT path exists but is not a directory: {}", pathToUtf8(g_app_paths.user_lut_dir));
        return;
    }

    std::error_code ec;
    fs::create_directories(g_app_paths.user_lut_dir, ec);
    if (ec) {
        spdlog::warn("Could not create user LUT folder {}: {}", pathToUtf8(g_app_paths.user_lut_dir), ec.message());
        return;
    }

    if (g_app_paths.default_lut_dir.empty()) {
        spdlog::warn("Default LUT folder was not found; created empty user LUT folder");
        return;
    }

    fs::copy(g_app_paths.default_lut_dir, g_app_paths.user_lut_dir,
             fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
    if (ec) {
        spdlog::warn("Could not copy default LUTs to {}: {}", pathToUtf8(g_app_paths.user_lut_dir), ec.message());
        return;
    }
    spdlog::info("Created user LUT folder: {}", pathToUtf8(g_app_paths.user_lut_dir));
}

const AppRuntimePaths&
appRuntimePaths()
{
    initializeAppRuntimePaths();
    return g_app_paths;
}

bool
prepareAppUserFiles()
{
    initializeAppRuntimePaths();
    if (g_app_user_files_prepared) {
        return pathIsRegularFile(g_app_paths.user_config_file);
    }

    std::error_code ec;
    fs::create_directories(g_app_paths.user_dir, ec);
    if (ec) {
        spdlog::warn("Could not create user config folder {}: {}", pathToUtf8(g_app_paths.user_dir), ec.message());
        g_app_user_files_prepared = true;
        return false;
    }

    const bool config_ready = copyDefaultConfigIfMissing();
    copyDefaultLutsIfMissing();
    fs::create_directories(g_app_paths.user_preset_dir, ec);
    if (ec) {
        spdlog::warn("Could not create user presets folder {}: {}", pathToUtf8(g_app_paths.user_preset_dir), ec.message());
        ec.clear();
    }
    fs::remove_all(g_app_paths.temp_preset_dir, ec);
    ec.clear();
    fs::create_directories(g_app_paths.temp_preset_dir, ec);
    if (ec) {
        spdlog::warn("Could not create temporary preset folder {}: {}", pathToUtf8(g_app_paths.temp_preset_dir),
                     ec.message());
    }

    g_app_user_files_prepared = true;
    return config_ready;
}

const char*
appImguiIniFilename()
{
    initializeAppRuntimePaths();
    return g_app_paths.imgui_ini_file_string.c_str();
}
