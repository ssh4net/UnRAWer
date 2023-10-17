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
//
    (*myPools)["LReader"]->enqueue(LReader, index, processing_entry, fileCntr, myPools);
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

// oiio file reader
void oReader(int index, std::shared_ptr<ProcessingParams>& processing_entry,
            std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {

        TypeDesc out_format;

        auto processing = processing_entry;

        LOG(info) << "Reader: file " << processing->srcFile << std::endl;

        QFileInfo fileInfo(processing->srcFile.c_str());
        if (fileInfo.isSymLink()) {
            std::string symLinkTarget = fileInfo.symLinkTarget().toStdString();
            LOG(debug) << "Reader: File is a symlink to: " << symLinkTarget << std::endl;
            processing->srcFile = symLinkTarget;
        }

        //  make this as a function
        ImageSpec config;
        config["raw:user_flip"] = settings.rawRot;

        config["oiio:UnassociatedAlpha"] = 0;
        config["tiff:UnassociatedAlpha"] = 0;
        config["oiio:ColorSpace"] = "Linear";
        // DENGEROUS!
        // LOG(info) << "ProPhoto color space" << std::endl;
        //
        config["raw:ColorSpace"] = settings.rawCspace[settings.rawSpace]; // raw, sRGB, sRGB-linear (sRGB primaries, but a linear transfer function), Adobe, Wide, ProPhoto, ProPhoto-linear, XYZ, ACES (only supported by LibRaw >= 0.18), DCI-P3 (LibRaw >= 0.21), Rec2020 (LibRaw >= 0.2). (Default: sRGB)
        config["raw:Demosaic"] = settings.demosaic[settings.dDemosaic + 1]; // linear, VNG, PPG, AHD (default), DCB, AHD-Mod, AFD, VCD, Mixed, LMMSE, AMaZE, DHT, AAHD, none
        //
        config["raw:use_camera_wb"] = settings.rawParms.use_camera_wb;     // If 1, use libraw camera white balance adjustment.
        config["raw:use_camera_matrix"] = settings.rawParms.use_camera_matrix; // 0 = never, 1 (default) = only for DNG files, 3 = always.
        config["raw:HighlightMode"] = settings.rawParms.highlight;     // 0 = clip, 1 = unclip, 2 = blend, 3 = reconstruct
        config["raw:aber"] = (settings.rawParms.aber[0], settings.rawParms.aber[1]);   //The default (1,1) means to perform no correction. This is an overall spatial scale, sensible values will be very close to 1.0.
        config["raw:Exposure"] = 1.0f;       // Exposure correction in stops before demosaic. Default: 1.0f

        config["raw:half_size"] = settings.rawParms.half_size; // If nonzero, use half-size color image (but return full size bayer image). Default: 0
        
        // required OIIO 2.6 with libraw demosaic PR
        // reset to defaults
        config["raw:threshold"] = 0.0f; 
        config["raw:fbdd_noiserd"] = 0;

        if (settings.denoise_mode == 1 || settings.denoise_mode == 3) {
			config["raw:threshold"] = settings.rawParms.denoise_thr; // Threshold for wavelet denoising (default: 0.0f)
		}
        if (settings.denoise_mode == 2 || settings.denoise_mode == 3) {
            config["raw:fbdd_noiserd"] = settings.rawParms.fbdd_noiserd; // FBDD noise threshold (default: 0)
        }

        ImageBuf inBuf(processing->srcFile, 0, 0, nullptr, &config, nullptr);

        if (!inBuf.init_spec(processing->srcFile, 0, 0)) {
            LOG(error) << "READ: Error reading " << processing->srcFile << std::endl;
            LOG(error) << "READ: " << inBuf.geterror() << std::endl;
            //mainWindow->emitUpdateTextSignal("Error! Check console for details");
            //return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN } };
            throw std::runtime_error("Reader: Could not read file: " + processing->srcFile);
        }

        TypeDesc orig_format = inBuf.spec().format;

        LOG(info) << "READ: File bith depth: " << formatText(orig_format) << std::endl;
        ///////////////////////////

        int last_channel = -1;

        ImageSpec& spec = inBuf.specmod();
        int nchannels = spec.nchannels;

        LOG(info) << "READ: CameraRaw Rotations: " << settings.rawRot << std::endl;

        TypeDesc o_format = spec.format; //getTypeDesc(settings.bitDepth);

        switch (o_format.basetype) {
        case TypeDesc::UINT8:
            o_format = TypeDesc::UINT16;
            break;
        case TypeDesc::HALF:
            o_format = TypeDesc::FLOAT;
            break;
        default:
            break;
        }

        (*fileCntr)--;

        bool read_ok = inBuf.read(0, 0, 0, last_channel, true, o_format, nullptr, nullptr);
        if (!read_ok) {
            LOG(error) << "READ: Error! Could not read input image\n";
            throw std::runtime_error("Reader: Could not read file: " + processing->srcFile);
            //mainWindow->emitUpdateTextSignal("Error! Check console for details");
            //return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN } };
        }

        // Get the image's spec
        const ImageSpec& ispec = inBuf.spec();

        // Get the format (bit depth and type)
        TypeDesc load_format = ispec.format;
        out_format = load_format; // copy latest buffer format as an output format

        LOG(info) << "READ: File loaded bit depth: " << formatText(load_format) << std::endl;

        // get the image size
        int width = inBuf.spec().width;
        int height = inBuf.spec().height;
        LOG(info) << "READ: Image size: " << width << "x" << height << std::endl;
        LOG(info) << "READ: Channels: " << inBuf.nchannels() << " Alpha channel index: " << inBuf.spec().alpha_channel << std::endl;

        //return { true, {std::make_shared<OIIO::ImageBuf>(outBuf), orig_format} };
        processing->image = std::make_shared<OIIO::ImageBuf>(inBuf);
        (*fileCntr)--;
        (*myPools)["OProcessor"]->enqueue(OProcessor, index, processing_entry, fileCntr, myPools);
}

// LibRaw buffer reader
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

// Libraw disk reader
void LReader(int index, std::shared_ptr<ProcessingParams>& processing_entry,
    std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;

    QFileInfo fileInfo(processing->srcFile.c_str());
    if (fileInfo.isSymLink()) {
        std::string symLinkTarget = fileInfo.symLinkTarget().toStdString();
        LOG(debug) << "Reader: File is a symlink to: " << symLinkTarget << std::endl;
        processing->srcFile = symLinkTarget;
    }

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

    LOG(info) << "Libraw Reader: file " << processing->srcFile << std::endl;

    //int ret = raw->open_buffer(raw_buffer->data(), raw_buffer->size());
    int ret = raw->open_file(processing->srcFile.c_str());
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "Reader: Cannot read file: " << processing->srcFile << std::endl;
        return;
    }

    (*fileCntr)--;
    
    (*myPools)["LUnpacker"]->enqueue(LUnpacker, index, processing_entry, fileCntr, myPools);
/*
    LOG(info) << "Unpack: file " << processing->srcFile << std::endl;
  
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
        (*myPools)["writer"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
    }
*/
}

// Libraw disk unpacker
void LUnpacker(int index, std::shared_ptr<ProcessingParams>& processing_entry,
               std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;
    LOG(info) << "Unpack: file " << processing->srcFile << std::endl;

    //LibRaw& raw = processing->raw_data;
    //std::shared_ptr<LibRaw> raw_ptr = std::make_shared<LibRaw>();
    //processing->raw_data = raw_ptr;
    //LibRaw* raw = raw_ptr.get();

    auto raw = processing->raw_data;

    int ret = raw->unpack();
    if (ret != LIBRAW_SUCCESS) {
        LOG(error) << "Unpack: Cannot unpack data from file: " << processing->srcFile << std::endl;
        return;
    }

    processing->setStatus(ProcessingStatus::Unpacked);

    if (settings.dDemosaic > -2) {
        (*fileCntr)--;
        (*myPools)["demosaic"]->enqueue(Demosaic, index, processing_entry, fileCntr, myPools);
    }
    else {
        (*fileCntr) -= 4; // can skip the writer
        (*myPools)["writer"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
    }
}

// Libraw buffer unpacker
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
            LOG(error) << "Demosaic: Cannot process data from file" << processing->srcFile << std::endl;
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr) -= 3;
        (*myPools)["writer"]->enqueue(Writer, index, processing_entry, fileCntr, myPools);
    }
    else if (settings.dDemosaic > -1) {
        raw_parms.output_bps = 16;
        raw_parms.user_qual = settings.dDemosaic;
        //auto x = raw->imgdata.rawparams.p4shot_order;

        if (raw->dcraw_process() != LIBRAW_SUCCESS) {
            LOG(error) << "Demosaic: Cannot process data from file" << processing->srcFile << std::endl;
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr)--;
        (*myPools)["dcraw"]->enqueue(Dcraw, index, processing_entry, fileCntr, myPools);
	}
    else {
        LOG(error) << "Demosaic: Unknown demosaic mode" << std::endl;
        return;
    }
}

// libraw dcraw dcraw_make_mem_image()
void Dcraw(int index, std::shared_ptr<ProcessingParams>& processing_entry,
           std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;

    std::shared_ptr<LibRaw> raw = processing->raw_data;

    LOG(debug) << "Dcraw: Processing data from file: " << processing->srcFile << std::endl;

    auto& raw_parms = raw->imgdata.params;
    raw_parms.output_bps = 16;

    //libraw_processed_image_t* image = raw->dcraw_make_mem_image();
    processing->raw_image = raw->dcraw_make_mem_image();

    if (!processing->raw_image) {
        LOG(error) << "Dcraw: Cannot process data from file: " << processing->srcFile << std::endl;
        return;
    }

    (*fileCntr)--;
    (*myPools)["processor"]->enqueue(Processor, index, processing_entry, fileCntr, myPools);
}

void Processor(int index, std::shared_ptr<ProcessingParams>& processing_entry,
               std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {

    auto processing = processing_entry;
    std::shared_ptr<LibRaw> raw = processing->raw_data;

    LOG(debug) << "Processor: Processing data from file: " << processing->srcFile << std::endl;

    //auto& raw_parms = raw->imgdata.params;
    //raw_parms.output_bps = 16;

    ////libraw_processed_image_t* image = raw->dcraw_make_mem_image();
    //processing->raw_image = raw->dcraw_make_mem_image();
    libraw_processed_image_t* image = processing->raw_image;

    //if (!image) {
    //    LOG(error) << "Processor: Cannot process data from fiel: " << processing->srcFile << std::endl;
    //    return;
    //}

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

void OProcessor(int index, std::shared_ptr<ProcessingParams>& processing_entry,
               std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;

    LOG(debug) << "Processor: Processing data from file: " << processing->srcFile << std::endl;

    ImageBuf* input_buf = processing->image.get();

    //auto [process_ok, out_buf] = imgProcessor(*image_buf, procGlobals.ocio_conf_ptr.get(), &settings.dLutPreset, processing_entry, image, nullptr, nullptr);
    //if (!process_ok) {
    //    LOG(error) << "Error processing " << processing->srcFile << std::endl;
    //    //mainWindow->emitUpdateTextSignal("Error! Check console for details");
    //    return;
    //}
    ImageBuf out_buf; // result_buf, rgba_buf, original_alpha, bit_alpha_buf;
    ImageBuf lut_buf;
    ImageBuf uns_buf;
    ImageBuf* out_buf_ptr = &out_buf;
    ImageBuf* lut_buf_ptr = &lut_buf;
    ImageBuf* uns_buf_ptr = &uns_buf;
    // LUT Transform
    bool lutValid = false;
    // check if lut_preset is not nullptr set lutValid to true
    if (settings.dLutPreset != "") {
        lutValid = true;
    }
    //auto test = input_buf.spec();
    //std::cout << test.width << " " << test.height << " " << test.nchannels << std::endl;
    LOG(trace) << "Input image: " << input_buf->spec().width << "x" << input_buf->spec().height << "x" << input_buf->spec().nchannels << std::endl;
    LOG(trace) << "Input image: " << input_buf->spec().format << std::endl;

    if (settings.lutMode >= 0 && lutValid) {
        auto lutPreset = settings.lut_Preset[settings.dLutPreset];
        if (ImageBufAlgo::ociofiletransform(*lut_buf_ptr, *input_buf, lutPreset, false, false, procGlobals.ocio_conf_ptr.get())) {
            LOG(info) << "LUT preset " << settings.dLutPreset << " <" << lutPreset << "> " << " applied" << std::endl;
            processing_entry->setStatus(ProcessingStatus::Graded);
            input_buf->clear();
        }
        else {
            LOG(error) << "LUT not applied: " << lut_buf.geterror() << std::endl;
            lut_buf_ptr = &*input_buf;
        }
    }
    else {
        LOG(debug) << "LUT transformation disabled" << std::endl;
        lut_buf_ptr = &*input_buf;
    }

    (*fileCntr)--;
    // Apply denoise
    // Apply unsharp mask

    if (settings.sharp_mode != -1) {
        string_view kernel = settings.sharp_kerns[settings.sharp_kernel];
        float width = settings.sharp_width;
        float contrast = settings.sharp_contrast;
        float threshold = settings.sharp_tresh;
        if (ImageBufAlgo::unsharp_mask(*uns_buf_ptr, *lut_buf_ptr, kernel, width, contrast, threshold)) {
            LOG(debug) << "Unsharp mask applied: <" << kernel.c_str() << ">" << std::endl;
            processing_entry->setStatus(ProcessingStatus::Unsharped);
            lut_buf_ptr->clear();
        }
        else {
            LOG(error) << "Unsharp mask not applied: " << uns_buf.geterror() << std::endl;
            uns_buf_ptr = lut_buf_ptr;
        }
    }
    else
    {
        LOG(debug) << "Unsharp mask disabled" << std::endl;
        uns_buf_ptr = lut_buf_ptr;
    }

    // temp copy for saving
    out_buf_ptr = uns_buf_ptr;
    //raw->dcraw_clear_mem(image);
    //raw->recycle();
    //image_buf.clear();

    processing->image = std::make_shared<ImageBuf>(*out_buf_ptr);
    processing->outSpec = std::make_shared<OIIO::ImageSpec>(out_buf_ptr->spec());

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

        bool write_ok = img_write(processing->image, outFilePath, TypeDesc::UINT16, TypeDesc::UINT16, nullptr, nullptr);
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

void Dummy(int index, std::shared_ptr<ProcessingParams>& processing_entry,
    std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools) {
    auto processing = processing_entry;

    if (!processing->rawCleared) {
        processing->raw_data->dcraw_clear_mem(processing->raw_image);
        processing->rawCleared = true;
    }

    (*fileCntr)--;
}
