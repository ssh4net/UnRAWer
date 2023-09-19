/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

//#include <toml++/toml.h>
#include <toml.hpp>
#include <filesystem>
#include "settings.h"
#include "Log.h"

Settings settings;

bool isValidPath(const std::string& path) {
    // List of characters not allowed in file paths.
    const std::string disallowedChars = "<>:\"|?*";

    for (char ch : path) {
        if (disallowedChars.find(ch) != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool loadSettings(Settings& settings, const std::string& filename) {
    try {
        auto parsed = toml::parse(filename);

        if (!parsed.contains("Global") || parsed["Global"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Global] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Range") || parsed["Range"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Range] section not found or empty." << std::endl;
            return false;
        }
        // Lut transform
        if (!parsed.contains("Transform") || parsed["Transform"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Transform] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Export") || parsed["Export"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Export] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("CameraRaw") || parsed["CameraRaw"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("OCIO") || parsed["OCIO"].as_table().empty()) {
			LOG(error) << "Error parsing settings file: [OCIO] section not found or empty." << std::endl;
			return false;
		}
        // Lut Preset
        if (!parsed.contains("LUT_Preset")) {
            LOG(error) << "Error parsing settings file: [LUT_Preset] section not found." << std::endl;
			return false;
        }
        // Unsharp
        if (!parsed.contains("Unsharp") || parsed["Unsharp"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Unsharp] section not found." << std::endl;
            return false;
        }
        ///////////////////////////
        auto check = [&parsed](const std::string& section, const std::string& key) {
            auto ts = parsed[section].as_table().find(key);
            if (ts == parsed[section].as_table().end()) {
                LOG(error) << "Warning: [" << section.c_str() << "] section does not contain \"" << key.c_str() << "\" key." << std::endl;
                return false;
            }
            return true;
        };
        // Global
        if (!check("Global", "Console")) return false;
        settings.conEnable = parsed["Global"]["Console"].as_boolean();

        if (!check("Global", "Threads")) return false;
        settings.numThreads = parsed["Global"]["Threads"].as_integer();
        if (!check("Global", "ThredsMult")) return false;
        settings.mltThreads = parsed["Global"]["ThredsMult"].as_floating();
        if (settings.mltThreads <= 0.0f) {
			LOG(error) << "Error parsing settings file: [Global] section: \"ThredsMult\" key value should be more than 0." << std::endl;
			return false;
		}

        if (!check("Global", "ExportSubf")) return false;
        settings.useSbFldr = parsed["Global"]["ExportSubf"].as_boolean();

        if (!check("Global", "PathPrefix")) return false;
        settings.pathPrefix = parsed["Global"]["PathPrefix"].as_string();
        if (!isValidPath(settings.pathPrefix)) {
            LOG(error) << "Error parsing settings file: [Global] section: \"PathPrefix\" key value contains invalid characters." << std::endl;
        }

        if (!check("Global", "Verbosity")) return false;
        settings.verbosity = parsed["Global"]["Verbosity"].as_integer();
        if (settings.verbosity < 0 || settings.verbosity > 5) {
			LOG(error) << "Error parsing settings file: [Global] section: \"Verbosity\" key value is out of range." << std::endl;
			return false;
		}

        // Range
        if (!check("Range", "RangeMode")) return false;
        settings.rangeMode = parsed["Range"]["RangeMode"].as_integer();
        if (settings.rangeMode < 0 || settings.rangeMode > 3) {
            LOG(error) << "Error parsing settings file: [Range] section: \"RangeMode\" key value is out of range." << std::endl;
            return false;
        }

        // Export
        if (!check("Export", "DefaultFormat")) return false;
        settings.defFormat = parsed["Export"]["DefaultFormat"].as_integer();
        if (settings.defFormat < 0 || settings.defFormat > 5) {
            LOG(error) << "Error parsing settings file: [Export] section: \"DefaultFormat\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "FileFormat")) return false;
        settings.fileFormat = parsed["Export"]["FileFormat"].as_integer();// .value_or(-1);
        if (settings.fileFormat < -1 || settings.fileFormat > 5) {
            LOG(error) << "Error parsing settings file: [Export] section: \"FileFormat\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "DefaultBit")) return false;
        settings.defBDepth = parsed["Export"]["DefaultBit"].as_integer();;
        if (settings.defBDepth < 0 || settings.defBDepth > 6) {
            LOG(error) << "Error parsing settings file: [Export] section: \"DefaultBit\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "BitDepth")) return false;
        settings.bitDepth = parsed["Export"]["BitDepth"].as_integer();//.value_or(-1);
        if (settings.bitDepth < -1 || settings.bitDepth > 6) {
            LOG(error) << "Error parsing settings file: [Export] section: \"BitDepth\" key value is out of range." << std::endl;
            return false;
        }
        // CameraRaw
        if (!check("CameraRaw", "RawRotation")) return false;
        settings.rawRot = parsed["CameraRaw"]["RawRotation"].as_integer();
        if (settings.rawRot < -1 || settings.rawRot > 6) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section: \"RawRotation\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("CameraRaw", "RawColorSpace")) return false;
        settings.rawSpace = parsed["CameraRaw"]["RawColorSpace"].as_integer();
        if (settings.rawSpace < 0 || settings.rawSpace > 10) {
			LOG(error) << "Error parsing settings file: [CameraRaw] section: \"RawColorSpace\" key value is out of range." << std::endl;
			return false;
		}
        if (!check("CameraRaw", "Demosaic")) return false;
        settings.dDemosaic = parsed["CameraRaw"]["Demosaic"].as_integer();
        if (settings.dDemosaic < -2 || settings.dDemosaic > 12) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section: \"Demosaic\" key value is out of range." << std::endl;
			return false;
        }
        if (!check("CameraRaw", "half_size")) return false;
        settings.rawParms.half_size = static_cast<int>(parsed["CameraRaw"]["half_size"].as_boolean());
        if (!check("CameraRaw", "use_auto_wb")) return false;
        settings.rawParms.use_auto_wb = static_cast<int>(parsed["CameraRaw"]["use_auto_wb"].as_boolean());
        if (!check("CameraRaw", "use_camera_wb")) return false;
        settings.rawParms.use_camera_wb = static_cast<int>(parsed["CameraRaw"]["use_camera_wb"].as_boolean());
        if (!check("CameraRaw", "use_camera_matrix")) return false;
        settings.rawParms.use_camera_matrix = parsed["CameraRaw"]["use_camera_matrix"].as_integer();
        if (settings.rawParms.use_camera_matrix < 0 || settings.rawParms.use_camera_matrix > 3) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section: \"use_camera_matrix\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("CameraRaw", "highlights")) return false;
        settings.rawParms.highlight = parsed["CameraRaw"]["highlights"].as_integer();
        if (settings.rawParms.highlight < 0 || settings.rawParms.highlight > 9) {
			LOG(error) << "Error parsing settings file: [CameraRaw] section: \"highlight\" key value is out of range." << std::endl;
            return false;
		}
        if (!check("CameraRaw", "aberrations")) return false;
        settings.rawParms.aber[0] = parsed["CameraRaw"]["aberrations"][0].as_floating();
        settings.rawParms.aber[1] = parsed["CameraRaw"]["aberrations"][1].as_floating();
        if (settings.rawParms.aber[0] < 0.0f || settings.rawParms.aber[1] < 0.0f) {
			LOG(error) << "Error parsing settings file: [CameraRaw] section: \"aberrations\" key value should use positive floats" << std::endl;
			return false;
		}

        // OCIO
        if (!check("OCIO", "OCIO_Config")) return false;
        settings.ocioConfigPath = parsed["OCIO"]["OCIO_Config"].as_string();//.value_or("");
        // check if file exists
        if (!std::filesystem::exists(settings.ocioConfigPath)) {
			LOG(error) << "Error parsing settings file: [OCIO] section: \"ocio_Config\" key value is invalid." << std::endl;
			return false;
		}
        // LUT_Preset
        auto lutPreset = parsed["LUT_Preset"].as_table();
        if (!lutPreset.empty()) {
            for (auto& [key, value] : lutPreset) {
				settings.lut_Preset.emplace(key, value.as_string());
			}
		} else {
			LOG(info) << "Parsing settings file: [LUT_Preset] section: \"LUT_Preset\" key value is empty." << std::endl;
		}
        // Transform
        if (!check("Transform", "LutTransform")) return false;
        settings.lutMode = parsed["Transform"]["LutTransform"].as_integer();//.value_or(-1);
        if (settings.lutMode < -1 || settings.lutMode > 1) {
			LOG(error) << "Error parsing settings file: [Transform] section: \"LutTransform\" key value is out of range." << std::endl;
			return false;
		}
        std::string lutDefault = parsed["Transform"]["LutDefault"].as_string();
        if (lutDefault != "") {
            // check if settings.lut_Preset[key] exists
            settings.dLutPreset = settings.lut_Preset.find(lutDefault)->first;
            if (settings.dLutPreset == "") {
				LOG(error) << "Error parsing settings file: [Transform] section: \"LutDefault\" key value is invalid." << std::endl;
				return false;
            }
        }
        // Unsharp
        if (!check("Unsharp", "sharp_mode")) return false;
        settings.sharp_mode = parsed["Unsharp"]["sharp_mode"].as_integer();
        if (settings.sharp_mode < -1 || settings.sharp_mode > 1) {
            LOG(error) << "Error parsing settings file: [Unsharp] section: \"sharp_mode\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Unsharp", "sharp_kernel")) return false;
        settings.sharp_kernel = parsed["Unsharp"]["sharp_kernel"].as_integer();
        if (settings.sharp_kernel < 0 || settings.sharp_kernel > 12) {
			LOG(error) << "Error parsing settings file: [Unsharp] section: \"sharp_kernel\" key value is out of range." << std::endl;
			return false;
		}
        if (!check("Unsharp", "sharp_width")) return false;
        settings.sharp_width = parsed["Unsharp"]["sharp_width"].as_floating();
        if (settings.sharp_width < 0.0f ) {
            LOG(error) << "Error parsing settings file: [Unsharp] section: \"sharp_width\" key value should be positive float" << std::endl;
        }
        if (!check("Unsharp", "sharp_contrast")) return false;
        settings.sharp_contrast = parsed["Unsharp"]["sharp_contrast"].as_floating();
        if (settings.sharp_contrast < 0.0f ) {
			LOG(error) << "Error parsing settings file: [Unsharp] section: \"sharp_contrast\" key value should be positive float" << std::endl;
			return false;
		}
        if (!check("Unsharp", "sharp_treshold")) return false;
        settings.sharp_tresh = parsed["Unsharp"]["sharp_treshold"].as_floating();
        if (settings.sharp_tresh < 0.0f) {
            LOG(error) << "Error parsing settings file: [Unsharp] section: \"sharp_treshold\" key value should bepositive float" << std::endl;
        }

        return true;
    }
    catch (const toml::syntax_error& err) {
        LOG(error) << "Error parsing settings file: " << err.what();
        return false;
    }
    catch (const toml::type_error& err) {
		LOG(error) << "Type Error: " << err.what();
		return false;
	}
    catch (const std::exception& ex) {
        LOG(error) << "Error loading settings file: " << ex.what();
        return false;
    }
}

void printSettings(Settings& settings) {
    QString mode;
    qDebug() << "--------- Settings ---------";

    qDebug() << "Parallel Threads: " << settings.numThreads;
    qDebug() << "Threads multiplier: " << settings.mltThreads;

    qDebug() << qPrintable(QString("Range Mode: %1").arg(mode));

    auto getMode = [](int fileFormat) {
        switch (fileFormat)
        {
        case 0:
            return QString("TIFF");
        case 1:
            return QString("OpenEXR");
        case 2:
            return QString("PNG");
        case 3:
            return QString("JPEG");
        case 4:
            return QString("JPEG-2000");
        case 5:
            return QString("PPM");
        default:
            return QString("Same as input");
        }
    };
    qDebug() << qPrintable(QString("File Format: %1").arg(getMode(settings.fileFormat)));
    qDebug() << qPrintable(QString("Default File Format: %1").arg(getMode(settings.defFormat)));

    auto getBitDepth = [](int bitDepth) {
		switch (bitDepth)
		{
		case 0:
			return QString("uint8");
		case 1:
			return QString("uint16");
		case 2:
			return QString("uint32");
		case 3:
			return QString("uint64");
		case 4:
			return QString("16bit (half) float");
		case 5:
			return QString("32bit float");
		case 6:
			return QString("64bit (double) float");
		default:
            return QString("Same as input");
		}
	};
    qDebug() << qPrintable(QString("Export Bit Depth: %1").arg(getBitDepth(settings.bitDepth)));
    qDebug() << qPrintable(QString("Default Export Bit Depth: %1").arg(getBitDepth(settings.defBDepth)));

    auto getRawRotation = [](int rawRot) {
        switch (rawRot)
        {
        case 0:
            return QString("Unrotated/Horisontal");
        case 3:
            return QString("180 Horisontal");
        case 5:
            return QString("90 CW Vertical");
        case 6:
            return QString("90 CCW Vertical");
        default:
            return QString("Auto EXIF");
        }
    };

    qDebug() << qPrintable(QString("Raw Rotation: %1").arg(getRawRotation(settings.rawRot)));
    qDebug() << qPrintable(QString("Half -size raw image: %1").arg(settings.rawParms.half_size == 0 ? "disabled" : "enabled"));
    qDebug() << qPrintable(QString("Use auto white balance: %1").arg(settings.rawParms.use_auto_wb == 0 ? "disabled" : "enabled"));
    qDebug() << qPrintable(QString("Use camera white balance: %1").arg(settings.rawParms.use_camera_wb == 0 ? "disabled" : "enabled"));
    qDebug() << qPrintable(QString("Use camera matrix: %1").arg(settings.rawParms.use_camera_matrix));
    qDebug() << qPrintable(QString("Highlight mode: %1").arg(settings.rawParms.highlight));
    qDebug() << qPrintable(QString("Aberrations: %1, %2")
        .arg(QString::number(settings.rawParms.aber[0], 'f', 2))
        .arg(QString::number(settings.rawParms.aber[1], 'f', 2)));
    qDebug() << qPrintable(QString("Raw Color Space: %1").arg(settings.rawSpace));

    if (settings.pathPrefix != "") {
        qDebug() << qPrintable(QString("Processed images will be saved to subfolder: %1").arg(settings.pathPrefix.c_str()));
    }

    qDebug() << qPrintable(QString("OCIO Config: %1").arg(settings.ocioConfigPath.c_str()));

    qDebug() << "----------------------------";
}
