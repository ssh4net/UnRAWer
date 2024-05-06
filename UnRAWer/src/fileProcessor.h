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
#pragma once

#include <cctype>
#include <string>
#include <algorithm>
#include <optional>
#include <unordered_set>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>

#include "Log.h"
#include "settings.h"
#include "imageio.h"
#include "ui.h"
#include "threadpool.h"

#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

class OutPaths {
public:
    std::pair<bool, int> try_add(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (t_paths.find(path) != t_paths.end()) { // Path is already in the set
            size_t i = t_paths[path];
			return { true, i }; 				   // Return "found" and the index of the path
        }
        else {                                     // Add the path to the set
            size_t i = m_paths.size();
            t_paths[path] = i;
            m_paths.push_back({ path, false });
            return { false, i };                   // Return "not found" and the index of the added path
        }
    }

    //void set_status(const std::string& path, bool status) {
    //    std::lock_guard<std::mutex> lock(m_mutex);
    //    t_paths[path] = status; // This will set the status for the path, whether it was in the map already or not
    //}
    //void get_status(const std::string& path, bool& status) {
    //    std::lock_guard<std::mutex> lock(m_mutex);
    //    status = t_paths[path];
    //};
    //size_t add_path(const std::string& path) { // Return the index of the added path and its status
	//	std::lock_guard<std::mutex> lock(m_mutex);
    //    size_t size = m_paths.size();
    //    m_paths.push_back({ path, false });
    //    return size; 
	//};

    std::string get_path(size_t index) {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_paths[index].first;
	}

    bool get_path_status(size_t index) {
        std::lock_guard<std::mutex> lock(m_mutex);
		return m_paths[index].second;
	}

    void set_path_status(size_t index, bool status) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_paths[index].second = status;
    }

private:
    std::unordered_map<std::string, size_t> t_paths;    // temp map of paths and their indexes in the vector
    std::vector<std::pair<std::string, bool>> m_paths;  // vector of paths and their statuses
    std::mutex m_mutex;
};

enum class ProcessingStatus {
    NotStarted,
    Prepared,
    Loaded,
    Unpacked,
    Demosaiced,
    Processed,
    Graded,
    Unsharped,
    Written,
    Failed
};

struct ProcessingParams {
    
    std::unique_ptr<OIIO::ImageBuf> image;
    // File paths:
    std::string srcFile; // Source file full path name
    int outPathIdx;      // Index of the output path in the vector of output paths
    std::string outFile; // Output file name without extension
    std::string outExt;  // Output file name extension
    // RAW image pointer
    
    //LibRaw raw_data;
    std::unique_ptr<LibRaw> raw_data;
    libraw_processed_image_t* raw_image;
    // source settings:
    std::unique_ptr<OIIO::ImageSpec> srcSpec;
    
    // output settings:
    OIIO::TypeDesc outType;
    std::unique_ptr<OIIO::ImageSpec> outSpec;
    
    // Color config:
    std::string srcCSpace;
    std::string outCSpace;
    
    // Processing params:
    std::string lut_preset;
    
    // Filters:
    struct sharpening {
        bool enabled;
        float amount;
        float radius;
        float threshold;
    } sharp;

    struct denoising {
        bool enabled;
        float sigma;
    } denoise;

    ProcessingStatus status = ProcessingStatus::NotStarted;
    // TODO: maybe add mutex for raw clear status
    bool rawCleared = false;

    // internal
    std::mutex statusMutex;

    void setStatus(ProcessingStatus newStatus) {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = newStatus;
    }

    ProcessingStatus getStatus() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return status;
    }
};

struct ProcessGlobals {
    std::unique_ptr<OIIO::ColorConfig> ocio_conf_ptr; // per session color config load
};

extern ProcessGlobals procGlobals;

#endif // FILEPROCESSOR_H

std::string toLower(const std::string& str);

void getWritableExt(QString* ext, Settings* settings);

QString getExtension(QString& extension, Settings* settings);

std::tuple<QString, QString, QString, QString> splitPath(const QString& fileName);

std::optional<std::string> getPresetfromName(const QString& fileName, Settings* settings);

std::tuple<QString, QString, QString> getOutName(QString& path, QString& baseName, QString& extension, QString& prest_sfx, Settings* settings);

