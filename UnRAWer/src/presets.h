#pragma once

#include "settings.h"

#include <filesystem>
#include <string>
#include <vector>

enum class PresetStorage {
    LocalToml,
    PortableZip,
};

struct PresetEntry {
    std::string name;
    PresetStorage storage;
    std::filesystem::path path;
};

std::vector<PresetEntry>
ListPresets();
bool
LoadPreset(const PresetEntry& preset, Settings& settings);
bool
LoadPresetFile(const std::filesystem::path& presetPath, Settings& settings);
bool
SaveLocalPreset(const Settings& settings, const std::string& presetName);
bool
ExportPortablePreset(const Settings& settings, const std::string& presetName);
bool
ExportPortablePresetToFile(const Settings& settings, const std::string& presetName, const std::filesystem::path& outputPath);
bool
OpenPresetsFolder();
