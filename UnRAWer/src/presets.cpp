#include "pch.h"
#include "presets.h"
#include "app_paths.h"
#include "pathutils.h"

#include <cstdio>
#include <type_traits>
#include <toml.hpp>

#ifdef UNRAWER_WITH_MINIZIP
extern "C" {
#    include <minizip-ng/mz.h>
#    include <minizip-ng/mz_strm.h>
#    include <minizip-ng/mz_zip.h>
#    include <minizip-ng/mz_zip_rw.h>
}
#endif

namespace fs = std::filesystem;

static constexpr int64_t MaxPresetTomlBytes = 1024 * 1024;
static constexpr int64_t MaxPresetLutBytes  = 512LL * 1024LL * 1024LL;

template<typename T>
static void
getPresetValue(const toml::value& v, const std::string& section, const std::string& key, T& var)
{
    if (v.contains(section) && v.at(section).contains(key)) {
        const toml::value& value = v.at(section).at(key);
        if constexpr (std::is_floating_point_v<T>) {
            if (value.is_floating()) {
                var = static_cast<T>(value.as_floating());
            } else if (value.is_integer()) {
                var = static_cast<T>(value.as_integer());
            } else {
                var = toml::find<T>(v, section, key);
            }
        } else {
            var = toml::find<T>(v, section, key);
        }
    }
}

static std::string
sanitizePresetName(const std::string& name)
{
    std::string clean;
    clean.reserve(name.size());
    for (const char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_' || c == '-' || c == ' ') {
            clean.push_back(c);
        } else {
            clean.push_back('_');
        }
    }
    while (!clean.empty() && clean.front() == ' ') {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && clean.back() == ' ') {
        clean.pop_back();
    }
    if (clean.empty()) {
        clean = "Preset";
    }
    return clean;
}

static std::string
tomlString(const std::string& value)
{
    std::string out = "\"";
    for (const char c : value) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::string
tomlFloat(float value)
{
    std::ostringstream out;
    out << std::setprecision(9) << value;
    std::string text = out.str();
    if (text.find_first_of(".eE") == std::string::npos) {
        text += ".0";
    }
    return text;
}

static std::string
lowerExtension(const fs::path& path)
{
    std::string ext = pathToUtf8(path.extension());
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

static bool
isSupportedLutFile(const fs::path& path)
{
    const std::string ext = lowerExtension(path);
    return ext == ".cube" || ext == ".clf";
}

static bool
isSafeArchivePath(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    if (path.front() == '/' || path.find('\\') != std::string::npos || path.find(':') != std::string::npos) {
        return false;
    }

    fs::path fs_path = pathFromUtf8(path);
    for (const fs::path& part : fs_path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

static std::string
presetTomlFromSettings(const Settings& settings, const std::string& presetName, bool portablePackage,
                       const std::string& portableLutName, const std::string& portableLutPath)
{
    std::ostringstream out;
    out << "[Preset]\n";
    out << "Name = " << tomlString(presetName) << "\n";
    out << "Version = 1\n";
    out << "Package = " << (portablePackage ? "true" : "false") << "\n\n";

    out << "[Range]\n";
    out << "RangeMode = " << settings.rangeMode << "\n\n";

    out << "[Export]\n";
    out << "DefaultFormat = " << settings.defFormat << "\n";
    out << "FileFormat = " << settings.fileFormat << "\n";
    out << "DefaultBit = " << settings.defBDepth << "\n";
    out << "BitDepth = " << settings.bitDepth << "\n";
    out << "Quality = " << settings.quality << "\n\n";

    out << "[Encoding]\n";
    out << "TiffCompression = " << settings.tiffCompression << "\n";
    out << "TiffZipLevel = " << settings.tiffZipLevel << "\n";
    out << "OpenEXRCompression = " << settings.exrCompression << "\n";
    out << "OpenEXRZipLevel = " << settings.exrZipLevel << "\n";
    out << "OpenEXRDwaLevel = " << settings.exrDwaLevel << "\n";
    out << "PngStrategy = " << settings.pngStrategy << "\n";
    out << "PngLevel = " << settings.pngCompressionLevel << "\n";
    out << "JpegQuality = " << settings.jpegQuality << "\n";
    out << "JpegSubsampling = " << settings.jpegSubsampling << "\n";
    out << "Jpeg2000QStep = " << tomlFloat(settings.jpeg2000QStep) << "\n";
    out << "HeicQuality = " << settings.heicQuality << "\n";
    out << "JpegXLQuality = " << settings.jpegxlQuality << "\n";
    out << "JpegXLEffort = " << settings.jpegxlEffort << "\n";
    out << "JpegXLSpeed = " << settings.jpegxlSpeed << "\n\n";

    out << "[CameraRaw]\n";
    out << "RawRotation = " << settings.rawRot << "\n";
    out << "RawColorSpace = " << settings.rawSpace << "\n";
    out << "Demosaic = " << settings.dDemosaic << "\n";
    out << "half_size = " << (settings.rawParms.half_size ? "true" : "false") << "\n";
    out << "use_auto_wb = " << (settings.rawParms.use_auto_wb ? "true" : "false") << "\n";
    out << "use_camera_wb = " << (settings.rawParms.use_camera_wb ? "true" : "false") << "\n";
    out << "use_camera_matrix = " << settings.rawParms.use_camera_matrix << "\n";
    out << "highlights = " << settings.rawParms.highlight << "\n";
    out << "aberrations = [" << tomlFloat(settings.rawParms.aber[0]) << ", " << tomlFloat(settings.rawParms.aber[1])
        << "]\n";
    out << "denoise_mode = " << settings.denoise_mode << "\n";
    out << "dnz_threshold = " << tomlFloat(settings.rawParms.denoise_thr) << "\n";
    out << "fbdd_noiserd = " << settings.rawParms.fbdd_noiserd << "\n";
    out << "exif_crop = " << settings.crop_mode << "\n\n";

    out << "[Transform]\n";
    out << "LutTransform = " << settings.lutMode << "\n";
    out << "LutDefault = " << tomlString(settings.dLutPreset) << "\n";
    out << "exif_lut = " << (settings.perCamera ? "true" : "false") << "\n\n";

    out << "[Unsharp]\n";
    out << "sharp_mode = " << settings.sharp_mode << "\n";
    out << "sharp_kernel = " << settings.sharp_kernel << "\n";
    out << "sharp_width = " << tomlFloat(settings.sharp_width) << "\n";
    out << "sharp_contrast = " << tomlFloat(settings.sharp_contrast) << "\n";
    out << "sharp_treshold = " << tomlFloat(settings.sharp_tresh) << "\n";

    if (portablePackage && !portableLutName.empty() && !portableLutPath.empty()) {
        out << "\n[[LUT]]\n";
        out << "Name = " << tomlString(portableLutName) << "\n";
        out << "File = " << tomlString(portableLutPath) << "\n";
    }

    return out.str();
}

static void
applyPresetToml(Settings& settings, const toml::value& data)
{
    getPresetValue(data, "Range", "RangeMode", settings.rangeMode);

    getPresetValue(data, "Export", "DefaultFormat", settings.defFormat);
    getPresetValue(data, "Export", "FileFormat", settings.fileFormat);
    getPresetValue(data, "Export", "DefaultBit", settings.defBDepth);
    getPresetValue(data, "Export", "BitDepth", settings.bitDepth);
    getPresetValue(data, "Export", "Quality", settings.quality);
    settings.jpegQuality  = settings.quality;
    settings.heicQuality  = settings.quality;
    settings.jpegxlQuality = settings.quality;

    getPresetValue(data, "Encoding", "TiffCompression", settings.tiffCompression);
    getPresetValue(data, "Encoding", "TiffZipLevel", settings.tiffZipLevel);
    getPresetValue(data, "Encoding", "OpenEXRCompression", settings.exrCompression);
    getPresetValue(data, "Encoding", "OpenEXRZipLevel", settings.exrZipLevel);
    getPresetValue(data, "Encoding", "OpenEXRDwaLevel", settings.exrDwaLevel);
    getPresetValue(data, "Encoding", "PngStrategy", settings.pngStrategy);
    getPresetValue(data, "Encoding", "PngLevel", settings.pngCompressionLevel);
    getPresetValue(data, "Encoding", "JpegQuality", settings.jpegQuality);
    getPresetValue(data, "Encoding", "JpegSubsampling", settings.jpegSubsampling);
    getPresetValue(data, "Encoding", "Jpeg2000QStep", settings.jpeg2000QStep);
    getPresetValue(data, "Encoding", "HeicQuality", settings.heicQuality);
    getPresetValue(data, "Encoding", "JpegXLQuality", settings.jpegxlQuality);
    getPresetValue(data, "Encoding", "JpegXLEffort", settings.jpegxlEffort);
    getPresetValue(data, "Encoding", "JpegXLSpeed", settings.jpegxlSpeed);

    getPresetValue(data, "CameraRaw", "RawRotation", settings.rawRot);
    getPresetValue(data, "CameraRaw", "RawColorSpace", settings.rawSpace);
    getPresetValue(data, "CameraRaw", "Demosaic", settings.dDemosaic);
    getPresetValue(data, "CameraRaw", "half_size", settings.rawParms.half_size);
    getPresetValue(data, "CameraRaw", "use_auto_wb", settings.rawParms.use_auto_wb);
    getPresetValue(data, "CameraRaw", "use_camera_wb", settings.rawParms.use_camera_wb);
    getPresetValue(data, "CameraRaw", "use_camera_matrix", settings.rawParms.use_camera_matrix);
    getPresetValue(data, "CameraRaw", "highlights", settings.rawParms.highlight);
    if (data.contains("CameraRaw") && data.at("CameraRaw").contains("aberrations")) {
        const toml::value& aberrations = data.at("CameraRaw").at("aberrations");
        if (aberrations.is_array()) {
            const auto& aber = aberrations.as_array();
            const size_t count = std::min<size_t>(aber.size(), 2);
            for (size_t i = 0; i < count; ++i) {
                if (aber[i].is_floating()) {
                    settings.rawParms.aber[i] = static_cast<float>(aber[i].as_floating());
                } else if (aber[i].is_integer()) {
                    settings.rawParms.aber[i] = static_cast<float>(aber[i].as_integer());
                }
            }
        }
    }
    getPresetValue(data, "CameraRaw", "denoise_mode", settings.denoise_mode);
    getPresetValue(data, "CameraRaw", "dnz_threshold", settings.rawParms.denoise_thr);
    getPresetValue(data, "CameraRaw", "fbdd_noiserd", settings.rawParms.fbdd_noiserd);
    getPresetValue(data, "CameraRaw", "exif_crop", settings.crop_mode);

    getPresetValue(data, "Transform", "LutTransform", settings.lutMode);
    getPresetValue(data, "Transform", "LutDefault", settings.dLutPreset);
    getPresetValue(data, "Transform", "exif_lut", settings.perCamera);

    getPresetValue(data, "Unsharp", "sharp_mode", settings.sharp_mode);
    getPresetValue(data, "Unsharp", "sharp_kernel", settings.sharp_kernel);
    getPresetValue(data, "Unsharp", "sharp_width", settings.sharp_width);
    getPresetValue(data, "Unsharp", "sharp_contrast", settings.sharp_contrast);
    getPresetValue(data, "Unsharp", "sharp_treshold", settings.sharp_tresh);
}

#ifdef UNRAWER_WITH_MINIZIP
static bool
readZipEntryToString(const fs::path& archive, const char* entryName, std::string& out)
{
    void* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        return false;
    }

    const std::string archive_path = pathToUtf8(archive);
    bool ok                       = false;
    if (mz_zip_reader_open_file(reader, archive_path.c_str()) == MZ_OK
        && mz_zip_reader_locate_entry(reader, entryName, 0) == MZ_OK && mz_zip_reader_entry_open(reader) == MZ_OK) {
        const int32_t length = mz_zip_reader_entry_save_buffer_length(reader);
        if (length > 0 && length <= MaxPresetTomlBytes) {
            std::vector<char> buffer(static_cast<size_t>(length));
            if (mz_zip_reader_entry_save_buffer(reader, buffer.data(), length) == MZ_OK) {
                out.assign(buffer.begin(), buffer.end());
                ok = true;
            }
        }
        (void)mz_zip_reader_entry_close(reader);
    }

    (void)mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return ok;
}

static bool
extractZipEntryToFile(const fs::path& archive, const std::string& entryName, const fs::path& output)
{
    if (!isSafeArchivePath(entryName)) {
        return false;
    }

    void* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        return false;
    }

    const std::string archive_path = pathToUtf8(archive);
    bool ok                       = false;
    if (mz_zip_reader_open_file(reader, archive_path.c_str()) == MZ_OK
        && mz_zip_reader_locate_entry(reader, entryName.c_str(), 0) == MZ_OK
        && mz_zip_reader_entry_open(reader) == MZ_OK) {
        mz_zip_file* file_info = nullptr;
        if (mz_zip_reader_entry_get_info(reader, &file_info) == MZ_OK && file_info != nullptr
            && file_info->uncompressed_size >= 0 && file_info->uncompressed_size <= MaxPresetLutBytes) {
            std::error_code ec;
            fs::create_directories(output.parent_path(), ec);
            if (!ec) {
                const std::string output_path = pathToUtf8(output);
                ok = mz_zip_reader_entry_save_file(reader, output_path.c_str()) == MZ_OK;
            }
        }
        (void)mz_zip_reader_entry_close(reader);
    }

    (void)mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return ok;
}

static bool
writeZipBuffer(void* writer, const char* entryName, const std::string& text)
{
    mz_zip_file file_info = {};
    file_info.filename   = entryName;
    return mz_zip_writer_add_buffer(writer, text.data(), static_cast<int32_t>(text.size()), &file_info) == MZ_OK;
}

static bool
loadPortablePreset(const PresetEntry& preset, Settings& settings)
{
    std::string preset_toml;
    if (!readZipEntryToString(preset.path, "preset.toml", preset_toml)) {
        spdlog::error("Preset archive does not contain readable preset.toml: {}", pathToUtf8(preset.path));
        return false;
    }

    std::istringstream toml_stream(preset_toml);
    const toml::value data = toml::parse(toml_stream, "preset.toml");

    const fs::path preset_cache_dir = appRuntimePaths().temp_preset_dir / sanitizePresetName(preset.name);
    std::error_code ec;
    fs::remove_all(preset_cache_dir, ec);
    ec.clear();
    fs::create_directories(preset_cache_dir, ec);
    if (ec) {
        spdlog::error("Could not create preset cache folder {}: {}", pathToUtf8(preset_cache_dir), ec.message());
        return false;
    }

    if (data.contains("LUT") && data.at("LUT").is_array()) {
        const auto& luts = data.at("LUT").as_array();
        for (const toml::value& lut : luts) {
            if (!lut.is_table() || !lut.contains("Name") || !lut.contains("File")) {
                continue;
            }
            const std::string lut_name = toml::find<std::string>(lut, "Name");
            const std::string lut_file = toml::find<std::string>(lut, "File");
            if (!isSafeArchivePath(lut_file) || !isSupportedLutFile(lut_file)) {
                spdlog::warn("Skipping unsafe or unsupported preset LUT path: {}", lut_file);
                continue;
            }
            const fs::path extracted_lut = preset_cache_dir / pathFromUtf8(lut_file);
            if (extractZipEntryToFile(preset.path, lut_file, extracted_lut)) {
                settings.lut_Preset[lut_name] = pathToUtf8(extracted_lut);
            } else {
                spdlog::warn("Could not extract preset LUT {} from {}", lut_file, pathToUtf8(preset.path));
            }
        }
    }

    applyPresetToml(settings, data);
    return true;
}
#endif

std::vector<PresetEntry>
ListPresets()
{
    std::vector<PresetEntry> presets;
    const fs::path preset_dir = appRuntimePaths().user_preset_dir;
    std::error_code ec;
    if (!fs::is_directory(preset_dir, ec)) {
        return presets;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(preset_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        const std::string ext = lowerExtension(path);
        if (ext == ".toml") {
            presets.push_back({ pathToUtf8(path.stem()), PresetStorage::LocalToml, path });
        } else if (ext == ".unrwpreset") {
            presets.push_back({ pathToUtf8(path.stem()), PresetStorage::PortableZip, path });
        }
    }

    std::sort(presets.begin(), presets.end(), [](const PresetEntry& a, const PresetEntry& b) {
        return a.name < b.name;
    });
    return presets;
}

bool
LoadPreset(const PresetEntry& preset, Settings& settings)
{
    try {
        if (preset.storage == PresetStorage::LocalToml) {
            std::ifstream preset_stream(preset.path, std::ios::binary);
            if (!preset_stream) {
                spdlog::error("Could not open preset {}", pathToUtf8(preset.path));
                return false;
            }
            const toml::value data = toml::parse(preset_stream, pathToUtf8(preset.path));
            applyPresetToml(settings, data);
            return true;
        }

#ifdef UNRAWER_WITH_MINIZIP
        return loadPortablePreset(preset, settings);
#else
        spdlog::error("Portable presets require minizip-ng support");
        return false;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Could not load preset {}: {}", pathToUtf8(preset.path), e.what());
        return false;
    }
}

bool
LoadPresetFile(const fs::path& presetPath, Settings& settings)
{
    const std::string ext = lowerExtension(presetPath);
    if (ext == ".toml") {
        return LoadPreset({ pathToUtf8(presetPath.stem()), PresetStorage::LocalToml, presetPath }, settings);
    }
    if (ext == ".unrwpreset") {
        return LoadPreset({ pathToUtf8(presetPath.stem()), PresetStorage::PortableZip, presetPath }, settings);
    }

    spdlog::error("Unsupported preset file extension: {}", pathToUtf8(presetPath));
    return false;
}

bool
SaveLocalPreset(const Settings& settings, const std::string& presetName)
{
    try {
        const std::string clean_name = sanitizePresetName(presetName);
        const fs::path output_path   = appRuntimePaths().user_preset_dir / (clean_name + ".toml");
        std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            spdlog::error("Could not create preset file: {}", pathToUtf8(output_path));
            return false;
        }
        out << presetTomlFromSettings(settings, clean_name, false, "", "");
        spdlog::info("Saved local preset: {}", pathToUtf8(output_path));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Could not save local preset: {}", e.what());
        return false;
    }
}

bool
ExportPortablePresetToFile(const Settings& settings, const std::string& presetName, const fs::path& outputPath)
{
#ifndef UNRAWER_WITH_MINIZIP
    spdlog::error("Portable preset export requires minizip-ng support");
    return false;
#else
    try {
        const std::string clean_name = sanitizePresetName(presetName);
        fs::path output_path         = outputPath;
        if (lowerExtension(output_path) != ".unrwpreset") {
            output_path += ".unrwpreset";
        }

        std::string lut_archive_name;
        std::string lut_preset_name;
        fs::path lut_file;
        if (settings.lutMode != -1 && !settings.dLutPreset.empty()) {
            const auto lut_it = settings.lut_Preset.find(settings.dLutPreset);
            if (lut_it != settings.lut_Preset.end()) {
                lut_file = pathFromUtf8(lut_it->second);
                if (fs::is_regular_file(lut_file) && isSupportedLutFile(lut_file)) {
                    lut_preset_name  = settings.dLutPreset;
                    lut_archive_name = "LUTs/" + pathToUtf8(lut_file.filename());
                } else {
                    spdlog::warn("Selected LUT is not a supported file and will not be embedded: {}", pathToUtf8(lut_file));
                }
            }
        }

        const std::string preset_toml
            = presetTomlFromSettings(settings, clean_name, true, lut_preset_name, lut_archive_name);

        void* writer = mz_zip_writer_create();
        if (writer == nullptr) {
            return false;
        }

        bool ok = false;
        const std::string output_string = pathToUtf8(output_path);
        if (mz_zip_writer_open_file(writer, output_string.c_str(), 0, 0) == MZ_OK
            && writeZipBuffer(writer, "preset.toml", preset_toml)) {
            ok = true;
            if (!lut_archive_name.empty()) {
                const std::string lut_path = pathToUtf8(lut_file);
                ok = mz_zip_writer_add_file(writer, lut_path.c_str(), lut_archive_name.c_str()) == MZ_OK;
            }
        }
        (void)mz_zip_writer_close(writer);
        mz_zip_writer_delete(&writer);

        if (!ok) {
            spdlog::error("Could not export portable preset: {}", pathToUtf8(output_path));
            return false;
        }
        spdlog::info("Exported portable preset: {}", pathToUtf8(output_path));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Could not export portable preset: {}", e.what());
        return false;
    }
#endif
}

bool
ExportPortablePreset(const Settings& settings, const std::string& presetName)
{
    const std::string clean_name = sanitizePresetName(presetName);
    return ExportPortablePresetToFile(settings, clean_name,
                                      appRuntimePaths().user_preset_dir / (clean_name + ".unrwpreset"));
}

bool
OpenPresetsFolder()
{
    const fs::path preset_dir = appRuntimePaths().user_preset_dir;
#ifdef _WIN32
    const std::wstring wide_path = preset_dir.wstring();
    return reinterpret_cast<intptr_t>(
               ShellExecuteW(nullptr, L"open", wide_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL))
           > 32;
#elif defined(__APPLE__)
    const std::string command = "open \"" + pathToUtf8(preset_dir) + "\"";
    return std::system(command.c_str()) == 0;
#else
    const std::string command = "xdg-open \"" + pathToUtf8(preset_dir) + "\"";
    return std::system(command.c_str()) == 0;
#endif
}
