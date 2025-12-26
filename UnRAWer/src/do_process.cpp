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

//#include "ui.h"
#include "pch.h"

#include "imageio.h"
#include "do_process.h"
#include "processors.h"
#include "settings.h"
#include "Timer.h"
#include <regex>

namespace fs = std::filesystem;

std::map<std::string, std::unique_ptr<ThreadPool>> myPools;
std::atomic_size_t fileCntr;

ProcessGlobals procGlobals;

// Step-based progress reporting
bool
doProgress(StepProgress* stepProgress, std::function<void(float, std::string)> callback,
           std::atomic<bool>* stopFlag)
{
    while (!(*stopFlag)) {
        float progress       = stepProgress->getProgress();
        std::string status = stepProgress->getStatusString();

        if (callback) {
            callback(progress, status);
        }

        // Check if all files are done
        if (stepProgress->completedFiles.load() >= stepProgress->totalFiles) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Final update
    if (callback)
        callback(1.0f, "Everything Done!");
    return true;
}

bool
doProcessing(const std::vector<std::string>& urls, std::function<void(float, std::string)> progressCallback)
{
    std::vector<std::string> fileNames;
    mTimer f_timer;

    // todo: add support for user defined raw formats and move to global scope?
    auto raw_ext = OIIO::get_extension_map()["raw"];
    const std::unordered_set<std::string> raw_ext_set(raw_ext.begin(), raw_ext.end());
    // end todo

    int count = 0;

    for (const auto& fileString : urls) {
        if (!fileString.empty()) {
            fs::path p(fileString);
            if (fs::exists(p) && fs::is_directory(p)) {
                spdlog::trace("SORT: Directory: {}", fs::absolute(p).string());
                try {
                    for (const auto& entry :
                         fs::recursive_directory_iterator(p, fs::directory_options::follow_directory_symlink)) {
                        if (entry.is_regular_file()) {
                            std::string file = entry.path().string();
                            if (isRaw(file, raw_ext_set)) {
                                fileNames.push_back(file);
                                count++;
                            } else {
                                spdlog::error("SORT: Not a raw file: {}", file);
                            }
                            spdlog::trace("SORT File: {}", file);
                        }
                    }
                } catch (const fs::filesystem_error& e) {
                    spdlog::error("Filesystem error: {}", e.what());
                }

            } else {
                if (isRaw(fileString, raw_ext_set)) {
                    fileNames.push_back(fileString);
                    count++;
                } else {
                    spdlog::error("SORT: Not a raw file: {}", fileString);
                }
                spdlog::trace("SORT: File: {}", fileString);
            }
        }
    }

    if (count == 0) {
        spdlog::error("No raw files found!");
        return false;
    }

    procGlobals.ocio_conf_ptr = std::make_unique<OIIO::ColorConfig>(settings.ocioConfigPath);

    std::vector<std::future<bool>> results;

    ///////////////////////////////////////////////////////////////////////////////////////////
    /// Multi-threading processing
    ///
    int preThreads      = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int readThreads     = settings.threads > 0 ? settings.threads : 1;
    int unpackThreads   = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int demosaicThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int processThreads  = settings.threads > 0 ? settings.threads : 1;
    int writeThreads    = settings.threads > 0 ? settings.threads : 1;

    int pre_size      = 10000;
    int read_size     = readThreads;      // 10
    int unpack_size   = unpackThreads;    // 36
    int demosaic_size = demosaicThreads;  // 36
    int process_size  = processThreads;   // 10
    int write_size    = writeThreads;     // 10

    myPools.emplace("progress", std::make_unique<ThreadPool>(1, 1));                            // Progress pool
    myPools.emplace("sorter", std::make_unique<ThreadPool>(preThreads, pre_size));              // Preprocessor pool
    myPools.emplace("rawReader", std::make_unique<ThreadPool>(readThreads, read_size));         // Libraw Reader pool
    myPools.emplace("LUnpacker", std::make_unique<ThreadPool>(unpackThreads, unpack_size));     // Unpacker pool
    myPools.emplace("demosaic", std::make_unique<ThreadPool>(demosaicThreads, demosaic_size));  // Demosaic pool
    myPools.emplace("dcraw", std::make_unique<ThreadPool>(demosaicThreads, demosaic_size));     // dcraw Libraw pool
    myPools.emplace("processor", std::make_unique<ThreadPool>(processThreads, process_size));   // Processor pool
    myPools.emplace("writer", std::make_unique<ThreadPool>(writeThreads, write_size));          // Writer pool

    std::vector<std::unique_ptr<ProcessingParams>> processingList(fileNames.size());
    fileCntr = fileNames.size() * 7;

    // Initialize step-based progress tracking
    StepProgress stepProgress;
    stepProgress.initialize(fileNames.size());
    std::atomic<bool> stopProgress { false };

    // 6+1 queues: sorter, reader, unpacker, demosaic, processor (lut, unsharp), writer
    std::string processText = "Processing steps : Load -> ";
    if (settings.denoise_mode > 0) {
        processText += "Denoise -> ";
        if (settings.dDemosaic > -1) {
            processText += "Demosaic -> ";
            if (settings.lutMode > -1) {
                processText += "Lut -> ";
            }
            if (settings.sharp_mode > -1) {
                processText += "Unsharp -> ";
            }
        }
    }
    processText += "Export";
    spdlog::info("Processing {} files... {}", fileNames.size(), processText);

    myPools["progress"]->enqueue(doProgress, &stepProgress, progressCallback, &stopProgress);

    // Start the preprocessor tasks
    for (int i = 0; i < fileNames.size(); ++i) {
        myPools["sorter"]->enqueue(Sorter, i, fileNames[i], std::ref(processingList[i]), &fileCntr, &myPools,
                                   &stepProgress);
    }

    myPools["sorter"]->waitForAllTasks();
    myPools["rawReader"]->waitForAllTasks();
    myPools["LUnpacker"]->waitForAllTasks();
    myPools["demosaic"]->waitForAllTasks();
    myPools["dcraw"]->waitForAllTasks();
    myPools["processor"]->waitForAllTasks();
    myPools["writer"]->waitForAllTasks();

    stopProgress = true;
    myPools["progress"]->waitForAllTasks();


    spdlog::info("Everything Done!");
    spdlog::info("Total processing time : {} for {} files.", f_timer.nowText(), fileNames.size());
    return true;
}
