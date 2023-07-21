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

#include "Log.h"
#include "settings.h"
#include "imageio.h"
#include "ui.h"
#include "threadpool.h"

#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

enum class ProcessingStatus {
    NotStarted,
    Prepared,
    Loaded,
    Unpacked,
    Demosaiced,
    Processed,
    Written,
    Failed
};

struct ProcessingParams {
    
    std::shared_ptr<OIIO::ImageBuf> image;
    // File paths:
    std::string srcFile;
    std::string outFile;
    // RAW image pointer
    
    //LibRaw raw_data;
    std::shared_ptr<LibRaw> raw_data;
    
    // source settings:
    std::shared_ptr<OIIO::ImageSpec> srcSpec;
    
    // output settings:
    OIIO::TypeDesc outType;
    std::shared_ptr<OIIO::ImageSpec> outSpec;
    
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

#endif // FILEPROCESSOR_H

std::string toLower(const std::string& str);

void getWritableExt(QString* ext, Settings* settings);

QString getExtension(const QString& fileName, Settings* settings);

std::optional<std::string> getPresetfromName(const QString& fileName, Settings* settings);

QString getOutName(const QString& fileName, QString& prest_sfx, Settings* settings);

