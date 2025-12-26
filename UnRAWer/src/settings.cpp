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

#include <toml.hpp>

Settings settings;

// Helper to safely get values from toml11
template<typename T>
void
get_value(const toml::value& v, const std::string& section, const std::string& key, T& var)
{
    if (v.contains(section) && v.at(section).contains(key)) {
        var = toml::find<T>(v, section, key);
    }
}

// Specialization or overload for handling type mismatches if necessary,
// but toml11 usually handles conversions well for standard types.

bool
loadSettings(Settings& settings, const std::string& filename)
{
    try {
        const auto data = toml::parse(filename);

        get_value(data, "Global", "Console", settings.conEnable);
        get_value(data, "Global", "Threads", settings.threads);
        get_value(data, "Global", "ThredsMult", settings.mltThreads);
        get_value(data, "Global", "ExportSubf", settings.useSbFldr);
        get_value(data, "Global", "PathPrefix", settings.pathPrefix);
        get_value(data, "Global", "Verbosity", settings.verbosity);

        get_value(data, "Range", "RangeMode", settings.rangeMode);

        get_value(data, "Export", "DefaultFormat", settings.defFormat);
        get_value(data, "Export", "FileFormat", settings.fileFormat);
        get_value(data, "Export", "DefaultBit", settings.defBDepth);
        get_value(data, "Export", "BitDepth", settings.bitDepth);
        get_value(data, "Export", "Quality", settings.quality);

        get_value(data, "CameraRaw", "RawRotation", settings.rawRot);
        get_value(data, "CameraRaw", "RawColorSpace", settings.rawSpace);
        get_value(data, "CameraRaw", "Demosaic", settings.dDemosaic);
        get_value(data, "CameraRaw", "half_size", settings.rawParms.half_size);
        get_value(data, "CameraRaw", "use_auto_wb", settings.rawParms.use_auto_wb);
        get_value(data, "CameraRaw", "use_camera_wb", settings.rawParms.use_camera_wb);
        get_value(data, "CameraRaw", "use_camera_matrix", settings.rawParms.use_camera_matrix);
        get_value(data, "CameraRaw", "highlights", settings.rawParms.highlight);

        // Aberrations is an array [1.0, 1.0]
        if (data.contains("CameraRaw") && data.at("CameraRaw").contains("aberrations")) {
            auto aber = toml::find<std::vector<float>>(data, "CameraRaw", "aberrations");
            if (aber.size() >= 2) {
                settings.rawParms.aber[0] = aber[0];
                settings.rawParms.aber[1] = aber[1];
            }
        }

        get_value(data, "CameraRaw", "denoise_mode", settings.denoise_mode);
        get_value(data, "CameraRaw", "dnz_threshold", settings.rawParms.denoise_thr);
        get_value(data, "CameraRaw", "fbdd_noiserd", settings.rawParms.fbdd_noiserd);
        get_value(data, "CameraRaw", "exif_crop", settings.crop_mode);

        get_value(data, "OCIO", "OCIO_Config", settings.ocioConfigPath);

        get_value(data, "Transform", "LutFolder", settings.lutFolder);
        get_value(data, "Transform", "LutTransform", settings.lutMode);
        get_value(data, "Transform", "LutDefault", settings.dLutPreset);
        get_value(data, "Transform", "exif_lut", settings.perCamera);

        get_value(data, "Unsharp", "sharp_mode", settings.sharp_mode);
        get_value(data, "Unsharp", "sharp_kernel", settings.sharp_kernel);
        get_value(data, "Unsharp", "sharp_width", settings.sharp_width);
        get_value(data, "Unsharp", "sharp_contrast", settings.sharp_contrast);
        get_value(data, "Unsharp", "sharp_treshold", settings.sharp_tresh);

        // Scan LUT folder
        namespace fs = std::filesystem;
        fs::path lutPath(settings.lutFolder);
        if (fs::exists(lutPath) && fs::is_directory(lutPath)) {
            lutPath            = fs::absolute(lutPath);
            settings.lutFolder = lutPath.string();
            for (const auto& entry : fs::directory_iterator(lutPath)) {
                if (entry.is_regular_file()) {
                    settings.lut_Preset[entry.path().stem().string()] = entry.path().string();
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error loading settings: {}", e.what());
        return false;
    }
}

void
printSettings(Settings& settings)
{
    spdlog::info("--- Current Settings ---");
    spdlog::info("Console: {}", settings.conEnable);
    spdlog::info("Threads: {}", settings.threads);
    spdlog::info("Verbosity: {}", settings.verbosity);
    spdlog::info("Range Mode: {}", settings.rangeMode);
    spdlog::info("Export Format: {}", settings.fileFormat);
    spdlog::info("Bit Depth: {}", settings.bitDepth);
    spdlog::info("Quality: {}", settings.quality);

    spdlog::info("Raw Rotation: {}", settings.rawRot);
    spdlog::info("Raw Color Space: {}", settings.rawSpace);
    spdlog::info("Demosaic: {}", settings.dDemosaic);

    spdlog::info("Auto WB: {}", settings.rawParms.use_auto_wb);
    spdlog::info("Camera WB: {}", settings.rawParms.use_camera_wb);

    spdlog::info("OCIO Config: {}", settings.ocioConfigPath);
    spdlog::info("LUT Mode: {}", settings.lutMode);
    spdlog::info("Sharp Mode: {}", settings.sharp_mode);
    spdlog::info("------------------------");
}