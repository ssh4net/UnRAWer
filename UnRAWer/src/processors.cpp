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

#include "processors.h"
#include "Unrawer.h"

OutPaths outpaths;

bool isRaw(QString file, const std::unordered_set<std::string>& raw_ext_set) {
    QFileInfo fileInfo(file);
    QString ext = fileInfo.suffix().toLower();

    if (raw_ext_set.find(ext.toStdString()) != raw_ext_set.end()) {
        return true;
    }
    return false;
}

void Sorter(int index, QString fileName, std::shared_ptr<ProcessingParams>& processing_entry, 
            std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {

    auto processing = std::make_shared<ProcessingParams>();
    processing->srcFile = fileName.toStdString();

    QString prest_sfx = "";
    auto [path, parentFolderName, baseName, extension] = splitPath(fileName);
    
    std::optional<std::string> lut_preset = getPresetfromName(parentFolderName + "/" + baseName, &settings); // std::string or std::nullopt
    if (lut_preset.has_value()) {
        prest_sfx = lut_preset.value().c_str();
    }
    else {
        LOG(debug) << "PRE: No suitable LUT preset was found from file name" << std::endl;
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
    auto [outPath, outName, outExt] = getOutName(path, baseName, extension, prest_sfx, &settings);
    
    auto [exist, path_idx] = outpaths.try_add(outPath.toStdString());
    if (!exist) {
        LOG(debug) << "PRE: New output path added: " << outPath.toStdString() << std::endl;
    }
    processing->outPathIdx = path_idx;
    processing->outFile = outName.toStdString();
    processing->outExt = outExt.toStdString();
    processing->lut_preset = lut_preset.value_or("");
    LOG(debug) << "PRE: Preprocessing file " << processing->srcFile << " > " << outpaths.get_path(path_idx) + "/" + processing->outFile + processing->outExt << std::endl;

    processing_entry = processing;
    processing->setStatus(ProcessingStatus::Prepared);

    (*myPools)["reader"]->enqueue(Reader, index, processing_entry, fileCntr, myPools);
}

bool read_chunk(std::ifstream* file, std::vector<char>& raw_buffer, std::streamoff start, std::streamoff end) {
    std::vector<char> buffer(end - start);
    file->seekg(start);
    file->read(buffer.data(), end - start);
    if (!file) {
        return false;
    }
    std::copy(buffer.begin(), buffer.end(), raw_buffer.begin() + start);
    return true;
}

void Reader(int index, std::shared_ptr<ProcessingParams>& processing_entry,
            std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;

    LOG(info) << "Reader: file " << processing->srcFile << std::endl;

    QFileInfo fileInfo(processing->srcFile.c_str());
    if (fileInfo.isSymLink()) {
        std::string symLinkTarget = fileInfo.symLinkTarget().toStdString();
        LOG(debug) << "Reader: File is a symlink to: " << symLinkTarget << std::endl;
        processing->srcFile = symLinkTarget;
    }

    std::ifstream file(processing->srcFile, std::ios::binary | std::ios::ate);
    if (!file)
        LOG(error) << "Reader: Could not open file: " << processing->srcFile << std::endl;

    const auto fileSize = file.tellg();
    if (fileSize < 0)
        LOG(error) << "Reader: Could not determine size of file: " << processing->srcFile << std::endl;
    file.seekg(0);

    LOG(debug) << "Reader: File size: " << fileSize << std::endl;

    std::vector<char> raw_buffer(fileSize);

#if 0
    const int numThreads = 3;
    ThreadPool poolChRead(numThreads, settings.numThreads > 0 ? settings.numThreads : 1);

    std::streamoff chunkSize = fileSize / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        std::streamoff start = i * chunkSize;
        std::streamoff end = (i + 1) * chunkSize;
        if (i == numThreads - 1) {
            end = fileSize;
        }
        poolChRead.enqueue(read_chunk, std::ref(file), std::ref(raw_buffer), start, end);
    }
    poolChRead.waitForAllTasks();
#endif

#if 1
    if (!file.read(raw_buffer.data(), fileSize)) {
        throw std::runtime_error("Reader: Could not read file: " + processing->srcFile);
    }
#endif
    std::shared_ptr<std::vector<char>> raw_buffer_ptr = std::make_shared<std::vector<char>>(raw_buffer);

    processing->setStatus(ProcessingStatus::Loaded);
    file.close();

    (*fileCntr)--;

    (*myPools)["unpacker"]->enqueue(Unpacker, index, processing_entry, raw_buffer_ptr, fileCntr, myPools);
}

void Unpacker(int index, std::shared_ptr<ProcessingParams>& processing_entry, std::shared_ptr<std::vector<char>> raw_buffer,
              std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;
    LOG(info) << "Unpack: file " << processing->srcFile << std::endl;

    //LibRaw& raw = processing->raw_data;
    std::shared_ptr<LibRaw> raw_ptr = std::make_shared<LibRaw>();
    processing->raw_data = raw_ptr;
    LibRaw* raw = raw_ptr.get();

    raw->imgdata.params.use_camera_wb = settings.rawParms.use_camera_wb;
    raw->imgdata.params.use_camera_matrix = settings.rawParms.use_camera_matrix;
    raw->imgdata.params.highlight = settings.rawParms.highlight;
    raw->imgdata.params.aber[0] = settings.rawParms.aber[0];
    raw->imgdata.params.aber[1] = settings.rawParms.aber[1];
    //raw->imgdata.params.exp_correc = settings.rawParms.exp_correc;
    raw->imgdata.params.half_size = settings.rawParms.half_size;

    raw->imgdata.params.output_color = settings.rawSpace;

    raw->imgdata.params.user_flip = settings.rawRot;

    if (settings.denoise_mode == 1 || settings.denoise_mode == 3) {
        raw->imgdata.params.threshold = settings.rawParms.denoise_thr;
	}
	else {
        raw->imgdata.params.threshold = 0.0f;
	}
    
    if (settings.denoise_mode == 2 || settings.denoise_mode == 3) {
        raw->imgdata.params.fbdd_noiserd = settings.rawParms.fbdd_noiserd;
    }
    else {
		raw->imgdata.params.fbdd_noiserd = 0;
	}

    int ret = raw->open_buffer(raw_buffer->data(), raw_buffer->size());
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "Unpack: Cannot read buffer: " << processing->srcFile << std::endl;
        return;
    }

    ret = raw->unpack();
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "Unpack: Cannot unpack data from file: " << processing->srcFile << std::endl;
        return;
    }

    processing->setStatus(ProcessingStatus::Unpacked);

    (*fileCntr)--;

    if (settings.dDemosaic > -2) {
        (*myPools)["demosaic"]->enqueue(Demosaic, index, processing_entry, fileCntr, myPools);
    }
    else {
        (*fileCntr)--; // no demosaic, so we can skip the processor
        (*fileCntr)--; // no demosaic, so we can skip the writer
        (*myPools)["processor"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
    }
}

void Demosaic(int index, std::shared_ptr<ProcessingParams>& processing_entry,
              std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;
    std::shared_ptr<LibRaw> raw = processing->raw_data;
    LOG(info) << "Demosaic: file " << processing->srcFile << std::endl;

    auto& raw_parms = raw->imgdata.params;

    if (settings.dDemosaic == -1) {

        raw_parms.output_bps = 16;
        raw_parms.user_qual = settings.dDemosaic;
        raw_parms.no_interpolation = 1;

        if (raw->dcraw_process() != LIBRAW_SUCCESS) {
            LOG(error) << "Unpack: Cannot process data from file" << processing->srcFile << std::endl;
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr)--;
        (*fileCntr)--;
        (*myPools)["writer"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
    }
    else if (settings.dDemosaic > -1) {
        raw_parms.output_bps = 16;
        raw_parms.user_qual = settings.dDemosaic;

        if (raw->dcraw_process() != LIBRAW_SUCCESS) {
            LOG(error) << "Unpack: Cannot process data from file" << processing->srcFile << std::endl;
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr)--;
        (*myPools)["processor"]->enqueue(Processor, index, processing_entry, fileCntr, myPools);
	}
    else {
        LOG(error) << "Unpack: Unknown demosaic mode" << std::endl;
        return;
    }
}

void Processor(int index, std::shared_ptr<ProcessingParams>& processing_entry,
               std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;
    std::shared_ptr<LibRaw> raw = processing->raw_data;

    LOG(debug) << "Processor: Processing data from file: " << processing->srcFile << std::endl;

    auto& raw_parms = raw->imgdata.params;
    raw_parms.output_bps = 16;

    //libraw_processed_image_t* image = raw->dcraw_make_mem_image();
    processing->raw_image = raw->dcraw_make_mem_image();
    libraw_processed_image_t* image = processing->raw_image;

    if (!image) {
        LOG(error) << "Processor: Cannot process data from buffer: " << processing->srcFile << std::endl;
        return;
    }

    //raw_parms.output_color = 1;

    OIIO::ImageSpec image_spec(image->width, image->height, image->colors, OIIO::TypeDesc::UINT16);
    OIIO::ImageBuf image_buf(image_spec, image->data);

    //OIIO::ColorConfig ocio_conf(settings.ocioConfigPath);

    auto [process_ok, out_buf] = imgProcessor(std::ref<ImageBuf>(image_buf), procGlobals.ocio_conf_ptr.get(), &settings.dLutPreset, processing_entry, image, nullptr, nullptr);
    if (!process_ok) {
        LOG(error) << "Error processing " << processing->srcFile << std::endl;
        //mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return;
    }

    //raw->dcraw_clear_mem(image);
    //raw->recycle();
    //image_buf.clear();

    processing->image = out_buf;
    processing->outSpec = std::make_shared<OIIO::ImageSpec>(image_spec);

    processing->setStatus(ProcessingStatus::Processed);

    (*fileCntr)--;

    (*myPools)["writer"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
}

void Writer(int index, std::shared_ptr<ProcessingParams>& processing_entry,
            std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;
    //LibRaw& raw = processing->raw_data;
    std::shared_ptr<LibRaw> raw = processing->raw_data;

    // Check if the output path exists and create it if not
    std::string outDir = outpaths.get_path(processing->outPathIdx);
    if (!outpaths.get_path_status(processing->outPathIdx)) {
        // check if outFilePath folder exists
        QDir dir(QString::fromStdString(outDir));
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        outpaths.set_path_status(processing->outPathIdx, true);
    }

    std::string outFilePath = outDir + "/" + processing->outFile + processing->outExt;

    if (!makePath(outDir)) {
        LOG(error) << "Writer: Cannot create output directory" << outFilePath << std::endl;
		return;
    };

    LOG(info) << "Writer: Writing data to file: " << outFilePath << std::endl;
    if (settings.dDemosaic == -2) {
        // Write raw data to a file
        // TODO: move this to OIIO writer to fix file format issue and byte order
        outFilePath = outDir + "/" + processing->outFile + ".ppm";
        std::ofstream output(outFilePath, std::ios::binary); // hack to add .ppm extension
        if (!output) {
            LOG(error) << "Writer: Cannot open output file " << outFilePath << std::endl;
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
    else if (settings.dDemosaic == -1) // writing color ppm/tiff using dcraw_ppm_tiff_writer
    {
        if (settings.fileFormat == -1) {
            if (settings.defFormat == 0) {
                raw->imgdata.params.output_tiff = 1; // TIF
            }
            else if (settings.defFormat == 5) {
                raw->imgdata.params.output_tiff = 0; // PPM
            }
            else {
                LOG(error) << "Writer: Unknown file format. Format changed to *.tif" << std::endl;
                outFilePath = outDir + "/" + processing->outFile + ".tif";
                //processing->raw_data.reset();
                //return;
            }
        }

        int ret = raw->dcraw_ppm_tiff_writer(outFilePath.c_str());
        if (ret != LIBRAW_SUCCESS) {
            LOG(error) << "Writer: Cannot write image to file " << outFilePath << std::endl;
            processing->raw_data.reset();
            return;
        }
    }
    else { // Write processed image using oiio
        //////////////////////////////////////////////////
        /// Image saving
        /// 

        bool write_ok = img_write(*processing->image, outFilePath, TypeDesc::UINT16, TypeDesc::UINT16, nullptr, nullptr);
        if (!write_ok) {
            LOG(error) << "Error writing " << outFilePath << std::endl;
            //mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return;
        }

        if (!processing->rawCleared) {
            processing->raw_data->dcraw_clear_mem(processing->raw_image);
            processing->rawCleared = true;
        }

        processing->image->reset();
        processing->image.reset();
        //////////////////////////////////////////////////
    }

    processing->setStatus(ProcessingStatus::Written);
    LOG(debug) << "Writer: Finished writing data to file: " << outFilePath << std::endl;
    processing->raw_data.reset();

    (*fileCntr)--;
}