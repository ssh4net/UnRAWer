/*
 * UnRAWer implementation using OpenImageIO
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
#include "processing.h"
#include "threadpool.h"
#include "fileProcessor.h"

#include <OpenImageIO/color.h>

#include <thread>
#include <mutex>
#include <condition_variable>

struct ThreadPools {
    ThreadPool& sorterPool;
    ThreadPool& readerPool;
    ThreadPool& unpackerPool;
    ThreadPool& processorPool;
    ThreadPool& writerPool;
};

void Reader(int index, std::shared_ptr<ProcessingParams>& processing_entry, const ThreadPools& pools);
void Unpacker(int index, std::shared_ptr<ProcessingParams>& processing_entry, std::shared_ptr<std::vector<char>> raw_buffer_ptr, const ThreadPools& pools);
void Writer(int index, std::shared_ptr<ProcessingParams>& processing_entry, bool isDemosaic, const ThreadPools& pools);

void Sorter(int index, QString fileName, std::shared_ptr<ProcessingParams> &processing_entry, const ThreadPools& pools) {
    auto processing = std::make_shared<ProcessingParams>();
    processing->srcFile = fileName.toStdString();

    QString prest_sfx = "";
    std::optional<std::string> lut_preset = getPresetfromName(fileName, &settings); // std::string or std::nullopt
    if (lut_preset.has_value()) {
        prest_sfx = lut_preset.value().c_str();
    }
    else {
        LOG(warning) << "PRE: No suitable LUT preset was found from file name" << std::endl;
    }

    if (settings.lutMode > 0) {
        // If LUT mode set to Force than use Default LUT preset find lut_preset by dLutPreset name
        auto lut_preset_it = settings.lut_Preset.find(settings.dLutPreset);
        if (lut_preset_it != settings.lut_Preset.end()) { // if the preset was found in the map
            lut_preset = lut_preset_it->first;
            prest_sfx = lut_preset.value().c_str();
        }
        else
        {
            // lut_preset remains empty or has the value from file name
            LOG(error) << "PRE: LUT preset " << settings.dLutPreset << " not found" << std::endl;
        }
    }
    else if (settings.lutMode == 0 && lut_preset.has_value()) {
        // LUT mode set to auto and file or path contains LUT preset name
        prest_sfx = lut_preset.value().c_str();
    }

    processing->outFile = getOutName(fileName, prest_sfx, &settings).toStdString();
    processing->lut_preset = lut_preset.value_or("");

    LOG(debug) << "PRE: Preprocessing file " << processing->srcFile << " > " << processing->outFile << std::endl;

    processing_entry = processing;
    processing->setStatus(ProcessingStatus::Prepared);

    pools.readerPool.enqueue(Reader, index, processing_entry, pools);
}

void Reader(int index, std::shared_ptr<ProcessingParams>& processing_entry, const ThreadPools& pools) {
    auto processing = processing_entry;

    LOG(info) << "READ: file " << processing->srcFile << " : " << processing->outFile << std::endl;

    std::ifstream file(processing->srcFile, std::ios::binary | std::ios::ate);
    if (!file)
        LOG(error) << "RAW: Could not open file: " << processing->srcFile << std::endl;

    const auto fileSize = file.tellg();
    if (fileSize < 0)
        LOG(error) << "RAW: Could not determine size of file: " << processing->srcFile << std::endl;
    file.seekg(0);

    std::vector<char> raw_buffer(fileSize);

    if (!file.read(raw_buffer.data(), fileSize)) {
        throw std::runtime_error("RAW: Could not read file: " + processing->srcFile);
    }
    std::shared_ptr<std::vector<char>> raw_buffer_ptr = std::make_shared<std::vector<char>>(raw_buffer);

    processing->setStatus(ProcessingStatus::Loaded);
    file.close();

    pools.unpackerPool.enqueue(Unpacker, index, processing_entry, raw_buffer_ptr, pools);
}

void Unpacker(int index, std::shared_ptr<ProcessingParams>& processing_entry, std::shared_ptr<std::vector<char>> raw_buffer, const ThreadPools& pools) {
    auto processing = processing_entry;
    LOG(info) << "READ: file " << processing->srcFile << " : " << processing->outFile << std::endl;
    
    //LibRaw& raw = processing->raw_data;
    std::shared_ptr<LibRaw> raw_ptr = std::make_shared<LibRaw>();
    processing->raw_data = raw_ptr;
    LibRaw* raw = raw_ptr.get();

    int ret = raw->open_buffer(raw_buffer->data(), raw_buffer->size());
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "RAW: Cannot read buffer: " << processing->srcFile << std::endl;
        return;
    }

    ret = raw->unpack();
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "RAW: Cannot unpack data from file: " << processing->srcFile << std::endl;
        return;
    }

    auto& raw_parms = raw->imgdata.params;

    raw_parms.output_bps = 16;
    raw_parms.output_color = 1;

    bool isDemosaic = false;

    if (settings.dDemosaic > 0) {
        raw_parms.user_qual = settings.dDemosaic - 1;

        ret = raw->dcraw_process();
        if (ret != LIBRAW_SUCCESS) {
            LOG(error) << "RAW: Cannot process data from file" << processing->srcFile << std::endl;
            return;
        }

        processing->setStatus(ProcessingStatus::Demosaiced);
        isDemosaic = true;
    }
    else {
        raw_parms.user_qual = settings.dDemosaic - 1;
        raw_parms.no_interpolation = 1;

        processing->setStatus(ProcessingStatus::Loaded);
        isDemosaic = false;
    }

    pools.writerPool.enqueue(Writer, index, processing_entry, isDemosaic, pools);
}

void Writer(int index, std::shared_ptr<ProcessingParams>& processing_entry, bool isDemosaic, const ThreadPools& pools) {
    auto processing = processing_entry;
    //LibRaw& raw = processing->raw_data;
    std::shared_ptr<LibRaw> raw = processing->raw_data;

    LOG(info) << "WRITE: Writing data to file: " << processing->outFile << std::endl;
    if (!isDemosaic) {
        // Write raw data to a file
        std::ofstream output(processing->outFile, std::ios::binary);
        if (!output) {
            LOG(error) << "RAW: Cannot open output file" << processing->outFile << std::endl;
            return;
        }

        size_t pix_count = raw->imgdata.sizes.raw_width * raw->imgdata.sizes.raw_height;
        size_t raw_image_size = pix_count * sizeof(ushort);

        // Write PGM header
        output << "P5\n";
        output << raw->imgdata.sizes.raw_width << " " << raw->imgdata.sizes.raw_height << "\n";
        output << "65535\n";  // Max value for 16-bit data

        // Write raw data
        // output.write(reinterpret_cast<char*>(raws[pair.first].imgdata.rawdata.raw_image), raw_image_size);

        // Write raw data with swapped byte order
        for (size_t i = 0; i < pix_count; ++i) {
            ushort value = raw->imgdata.rawdata.raw_image[i];
            value = (value << 8) | (value >> 8);  // Swap bytes
            output.write(reinterpret_cast<char*>(&value), sizeof(ushort));
        }

        output.close();
    }
    else // writing color ppm/tiff
    {
        if (settings.fileFormat == -1) {
            if (settings.defFormat == 0)
                raw->imgdata.params.output_tiff = 1; // TIF
            else if (settings.defFormat == 5)
                raw->imgdata.params.output_tiff = 0; // PPM
            else {
                LOG(error) << "WRITE: Unknown file format" << std::endl;
                processing->raw_data.reset();
                return;
            }
        }

        int ret = raw->dcraw_ppm_tiff_writer(processing->outFile.c_str());
        if (ret != LIBRAW_SUCCESS) {
            LOG(error) << "WRITE: Cannot write image to file" << processing->srcFile << std::endl;
            processing->raw_data.reset();
            return;
        }
    }

    processing->setStatus(ProcessingStatus::Written);
    LOG(info) << "WRITE: Finished writing data to file: " << processing->outFile << std::endl;
    processing->raw_data.reset();
}

bool doProcessing(QList<QUrl> urls, QProgressBar* progressBar, MainWindow* mainWindow) {
    std::vector<QString> fileNames; // This will hold the names of all files
    Timer f_timer;

    for (const QUrl& url : urls) {
        QString fileName = url.toLocalFile();
        if (!fileName.isEmpty()) {
            fileNames.push_back(fileName);
        }
    }

    OIIO::ColorConfig ocio_conf(settings.ocioConfig);

    std::vector< std::future<bool> > results;
///////////////////////////////////////////////////////////////////////////////////////////
/// Multi-threading tests
///
    int preThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int readThreads = settings.numThreads > 0 ? settings.numThreads : 1;
    int unpackThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int processThreads = floor(std::thread::hardware_concurrency() * settings.mltThreads);
    int writeThreads = settings.numThreads > 0 ? settings.numThreads : 1;

    int pre_size = 10000;
    int read_size = readThreads * 2; // 20
    int unpack_size = unpackThreads * 2; // 72;
    int process_size = processThreads * 2; // 72;
    int write_size = writeThreads * 2; // 20

    ThreadPool poolSorter(preThreads, pre_size);            // Preprocessor pool
    ThreadPool poolReader(readThreads, read_size);          // Reader pool
    ThreadPool poolUnpacker(unpackThreads, unpack_size);    // Unpacker pool
    ThreadPool poolProcessor(processThreads, process_size); // Processor pool
    ThreadPool poolWriter(writeThreads, write_size);        // Writer pool

    ThreadPools threadPools = { poolSorter, poolReader, poolUnpacker, poolProcessor, poolWriter };

    //SafeQueue<std::pair<int, std::shared_ptr<ProcessingParams>>> url2ReaderQueue;
    //SafeQueue<std::pair<int, std::shared_ptr<std::vector<char>>>> read2UnpackQueue;
    //SafeQueue<std::pair<int, bool>> unp2WriteQueue(10);

    std::vector<std::shared_ptr<ProcessingParams>> processingList(fileNames.size()); // Initialize the list
    //std::atomic<bool> allPreprocessed(false);
    //std::vector<LibRaw> raws(fileNames.size());         // Initialize the list
    
    //std::atomic<int> _preTasks = fileNames.size();      // Number of preprocessor tasks
    //std::atomic<int> _readTasks = fileNames.size();     // Number of reader tasks
    //std::atomic<int> _unpackTasks = fileNames.size();   // Number of unpacker tasks
    //std::atomic<int> _writeTasks = fileNames.size();    // Number of writer tasks

    // Start the preprocessor tasks
    for (int i = 0; i < fileNames.size(); ++i) {
        poolSorter.enqueue(Sorter, i, fileNames[i], std::ref(processingList[i]), threadPools);
    }

    poolSorter.waitForAllTasks();
    poolReader.waitForAllTasks();
    poolUnpacker.waitForAllTasks();
    poolWriter.waitForAllTasks();

    int c = 0;

///////////////////////////////////////////////////////////////////////////////////////////
/* 
    // Create a pool with 5 threads
    ThreadPool pool(settings.numThreads > 0 ? settings.numThreads : 1);
    //for (auto& thr_file : fileNames) {
    for (int i = 0; i < fileNames.size(); i++) {
        //qDebug() << "File name: " << fileName;

        QString prest_sfx = "";
        std::optional<std::string> lut_preset = getPresetfromName(fileNames[i], &settings); // std::string or std::nullopt
        if (lut_preset.has_value()) {
            prest_sfx = lut_preset.value().c_str();
        }
        else {
            LOG(info) << "No suitable LUT preset was found from file name" << std::endl;
        }

        if (settings.lutMode > 0) {
            // If LUT mode set to Force than use Default LUT preset
            // find lut_preset by dLutPreset name
            auto lut_preset_it = settings.lut_Preset.find(settings.dLutPreset);
            if (lut_preset_it != settings.lut_Preset.end()) { // if the preset was found in the map
                lut_preset = lut_preset_it->first; 
                prest_sfx = lut_preset.value().c_str();
            }
            else
            {
                // lut_preset remains empty or has the value from file name
                LOG(error) << "LUT preset " << settings.dLutPreset << " not found" << std::endl;
            }
        }
        else if (settings.lutMode == 0 && lut_preset.has_value()) {
            // LUT mode set to auto and file or path contains LUT preset name
            prest_sfx = lut_preset.value().c_str();
        }

        QString outName = getOutName(fileNames[i], prest_sfx, &settings);
        
        std::string infile = fileNames[i].toStdString();
        std::string outfile = outName.toStdString();

        ProcessingParams params;
        params.srcFile = infile;
        params.outFile = outfile;
        params.lut_preset = lut_preset.value_or("");

        QString DebugText = "Source: " + QFileInfo(fileNames[i]).fileName() +
                          "\nTarget: " + QFileInfo(outName).fileName();

        mainWindow->emitUpdateTextSignal(DebugText);
        // check if outName folder exists
        QDir dir(QFileInfo(outName).absolutePath());
        if (!dir.exists()) {
			// make it
            dir.mkpath(".");
		}
        results.emplace_back(pool.enqueue(unrawer_main, infile, outfile, &ocio_conf, &lut_preset.value_or(""), progressBar, mainWindow));
    }
    for (auto&& result : results) {
        result.get();
    }
*/
    mainWindow->emitUpdateTextSignal("Everything Done!");
    std::cout << "Total processing time : " << f_timer.nowText() << " for " << fileNames.size() << " files." << std::endl;
    return true;
}