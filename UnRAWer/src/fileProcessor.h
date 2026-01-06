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

#include <cctype>
#include <string>
#include <algorithm>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <atomic>

#include "imageio.h"

#ifndef FILEPROCESSOR_H
#    define FILEPROCESSOR_H

struct Settings;  // forward declaration

class OutPaths {
public:
    std::pair<bool, size_t> try_add(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (t_paths.find(path) != t_paths.end()) {  // Path is already in the set
            size_t i = t_paths[path];
            return { true, i };  // Return "found" and the index of the path
        } else {                 // Add the path to the set
            size_t i      = m_paths.size();
            t_paths[path] = i;
            m_paths.push_back({ path, false });
            return { false, i };  // Return "not found" and the index of the added path
        }
    }

    std::string get_path(size_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_paths[index].first;
    }

    bool get_path_status(size_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_paths[index].second;
    }

    void set_path_status(size_t index, bool status)
    {
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

// Step-based progress tracking
struct StepProgress {
    std::atomic<size_t> completedSteps { 0 };
    std::atomic<size_t> totalSteps { 0 };
    std::atomic<size_t> completedFiles { 0 };
    size_t totalFiles { 0 };
    std::mutex mutex;

    // Per-file step tracking
    struct FileSteps {
        std::atomic<size_t> completed { 0 };
        size_t total { 0 };
        bool failed { false };

        FileSteps() : completed(0), total(0), failed(false) {}
        FileSteps(const FileSteps& other)
            : completed(other.completed.load()), total(other.total), failed(other.failed)
        {
        }
        FileSteps(FileSteps&& other) noexcept
            : completed(other.completed.load()), total(other.total), failed(other.failed)
        {
        }
        FileSteps& operator=(const FileSteps& other)
        {
            completed.store(other.completed.load());
            total  = other.total;
            failed = other.failed;
            return *this;
        }
        FileSteps& operator=(FileSteps&& other) noexcept
        {
            completed.store(other.completed.load());
            total  = other.total;
            failed = other.failed;
            return *this;
        }
    };
    std::vector<FileSteps> fileSteps;

    void
    initialize(size_t numFiles)
    {
        totalFiles      = numFiles;
        completedFiles  = 0;
        completedSteps  = 0;
        totalSteps      = 0;
        fileSteps.resize(numFiles);
    }

    void
    setFileStepCount(size_t fileIndex, size_t stepCount)
    {
        fileSteps[fileIndex].total = stepCount;
        totalSteps += stepCount;
    }

    void
    incrementStep(size_t fileIndex)
    {
        fileSteps[fileIndex].completed++;
        completedSteps++;
    }

    void
    markFileComplete(size_t fileIndex, bool success)
    {
        if (!success) {
            fileSteps[fileIndex].failed = true;
            // Add remaining steps to maintain progress
            size_t completed = fileSteps[fileIndex].completed.load();
            size_t total     = fileSteps[fileIndex].total;
            size_t remaining = total > completed ? total - completed : 0;
            completedSteps += remaining;
        }
        completedFiles++;
    }

    float
    getProgress() const
    {
        size_t total = totalSteps.load();
        if (total == 0)
            return 0.0f;
        size_t completed = completedSteps.load();
        return static_cast<float>(completed) / static_cast<float>(total);
    }

    std::string
    getStatusString() const
    {
        size_t done          = completedFiles.load();
        size_t steps         = completedSteps.load();
        size_t totalStepsVal = totalSteps.load();
        return "Files: " + std::to_string(done) + "/" + std::to_string(totalFiles) + " | Steps: "
               + std::to_string(steps) + "/" + std::to_string(totalStepsVal);
    }
};

struct ProcessingParams {
    std::unique_ptr<OIIO::ImageBuf> image;
    // File paths:
    std::string srcFile;  // Source file full path name
    size_t outPathIdx;    // Index of the output path in the vector of output paths
    std::string outFile;  // Output file name without extension
    std::string outExt;   // Output file name extension
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

    struct raw_crop {
        int left, top, width, height;
    } m_crops;

    struct exif {
        std::string make, model, lens, serial, software, date, time;
        float fnumber, focal, iso, shutter;
        int width, height;
    } m_exif;

    ProcessingStatus status = ProcessingStatus::NotStarted;
    // TODO: maybe add mutex for raw clear status
    bool rawCleared = false;

    // internal
    std::mutex statusMutex;

    // Step tracking
    size_t fileIndex;
    StepProgress* progressTracker = nullptr;
    size_t expectedSteps          = 0;

    void
    initializeSteps(size_t stepCount)
    {
        expectedSteps = stepCount;
        if (progressTracker) {
            progressTracker->setFileStepCount(fileIndex, stepCount);
        }
    }

    void
    setStatus(ProcessingStatus newStatus)
    {
        std::lock_guard<std::mutex> lock(statusMutex);

        ProcessingStatus oldStatus = status;
        status                     = newStatus;

        // Only increment step if moving forward (not to Failed)
        bool isStepComplete = (newStatus == ProcessingStatus::Loaded || newStatus == ProcessingStatus::Unpacked
                               || newStatus == ProcessingStatus::Demosaiced || newStatus == ProcessingStatus::Processed
                               || newStatus == ProcessingStatus::Graded || newStatus == ProcessingStatus::Unsharped
                               || newStatus == ProcessingStatus::Written);

        if (isStepComplete && progressTracker && oldStatus != ProcessingStatus::Failed) {
            progressTracker->incrementStep(fileIndex);
        }

        // Handle completion
        if (newStatus == ProcessingStatus::Written) {
            if (progressTracker) {
                progressTracker->markFileComplete(fileIndex, true);
            }
        }
        // Handle failure - mark as complete to keep progress moving
        else if (newStatus == ProcessingStatus::Failed && oldStatus != ProcessingStatus::Failed) {
            if (progressTracker) {
                progressTracker->markFileComplete(fileIndex, false);
                spdlog::warn("File {} failed at processing step", srcFile);
            }
        }
    }

    ProcessingStatus
    getStatus()
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return status;
    }
};

struct ProcessGlobals {
    std::unique_ptr<OIIO::ColorConfig> ocio_conf_ptr;  // per session color config load
    struct PreviewSink {
        using EnqueueFn = void (*)(void* user, const char* out_file_path, int file_index1, int total_files);
        std::atomic<EnqueueFn> enqueue { nullptr };
        std::atomic<void*> user { nullptr };
    } previewSink;
};

extern ProcessGlobals procGlobals;

#endif  // FILEPROCESSOR_H

std::string
toLower(const std::string& str);

void
getWritableExt(std::string* ext, Settings* settings);

std::string
getExtension(std::string& extension, Settings* settings);

std::tuple<std::string, std::string, std::string, std::string>
splitPath(const std::string& fileName);

std::optional<std::string>
getPresetfromName(const std::string& fileName, Settings* settings);

std::tuple<std::string, std::string, std::string>
getOutName(std::string& path, std::string& baseName, std::string& extension, std::string& prest_sfx,
           Settings* settings);

// Helper struct for step counting
struct FileStepInfo {
    size_t stepCount;
    bool hasLUT;
    bool hasDemosaic;
    bool hasSharp;
};

// Count expected processing steps for a file based on settings and LUT preset availability
FileStepInfo
countExpectedSteps(const Settings& settings, const std::optional<std::string>& lut_preset);
