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
    // Create a pool with 5 threads
    ThreadPool pool(settings.numThreads > 0 ? settings.numThreads : 1);
    //for (auto& thr_file : fileNames) {
    for (int i = 0; i < fileNames.size(); i++) {
        //qDebug() << "File name: " << fileName;
        
        QString prest_sfx = "";
        auto lut_preset = getPresetfromName(fileNames[i], &settings);
        if (lut_preset != nullptr) {
            prest_sfx = lut_preset->first.c_str();
        }

        if (settings.lutMode > 0) {
            // If LUT mode set to Force than use Default LUT preset
            // find lut_preset by dLutPreset name
            auto lut_preset_it = settings.lut_Preset.find(settings.dLutPreset);
            if (lut_preset_it != settings.lut_Preset.end()) { // if the preset was found in the map
                lut_preset = &*lut_preset_it;  // use the address of the pair pointed to by the iterator
                prest_sfx = lut_preset->first.c_str();
            }
            else
            {
                LOG(info) << "LUT preset " << settings.dLutPreset << " not found" << std::endl;
            }
        }
        else if (settings.lutMode == 0 && lut_preset != nullptr) {
            // LUT mode set to auto and file or path contains LUT preset name
            prest_sfx = lut_preset->first.c_str();
        }

        QString outName = getOutName(fileNames[i], prest_sfx, &settings);
        
        std::string infile = fileNames[i].toStdString();
        std::string outfile = outName.toStdString();

        QString DebugText = "Source: " + QFileInfo(fileNames[i]).fileName() +
                          "\nTarget: " + QFileInfo(outName).fileName();

        mainWindow->emitUpdateTextSignal(DebugText);
        // check if outName folder exists
        QDir dir(QFileInfo(outName).absolutePath());
        if (!dir.exists()) {
			// make it
            dir.mkpath(".");
		}
        results.emplace_back(pool.enqueue(unrawer_main, infile, outfile, &ocio_conf, lut_preset, progressBar, mainWindow));
    }
    for (auto&& result : results) {
        result.get();
    }

    mainWindow->emitUpdateTextSignal("Everything Done!");
    LOG(info) << "Total processing time : " << f_timer.nowText() << " for " << fileNames.size() << " files." << std::endl;
    return true;
}