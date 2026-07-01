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
#pragma once

#ifndef APP_PATHS_H
#    define APP_PATHS_H

#    include <filesystem>
#    include <string>

struct AppRuntimePaths {
    std::filesystem::path user_dir;
    std::filesystem::path user_config_file;
    std::filesystem::path user_lut_dir;
    std::filesystem::path user_preset_dir;
    std::filesystem::path temp_preset_dir;
    std::filesystem::path imgui_ini_file;
    std::filesystem::path default_config_file;
    std::filesystem::path default_lut_dir;

    std::string user_config_file_string;
    std::string imgui_ini_file_string;
};

const AppRuntimePaths&
appRuntimePaths();
bool
prepareAppUserFiles();
const char*
appImguiIniFilename();

#endif
