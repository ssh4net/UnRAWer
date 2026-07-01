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
#include "pathutils.h"

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

static std::string
settingsToken(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        if (c != ' ' && c != '-' && c != '_' && c != '/') {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return result;
}

template<typename Fn>
static void
get_codec_value(const toml::value& data, const std::string& section, const std::string& key, int& var, Fn parser)
{
    if (!data.contains(section) || !data.at(section).contains(key)) {
        return;
    }
    const toml::value& value = data.at(section).at(key);
    if (value.is_string()) {
        var = parser(value.as_string(), var);
    } else if (value.is_integer()) {
        var = static_cast<int>(value.as_integer());
    }
}

static int
tiffCompressionFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "zip" || token == "deflate" || token == "zipdeflate") return TiffCompression_Zip;
    if (token == "lzw") return TiffCompression_Lzw;
    if (token == "packbits") return TiffCompression_PackBits;
    if (token == "none") return TiffCompression_None;
    return fallback;
}

static int
exrCompressionFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "zip" || token == "deflate" || token == "zipdeflate") return ExrCompression_Zip;
    if (token == "zips") return ExrCompression_Zips;
    if (token == "piz") return ExrCompression_Piz;
    if (token == "pxr24") return ExrCompression_Pxr24;
    if (token == "rle") return ExrCompression_Rle;
    if (token == "b44") return ExrCompression_B44;
    if (token == "b44a") return ExrCompression_B44A;
    if (token == "dwaa") return ExrCompression_Dwaa;
    if (token == "dwab") return ExrCompression_Dwab;
    if (token == "htj2k256") return ExrCompression_Htj2k256;
    if (token == "htj2k32") return ExrCompression_Htj2k32;
    if (token == "none") return ExrCompression_None;
    return fallback;
}

static int
pngStrategyFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "default") return PngCompression_Default;
    if (token == "filtered") return PngCompression_Filtered;
    if (token == "huffman") return PngCompression_Huffman;
    if (token == "rle") return PngCompression_Rle;
    if (token == "fixed") return PngCompression_Fixed;
    if (token == "fast" || token == "pngfast") return PngCompression_Fast;
    if (token == "none") return PngCompression_None;
    return fallback;
}

static int
jpegSubsamplingFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "4:4:4" || token == "444") return JpegSubsampling_444;
    if (token == "4:2:2" || token == "422") return JpegSubsampling_422;
    if (token == "4:2:0" || token == "420") return JpegSubsampling_420;
    if (token == "4:1:1" || token == "411") return JpegSubsampling_411;
    return fallback;
}

// Specialization or overload for handling type mismatches if necessary,
// but toml11 usually handles conversions well for standard types.

bool
loadSettings(Settings& settings, const std::string& filename)
{
    try {
        namespace fs = std::filesystem;
        const fs::path config_path = pathFromUtf8(filename);
        std::error_code ec;
        fs::path config_dir = fs::absolute(config_path, ec).parent_path();
        if (ec || config_dir.empty()) {
            config_dir = fs::current_path();
        }

        std::ifstream config_stream(config_path, std::ios::binary);
        if (!config_stream) {
            spdlog::error("Could not open settings file: {}", filename);
            return false;
        }
        const auto data = toml::parse(config_stream, filename);

        settings.lut_Preset.clear();

        get_value(data, "Global", "Console", settings.conEnable);
        get_value(data, "Global", "Threads", settings.threads);
        get_value(data, "Global", "ThredsMult", settings.mltThreads);
        get_value(data, "Global", "ExportSubf", settings.useSbFldr);
        get_value(data, "Global", "PathPrefix", settings.pathPrefix);
        get_value(data, "Global", "Verbosity", settings.verbosity);

        get_value(data, "Range", "RangeMode", settings.rangeMode);

        get_value(data, "Preview", "Enable", settings.previewEnable);
        get_value(data, "Preview", "QueueMax", settings.previewQueueMax);
        get_value(data, "Preview", "MinTimeMs", settings.previewMinTimeMs);

        get_value(data, "Export", "DefaultFormat", settings.defFormat);
        get_value(data, "Export", "FileFormat", settings.fileFormat);
        get_value(data, "Export", "DefaultBit", settings.defBDepth);
        get_value(data, "Export", "BitDepth", settings.bitDepth);
        get_value(data, "Export", "Quality", settings.quality);
        settings.jpegQuality  = settings.quality;
        settings.heicQuality  = settings.quality;
        settings.jpegxlQuality = settings.quality;

        get_codec_value(data, "Encoding", "TiffCompression", settings.tiffCompression, tiffCompressionFromString);
        get_value(data, "Encoding", "TiffZipLevel", settings.tiffZipLevel);
        get_codec_value(data, "Encoding", "OpenEXRCompression", settings.exrCompression, exrCompressionFromString);
        get_value(data, "Encoding", "OpenEXRZipLevel", settings.exrZipLevel);
        get_value(data, "Encoding", "OpenEXRDwaLevel", settings.exrDwaLevel);
        get_codec_value(data, "Encoding", "PngStrategy", settings.pngStrategy, pngStrategyFromString);
        get_value(data, "Encoding", "PngLevel", settings.pngCompressionLevel);
        get_value(data, "Encoding", "JpegQuality", settings.jpegQuality);
        get_codec_value(data, "Encoding", "JpegSubsampling", settings.jpegSubsampling, jpegSubsamplingFromString);
        get_value(data, "Encoding", "Jpeg2000QStep", settings.jpeg2000QStep);
        get_value(data, "Encoding", "HeicQuality", settings.heicQuality);
        get_value(data, "Encoding", "JpegXLQuality", settings.jpegxlQuality);
        get_value(data, "Encoding", "JpegXLEffort", settings.jpegxlEffort);
        get_value(data, "Encoding", "JpegXLSpeed", settings.jpegxlSpeed);

        settings.defFormat           = std::clamp(settings.defFormat, 0, 8);
        settings.fileFormat          = std::clamp(settings.fileFormat, -1, 8);
        settings.defBDepth           = std::clamp(settings.defBDepth, 0, 6);
        settings.bitDepth            = std::clamp(settings.bitDepth, -1, 6);
        settings.tiffCompression     = std::clamp(settings.tiffCompression, static_cast<int>(TiffCompression_Zip),
                                                  static_cast<int>(TiffCompression_None));
        settings.tiffZipLevel        = std::clamp(settings.tiffZipLevel, 1, 9);
        settings.exrCompression      = std::clamp(settings.exrCompression, static_cast<int>(ExrCompression_Zip),
                                                  static_cast<int>(ExrCompression_None));
        settings.exrZipLevel         = std::clamp(settings.exrZipLevel, 1, 9);
        settings.exrDwaLevel         = std::clamp(settings.exrDwaLevel, 1, 100);
        settings.pngStrategy         = std::clamp(settings.pngStrategy, static_cast<int>(PngCompression_Default),
                                                  static_cast<int>(PngCompression_None));
        settings.pngCompressionLevel = std::clamp(settings.pngCompressionLevel, 0, 9);
        settings.jpegQuality         = std::clamp(settings.jpegQuality, 1, 100);
        settings.jpegSubsampling     = std::clamp(settings.jpegSubsampling, static_cast<int>(JpegSubsampling_444),
                                                  static_cast<int>(JpegSubsampling_411));
        settings.jpeg2000QStep       = std::clamp(settings.jpeg2000QStep, -1.0f, 10.0f);
        settings.heicQuality         = std::clamp(settings.heicQuality, 1, 100);
        settings.jpegxlQuality       = std::clamp(settings.jpegxlQuality, 1, 100);
        settings.jpegxlEffort        = std::clamp(settings.jpegxlEffort, 1, 9);
        settings.jpegxlSpeed         = std::clamp(settings.jpegxlSpeed, 0, 4);

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

        get_value(data, "Transform", "LutFolder", settings.lutFolder);
        get_value(data, "Transform", "LutTransform", settings.lutMode);
        get_value(data, "Transform", "LutDefault", settings.dLutPreset);
        get_value(data, "Transform", "exif_lut", settings.perCamera);

        get_value(data, "Unsharp", "sharp_mode", settings.sharp_mode);
        get_value(data, "Unsharp", "sharp_kernel", settings.sharp_kernel);
        get_value(data, "Unsharp", "sharp_width", settings.sharp_width);
        get_value(data, "Unsharp", "sharp_contrast", settings.sharp_contrast);
        get_value(data, "Unsharp", "sharp_treshold", settings.sharp_tresh);

        fs::path lutPath = pathFromUtf8(settings.lutFolder);
        if (!lutPath.empty() && lutPath.is_relative()) {
            lutPath = config_dir / lutPath;
        }
        if (fs::exists(lutPath) && fs::is_directory(lutPath)) {
            lutPath            = fs::absolute(lutPath);
            settings.lutFolder = pathToUtf8(lutPath);
            for (const auto& entry : fs::directory_iterator(lutPath)) {
                if (entry.is_regular_file()) {
                    settings.lut_Preset[pathToUtf8(entry.path().stem())] = pathToUtf8(entry.path());
                }
            }
        }

        settings.previewQueueMax  = std::clamp(settings.previewQueueMax, 0, 10000);
        settings.previewMinTimeMs = std::clamp(settings.previewMinTimeMs, 0, 600000);

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
    spdlog::info("Preview Enable: {}", settings.previewEnable);
    spdlog::info("Preview QueueMax: {}", settings.previewQueueMax);
    spdlog::info("Preview MinTimeMs: {}", settings.previewMinTimeMs);
    spdlog::info("Range Mode: {}", settings.rangeMode);
    spdlog::info("Export Format: {}", settings.fileFormat);
    spdlog::info("Bit Depth: {}", settings.bitDepth);
    spdlog::info("Quality: {}", settings.quality);
    spdlog::info("TIFF Compression: {} level {}", settings.tiffCompression, settings.tiffZipLevel);
    spdlog::info("OpenEXR Compression: {} zip level {} dwa level {}", settings.exrCompression, settings.exrZipLevel,
                 settings.exrDwaLevel);
    spdlog::info("PNG Compression: {} level {}", settings.pngStrategy, settings.pngCompressionLevel);
    spdlog::info("JPEG Quality: {} subsampling {}", settings.jpegQuality, settings.jpegSubsampling);
    spdlog::info("HTJ2K QStep: {}", settings.jpeg2000QStep);
    spdlog::info("HEIC Quality: {}", settings.heicQuality);
    spdlog::info("JPEG XL Quality: {} effort {} speed {}", settings.jpegxlQuality, settings.jpegxlEffort,
                 settings.jpegxlSpeed);

    spdlog::info("Raw Rotation: {}", settings.rawRot);
    spdlog::info("Raw Color Space: {}", settings.rawSpace);
    spdlog::info("Demosaic: {}", settings.dDemosaic);

    spdlog::info("Auto WB: {}", settings.rawParms.use_auto_wb);
    spdlog::info("Camera WB: {}", settings.rawParms.use_camera_wb);

    spdlog::info("LUT Mode: {}", settings.lutMode);
    spdlog::info("Sharp Mode: {}", settings.sharp_mode);
    spdlog::info("------------------------");
}
