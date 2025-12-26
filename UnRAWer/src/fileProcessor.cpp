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

#include "fileProcessor.h"
#include "settings.h"
#include <filesystem>


namespace fs = std::filesystem;

std::string
toLower(const std::string& str)
{
    std::string strCopy = str;
    std::transform(strCopy.begin(), strCopy.end(), strCopy.begin(), [](unsigned char c) { return std::tolower(c); });
    return strCopy;
}

void
getWritableExt(std::string* ext, Settings* settings)
{
    std::unique_ptr<ImageOutput> probe;
    std::string fn = "probename" + *ext;
    probe          = ImageOutput::create(fn);
    if (probe) {
        spdlog::info("{} is writable", *ext);
    } else {
        spdlog::info("{} is readonly", *ext);
        spdlog::info("Output format changed to {}", settings->out_formats[settings->defFormat]);
        *ext = "." + settings->out_formats[settings->defFormat];
    }
    probe.reset();
}

std::string
getExtension(std::string& extension, Settings* settings)
{
    extension = toLower(extension);
    switch (settings->fileFormat) {
        //-1 - original, 0 - TIFF, 1 - OpenEXR, 2 - PNG, 3 - JPEG, 4 - JPEG-2000, 5 - JPEG-XL, 6 - HEIC, 7 - PPM
    case 0: return ".tif";
    case 1: return ".exr";
    case 2: return ".png";
    case 3: return ".jpg";
    case 4: return ".jp2";
    case 5: return ".jxl";
    case 6: return ".heic";
    case 7: return ".ppm";
    }
    extension = "." + settings->out_formats[settings->defFormat];
    return extension;
}

std::tuple<std::string, std::string, std::string, std::string>
splitPath(const std::string& fileName)
{  // returns path, parent folder, base name, extension
    fs::path p(fileName);
    std::string path         = p.parent_path().string();
    std::string parentFolder = p.parent_path().filename().string();
    std::string baseName     = p.stem().string();
    std::string extension    = p.extension().string();
    return { path, parentFolder, baseName, extension };
}

std::optional<std::string>
getPresetfromName(const std::string& fileName, Settings* settings)
{
    fs::path p(fileName);
    std::string baseName = p.stem().string();
    std::string path     = p.parent_path().string();

    std::string baseNameLower = toLower(baseName);
    std::string pathLower     = toLower(path);

    // find if path or baseName contains any of settings.lut_Preset strings
    for (auto& lut_preset : settings->lut_Preset) {
        std::string lut_preset_key = toLower(lut_preset.first);
        if (pathLower.find(lut_preset_key) != std::string::npos
            || baseNameLower.find(lut_preset_key) != std::string::npos) {
            return lut_preset.first;
        }
    }
    return std::nullopt;
}

FileStepInfo
countExpectedSteps(const Settings& settings, const std::optional<std::string>& lut_preset)
{
    FileStepInfo info;
    info.stepCount = 0;

    // Step 1: Load (rawReader)
    info.stepCount++;

    // Step 2: Unpack (LUnpacker)
    info.stepCount++;

    // Step 3: Demosaic (if enabled)
    info.hasDemosaic = (settings.dDemosaic > -1);
    if (info.hasDemosaic) {
        info.stepCount++;
    }

    // Step 4: LUT (Smart mode check!)
    info.hasLUT = false;
    if (settings.lutMode == 1) {
        // Forced mode - always apply
        info.hasLUT = true;
    } else if (settings.lutMode == 0 && lut_preset.has_value()) {
        // Smart mode - only if preset found for this file
        info.hasLUT = true;
    }

    if (info.hasLUT) {
        info.stepCount++;
    }

    // Step 5: Sharpening (if enabled)
    info.hasSharp = (settings.sharp_mode > -1);
    if (info.hasSharp) {
        info.stepCount++;
    }

    // Step 6: Write (always)
    info.stepCount++;

    return info;
}

std::tuple<std::string, std::string, std::string>
getOutName(std::string& path, std::string& baseName, std::string& extension, std::string& prest_sfx, Settings* settings)
{
    std::string outPath = path;
    if (settings->pathPrefix != "") {
        // Use fs::path to handle separator correctly
        fs::path p(outPath);
        p /= settings->pathPrefix;
        outPath = p.string();
    }
    std::string outName  = baseName;
    std::string proc_sfx = "_conv";
    std::string outExt;
    if (prest_sfx != "") {
        if (prest_sfx.rfind("_", 0) == 0) {  // startsWith "_"
            std::string temp_sfx;
            bool last_was_underscore = false;
            for (char c : prest_sfx) {
                if (c == '_') {
                    if (!last_was_underscore) {
                        temp_sfx += c;
                        last_was_underscore = true;
                    }
                } else {
                    temp_sfx += c;
                    last_was_underscore = false;
                }
            }
            prest_sfx = temp_sfx;
            proc_sfx  = prest_sfx;
        } else {
            proc_sfx = "_" + prest_sfx;
        }
    }

    outExt = getExtension(extension, settings);

    if (settings->useSbFldr) {
        fs::path p(outPath);
        p /= proc_sfx;
        outPath = p.string();
    } else {
        outName += proc_sfx;
    }
    return { outPath, outName, outExt };
}
