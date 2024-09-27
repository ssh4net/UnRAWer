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

//#include "ui.h"

#include "Unrawer.h"
#include "stdafx.h"
#include "imageio.h"
#include "process.h"
#include "processors.h"

std::map<std::string, std::unique_ptr<ThreadPool>> myPools;
std::atomic_size_t fileCntr;
ProcessGlobals procGlobals;

bool doProgress(std::atomic_size_t* fileCntr, size_t files, QProgressBar* progressBar, MainWindow* mainWindow) {
    while (*fileCntr > 0) {
        float counts = static_cast<float>(files * 5); // 5 queues
        float progress = (counts - *fileCntr) / counts;
        bool ok = m_progress_callback(progressBar, progress);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return true;
}

bool doProcessing(QList<QUrl> urls, QProgressBar* progressBar, MainWindow* mainWindow) {
    std::vector<QString> fileNames;
    mTimer f_timer;

// todo: add support for user defined raw formats and move to global scope?
    auto raw_ext = OIIO::get_extension_map()["raw"];
    const std::unordered_set<std::string> raw_ext_set(raw_ext.begin(), raw_ext.end());
// end todo

	int count = 0;

    for (const QUrl& url : urls) {
        QString fileString = url.toLocalFile();
        if (!fileString.isEmpty()) {
            QFileInfo fileInfo(fileString);
            if (fileInfo.isDir()) {
                LOG(trace) << "SORT: Directory: " << fileInfo.absoluteFilePath().toStdString() << std::endl;
                QDirIterator it(fileInfo.absoluteFilePath(), QDir::Files | QDir::NoDotAndDotDot, 
                                QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
                while (it.hasNext()) {
                    QString file = it.next();
                    if (isRaw(file, raw_ext_set)) {
                        fileNames.push_back(file);
						count++;
                    }
                    else {
                        LOG(error) << "SORT: Not a raw file: " << file.toStdString() << std::endl;
                    }
                    LOG(trace) << "SORT File: " << file.toStdString() << std::endl;
                }

            }
			else {
                if (isRaw(fileString, raw_ext_set)) {
                    fileNames.push_back(fileString);
					count++;
                }
                else {
                    LOG(error) << "SORT: Not a raw file: " << fileString.toStdString() << std::endl;
                }
                LOG(trace) << "SORT: File: " << fileString.toStdString() << std::endl;
			}
        }
    }

	if (count == 0) {
        qDebug() << "No raw files found!";
		return false;
	}

    //OIIO::ColorConfig ocio_conf(settings.ocioConfigPath); // load ocio config once

    procGlobals.ocio_conf_ptr = std::make_unique<OIIO::ColorConfig>(settings.ocioConfigPath);

    std::vector<std::future<bool>> results;

    //OutPaths outpaths_map;
    ///////////////////////////////////////////////////////////////////////////////////////////
    /// Multi-threading processing
    ///
    int preThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int readThreads = settings.numThreads > 0 ? settings.numThreads : 1;
    int unpackThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int demosaicThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int processThreads = settings.numThreads > 0 ? settings.numThreads : 1; // floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int writeThreads = settings.numThreads > 0 ? settings.numThreads : 1;

    int pre_size = 10000;
    int read_size = readThreads;        // 10
    int unpack_size = unpackThreads;    // 36
    int demosaic_size = demosaicThreads;// 36
    int process_size = processThreads;  // 10
    int write_size = writeThreads;      // 10

    myPools.emplace("progress", std::make_unique<ThreadPool>(1, 1));                            // Progress pool
    myPools.emplace("sorter", std::make_unique<ThreadPool>(preThreads, pre_size));              // Preprocessor pool
    //myPools.emplace("reader", std::make_unique<ThreadPool>(readThreads, read_size));          // Reader pool
    myPools.emplace("LReader", std::make_unique<ThreadPool>(readThreads, read_size));           // Libraw Reader pool
    //myPools.emplace("oReader", std::make_unique<ThreadPool>(readThreads, read_size));         // Reader pool
    //myPools.emplace("unpacker", std::make_unique<ThreadPool>(unpackThreads, unpack_size));    // Unpacker pool
    myPools.emplace("LUnpacker", std::make_unique<ThreadPool>(unpackThreads, unpack_size));    // Unpacker pool
    myPools.emplace("demosaic", std::make_unique<ThreadPool>(demosaicThreads, demosaic_size));  // Demosaic pool
    myPools.emplace("dcraw", std::make_unique<ThreadPool>(demosaicThreads, demosaic_size));     // dcraw Libraw pool
    //myPools.emplace("OProcessor", std::make_unique<ThreadPool>(processThreads, process_size));   // Processor pool
    myPools.emplace("processor", std::make_unique<ThreadPool>(processThreads, process_size));   // Processor pool
    myPools.emplace("writer", std::make_unique<ThreadPool>(writeThreads, write_size));          // Writer pool
    //myPools.emplace("dummy", std::make_unique<ThreadPool>(1, 1));                               // Dummy "Benchmark" pool

	std::vector<std::unique_ptr<ProcessingParams>> processingList(fileNames.size());
    fileCntr = fileNames.size() * 7; 
    // 6+1 queues: sorter, reader, unpacker, demosaic, processor (lut, unsharp), writer
    QString processText = "Processing steps : Load -> ";
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
    QString progressText = QString("Processing %1 files...\n").arg(fileNames.size()) + processText;
    
    mainWindow->emitUpdateTextSignal(progressText);
    myPools["progress"]->enqueue(doProgress, &fileCntr, fileNames.size(), progressBar, mainWindow);

    // Start the preprocessor tasks
    for (int i = 0; i < fileNames.size(); ++i) {
        myPools["sorter"]->enqueue(Sorter, i, fileNames[i], std::ref(processingList[i]), &fileCntr, &myPools);
    }

    myPools["sorter"]->waitForAllTasks();
    //myPools["oReader"]->waitForAllTasks();
    //myPools["reader"]->waitForAllTasks();
    myPools["LReader"]->waitForAllTasks();
    //myPools["unpacker"]->waitForAllTasks();
    myPools["LUnpacker"]->waitForAllTasks();
    myPools["demosaic"]->waitForAllTasks();
    myPools["dcraw"]->waitForAllTasks();
    myPools["processor"]->waitForAllTasks();
    //myPools["OProcessor"]->waitForAllTasks();
    myPools["writer"]->waitForAllTasks();
    //myPools["dummy"]->waitForAllTasks();
    myPools["progress"]->waitForAllTasks();


    mainWindow->emitUpdateTextSignal("Everything Done!");
    std::cout << "Total processing time : " << f_timer.nowText() << " for " << fileNames.size() << " files." << std::endl;
    bool ok = m_progress_callback(progressBar, 0.0f);
    return true;
}