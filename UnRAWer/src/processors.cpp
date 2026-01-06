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
#include "pch.h"

#include "processors.h"
#include "exif_parser.h"
#include "settings.h"

namespace fs = std::filesystem;

#define DEBWRT 0

OutPaths outpaths;

bool
isRaw(const std::string& file, const std::unordered_set<std::string>& raw_ext_set)
{
    fs::path p(file);
    std::string ext = toLower(p.extension().string());
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    if (raw_ext_set.find(ext) != raw_ext_set.end()) {
        return true;
    }
    return false;
}

bool
setCrops(int index, std::unique_ptr<ProcessingParams>& processing_entry)
{
    auto& processing = processing_entry;
    bool no_error    = true;

    // Estimate Crop for ImageBuf w.r.t. orientation
    auto crops = processing->raw_data->imgdata.sizes.raw_inset_crops;
    spdlog::trace("setCrops: Exif crops:\n\tTop: {}\n\tLeft: {}\n\tWidth: {}\n\tHeigth: {}", crops->ctop, crops->cleft,
                  crops->cwidth, crops->cheight);

    unsigned short th_width  = processing->raw_data->imgdata.thumbnail.twidth;
    unsigned short th_height = processing->raw_data->imgdata.thumbnail.theight;

    unsigned short iheight = processing->raw_data->imgdata.sizes.iheight;
    unsigned short iwidth  = processing->raw_data->imgdata.sizes.iwidth;

    unsigned short im_width  = processing->raw_data->imgdata.sizes.width;
    unsigned short im_height = processing->raw_data->imgdata.sizes.height;

    unsigned short raw_width  = processing->raw_data->imgdata.sizes.raw_width;
    unsigned short raw_height = processing->raw_data->imgdata.sizes.raw_height;

    spdlog::trace("Thumbnail width: {}, Thumbnail heigth: {}", th_width, th_height);

    spdlog::trace("Image iwidth: {}, Image iheigth: {}", iwidth, iheight);

    spdlog::trace("setCrops: Orientation: {}", processing->raw_data->imgdata.sizes.flip);

    int r_width  = im_width != 0 ? im_width : raw_width;
    int r_height = im_height != 0 ? im_height : raw_height;
    int r_left   = 0;
    int r_top    = 0;

    int m_cwidth  = r_width;
    int m_cheight = r_height;
    int m_cleft   = 0;
    int m_ctop    = 0;

    processing->m_crops.width  = m_cwidth;
    processing->m_crops.height = m_cheight;
    processing->m_crops.left   = 0;
    processing->m_crops.top    = 0;

    bool cw_ok = crops->cwidth > 0;
    bool ch_ok = crops->cheight > 0;

    cw_ok &= crops->cwidth <= processing->raw_data->imgdata.sizes.width;
    ch_ok &= crops->cheight <= processing->raw_data->imgdata.sizes.height;

    bool cl_ok = crops->cleft + crops->cwidth <= processing->raw_data->imgdata.sizes.width;
    bool ct_ok = crops->ctop + crops->cheight <= processing->raw_data->imgdata.sizes.height;

    bool ok = cw_ok && ch_ok && cl_ok && ct_ok;

    if (!ok) {
        spdlog::debug("SetCrop: Invalid crop values");
        if (cw_ok && ch_ok) {  // if crop width and height are valid
            m_cleft   = (r_width - crops->cwidth) / 2;
            m_ctop    = (r_height - crops->cheight) / 2;
            m_cwidth  = crops->cwidth;
            m_cheight = crops->cheight;
            ok        = true;
        } else if (th_width > 0 && th_height > 0) {  // if thumbnail width and height are valid
            if (r_width / float(th_width) < 1.25f && r_height / float(th_height) < 1.25f) {
                if (processing->raw_data->imgdata.sizes.flip == 0 || processing->raw_data->imgdata.sizes.flip == 3) {
                    spdlog::debug("SetCrop: Thumbnail is full-size");
                    m_cleft   = (r_width - th_width) / 2;
                    m_ctop    = (r_height - th_height) / 2;
                    m_cwidth  = th_width;
                    m_cheight = th_height;
                    ok        = true;
                } else if (processing->raw_data->imgdata.sizes.flip == 5
                           || processing->raw_data->imgdata.sizes.flip == 6) {
                    spdlog::debug("SetCrop: Thumbnail is full-size");
                    m_cleft   = (r_width - th_width) / 2;
                    m_ctop    = (r_height - th_height) / 2;
                    m_cwidth  = th_width;
                    m_cheight = th_height;
                    ok        = true;
                }
            }
        } else {
            spdlog::warn("SetCrop: No valid crop values");
            no_error = false;
        }
    } else {
        m_cwidth  = crops->cwidth;
        m_cheight = crops->cheight;
        m_cleft   = crops->cleft > 0 ? crops->cleft : (im_width - crops->cwidth) / 2;
        m_ctop    = crops->ctop > 0 ? crops->ctop : (im_height - crops->cheight) / 2;
        no_error  = true;
    };

    if (settings.crop_mode != -1) {
        spdlog::debug("setCrops: Crop valid");
        switch (processing->raw_data->imgdata.sizes.flip) {
        case 0:  // Unrotated/Horisontal
            spdlog::trace("setCrops: Unrotated/Horisontal");
            processing->m_crops.left   = m_cleft;
            processing->m_crops.top    = m_ctop;
            processing->m_crops.width  = m_cwidth;
            processing->m_crops.height = m_cheight;
            break;
        case 3:  // 180 Horisontal
            spdlog::trace("setCrops: 180 Horisontal");
            processing->m_crops.left   = processing->raw_data->imgdata.sizes.width - m_cleft - m_cwidth;
            processing->m_crops.top    = processing->raw_data->imgdata.sizes.height - m_ctop - m_cheight;
            processing->m_crops.width  = m_cwidth;
            processing->m_crops.height = m_cheight;
            break;
        case 5:  // 90 CCW Vertical
            spdlog::trace("setCrops: 90 CCW Vertical");
            processing->m_crops.left   = m_ctop;
            processing->m_crops.top    = processing->raw_data->imgdata.sizes.width - m_cleft - m_cwidth;
            processing->m_crops.width  = m_cheight;
            processing->m_crops.height = m_cwidth;
            break;
        case 6:  // 90 CW Vertical
            spdlog::trace("setCrops: 90 CW Vertical");
            processing->m_crops.left   = processing->raw_data->imgdata.sizes.height - m_ctop - m_cheight;
            processing->m_crops.top    = m_cleft;
            processing->m_crops.width  = m_cheight;
            processing->m_crops.height = m_cwidth;
            break;
        default:
            spdlog::error("setCrops: Unsupported orientation");
            no_error = false;
            break;
        }
    }
    spdlog::debug("setCrops: crops {}", no_error ? "initialized correctly" : "initialization failed");
    spdlog::trace("setCrops: Crops:\n\tTop: {}\n\tLeft: {}\n\tWidth: {}\n\tHeigth: {}", processing->m_crops.top,
                  processing->m_crops.left, processing->m_crops.width, processing->m_crops.height);
    return no_error;
}

void
Sorter(int index, std::string fileName, std::unique_ptr<ProcessingParams>& processing_entry,
       std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools,
       StepProgress* stepProgress)
{
    processing_entry.reset(new ProcessingParams());
    auto& processing        = processing_entry;
    processing->fileIndex   = index;
    processing->srcFile     = fileName;
    processing->progressTracker = stepProgress;

    std::string prest_sfx                              = "";
    auto [path, parentFolderName, baseName, extension] = splitPath(fileName);

    std::optional<std::string> lut_preset = getPresetfromName(parentFolderName + "/" + baseName, &settings);
    if (lut_preset.has_value()) {
        prest_sfx = lut_preset.value();
    } else {
        spdlog::debug("PRE: No suitable LUT preset was found from file name");
    }

    if (settings.lutMode > 0) {
        auto lut_preset_it = settings.lut_Preset.find(settings.dLutPreset);
        if (lut_preset_it != settings.lut_Preset.end()) {
            lut_preset = lut_preset_it->first;
            prest_sfx  = lut_preset.value();
        } else {
            spdlog::error("PRE: LUT preset {} not found", settings.dLutPreset);
        }
    } else if (settings.lutMode == 0 && lut_preset.has_value()) {
        prest_sfx = lut_preset.value();
    }

    // Count expected processing steps for this file
    auto stepInfo = countExpectedSteps(settings, lut_preset);
    processing->initializeSteps(stepInfo.stepCount);

    spdlog::debug("PRE: File {} will have {} steps (Demosaic:{} LUT:{} Sharp:{})",
                  fileName, stepInfo.stepCount,
                  stepInfo.hasDemosaic ? "Y" : "N",
                  stepInfo.hasLUT ? "Y" : "N",
                  stepInfo.hasSharp ? "Y" : "N");

    auto [outPath, outName, outExt] = getOutName(path, baseName, extension, prest_sfx, &settings);

    auto [exist, path_idx] = outpaths.try_add(outPath);
    if (!exist) {
        spdlog::debug("PRE: New output path added: {}", outPath);
    }
    processing->outPathIdx = path_idx;
    processing->outFile    = outName;
    processing->outExt     = outExt;
    processing->lut_preset = lut_preset.value_or("");
    spdlog::debug("PRE: Preprocessing file {} > {}/{}{}", processing->srcFile, outpaths.get_path(path_idx),
                  processing->outFile, processing->outExt);

    processing->setStatus(ProcessingStatus::Prepared);
    (*myPools)["rawReader"]->enqueue(rawReader, index, std::ref(processing_entry), fileCntr, myPools);
}

// LibRaw buffer reader
void
Reader(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
       std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;

    spdlog::info("Reader: file {}", processing->srcFile);

    fs::path p(processing->srcFile);
    if (fs::is_symlink(p)) {
        try {
            std::string symLinkTarget = fs::read_symlink(p).string();
            spdlog::debug("Reader: File is a symlink to: {}", symLinkTarget);
            processing->srcFile = symLinkTarget;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Reader: Could not read symlink: {}", e.what());
        }
    }

    std::ifstream file(processing->srcFile, std::ios::binary | std::ios::ate);
    if (!file)
        spdlog::error("Reader: Could not open file: {}", processing->srcFile);

    const auto fileSize = file.tellg();
    if (fileSize < 0)
        spdlog::error("Reader: Could not determine size of file: {}", processing->srcFile);
    file.seekg(0);

    spdlog::debug("Reader: File size: {}", static_cast<long long>(fileSize));

    std::vector<char> raw_buffer(fileSize);

    if (!file.read(raw_buffer.data(), fileSize)) {
        throw std::runtime_error("Reader: Could not read file: " + processing->srcFile);
    }

    std::unique_ptr<std::vector<char>> raw_buffer_ptr = std::make_unique<std::vector<char>>(raw_buffer);

    processing->setStatus(ProcessingStatus::Loaded);
    file.close();

    (*fileCntr)--;
    (*myPools)["unpacker"]->enqueue(Unpacker, index, std::ref(processing_entry), std::ref(raw_buffer_ptr), fileCntr,
                                    myPools);
}

// Libraw disk reader
void
rawReader(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
          std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;

    fs::path p(processing->srcFile);
    if (fs::is_symlink(p)) {
        try {
            std::string symLinkTarget = fs::read_symlink(p).string();
            spdlog::debug("Reader: File is a symlink to: {}", symLinkTarget);
            processing->srcFile = symLinkTarget;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Reader: Could not read symlink: {}", e.what());
        }
    }

    processing->raw_data = std::make_unique<LibRaw>();
    LibRaw* raw          = processing->raw_data.get();

    spdlog::info("Libraw Reader: file {}", processing->srcFile);

    int ret = raw->open_file(processing->srcFile.c_str());
    if (ret != LIBRAW_SUCCESS) {
        spdlog::error("Reader: Cannot read file: {}", processing->srcFile);
        return;
    }

    spdlog::trace("Reader: Model: {}", processing->raw_data->imgdata.idata.model);
    spdlog::trace("Reader: Make: {}", processing->raw_data->imgdata.idata.make);
    processing->m_exif.make  = processing->raw_data->imgdata.idata.make;
    processing->m_exif.model = processing->raw_data->imgdata.idata.model;

    (*fileCntr)--;
    (*myPools)["LUnpacker"]->enqueue(LUnpacker, index, std::ref(processing_entry), fileCntr, myPools);
}

// Libraw disk unpacker
void
LUnpacker(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
          std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;
    spdlog::info("Unpack: file {}", processing->srcFile);

    auto& raw = processing->raw_data;

    raw->imgdata.params.use_camera_wb     = settings.rawParms.use_camera_wb;
    raw->imgdata.params.use_camera_matrix = settings.rawParms.use_camera_matrix;
    raw->imgdata.params.highlight         = settings.rawParms.highlight;
    raw->imgdata.params.aber[0]           = settings.rawParms.aber[0];
    raw->imgdata.params.aber[1]           = settings.rawParms.aber[1];
    raw->imgdata.params.half_size         = settings.rawParms.half_size;

    std::fill(std::begin(raw->imgdata.params.gamm), std::end(raw->imgdata.params.gamm), 1.0);

    raw->imgdata.params.output_color = settings.rawSpace;

    raw->imgdata.params.user_flip = settings.rawRot;

    spdlog::trace("Unpack: CameraRaw Rotations: {}", settings.rawRot);

    if (settings.denoise_mode == 1 || settings.denoise_mode == 3) {
        raw->imgdata.params.threshold = settings.rawParms.denoise_thr;
    } else {
        raw->imgdata.params.threshold = 0.0f;
    }

    if (settings.denoise_mode == 2 || settings.denoise_mode == 3) {
        raw->imgdata.params.fbdd_noiserd = settings.rawParms.fbdd_noiserd;
    } else {
        raw->imgdata.params.fbdd_noiserd = 0;
    }

    int ret = raw->unpack();
    if (ret != LIBRAW_SUCCESS) {
        spdlog::error("Unpack: Cannot unpack data from file: {}", processing->srcFile);
        return;
    }


    if (settings.crop_mode != -1) {
        if (raw->imgdata.sizes.raw_inset_crops->cwidth != 0 && raw->imgdata.sizes.raw_inset_crops->cheight != 0) {
            if (raw->imgdata.sizes.raw_inset_crops->cwidth <= raw->imgdata.rawdata.sizes.raw_width
                && raw->imgdata.sizes.raw_inset_crops->cheight <= raw->imgdata.rawdata.sizes.raw_height) {
                raw->imgdata.sizes.width  = raw->imgdata.sizes.raw_inset_crops->cwidth;
                raw->imgdata.sizes.height = raw->imgdata.sizes.raw_inset_crops->cheight;

                raw->imgdata.rawdata.sizes.width  = raw->imgdata.sizes.raw_inset_crops->cwidth;
                raw->imgdata.rawdata.sizes.height = raw->imgdata.sizes.raw_inset_crops->cheight;
            }
        }
    };

    ret = raw->adjust_sizes_info_only();
    if (ret != LIBRAW_SUCCESS) {
        spdlog::error("Unpack: Cannot adjust sizes info: {}", processing->srcFile);
        return;
    }

    if (!setCrops(index, processing_entry)) {
        spdlog::error("Unpack: Cannot set crops for file: {}", processing->srcFile);
    }

    processing->setStatus(ProcessingStatus::Unpacked);

    if (settings.dDemosaic > -2) {
        (*fileCntr)--;
        (*myPools)["demosaic"]->enqueue(Demosaic, index, std::ref(processing_entry), fileCntr, myPools);
    } else {
        (*fileCntr) -= 4;  // can skip the writer
        (*myPools)["writer"]->enqueue(Writer, index, std::ref(processing_entry), fileCntr, myPools);
    }
}

// Libraw buffer unpacker
void
Unpacker(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::unique_ptr<std::vector<char>>& raw_buffer,
         std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;
    spdlog::info("Unpack: file {}", processing->srcFile);

    processing->raw_data = std::make_unique<LibRaw>();
    LibRaw* raw          = processing->raw_data.get();

    raw->imgdata.params.use_camera_matrix = settings.rawParms.use_camera_matrix;
    raw->imgdata.params.highlight         = settings.rawParms.highlight;
    raw->imgdata.params.aber[0]           = settings.rawParms.aber[0];
    raw->imgdata.params.aber[1]           = settings.rawParms.aber[1];
    raw->imgdata.params.half_size         = settings.rawParms.half_size;

    raw->imgdata.params.output_color = settings.rawSpace;

    raw->imgdata.params.user_flip = settings.rawRot;

    if (settings.denoise_mode == 1 || settings.denoise_mode == 3) {
        raw->imgdata.params.threshold = settings.rawParms.denoise_thr;
    } else {
        raw->imgdata.params.threshold = 0.0f;
    }

    if (settings.denoise_mode == 2 || settings.denoise_mode == 3) {
        raw->imgdata.params.fbdd_noiserd = settings.rawParms.fbdd_noiserd;
    } else {
        raw->imgdata.params.fbdd_noiserd = 0;
    }

    int ret = raw->open_buffer(raw_buffer->data(), raw_buffer->size());
    if (ret != LIBRAW_SUCCESS) {
        spdlog::error("Unpack: Cannot read buffer: {}", processing->srcFile);
        return;
    }

    ret = raw->unpack();
    if (ret != LIBRAW_SUCCESS) {
        spdlog::error("Unpack: Cannot unpack data from file: {}", processing->srcFile);
        return;
    }

    processing->setStatus(ProcessingStatus::Unpacked);

    (*fileCntr)--;

    if (settings.dDemosaic > -2) {
        (*myPools)["demosaic"]->enqueue(Demosaic, index, std::ref(processing_entry), fileCntr, myPools);
    } else {
        (*fileCntr)--;
        (*fileCntr)--;
        (*myPools)["processor"]->enqueue(Writer, index, std::ref(processing_entry), fileCntr, myPools);
    }
}

void
Demosaic(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
         std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;
    auto& raw        = processing->raw_data;
    spdlog::info("Demosaic: file {}", processing->srcFile);

    auto& raw_parms = raw->imgdata.params;

    if (settings.dDemosaic == -1) {
        raw_parms.output_bps       = 16;
        raw_parms.user_qual        = settings.dDemosaic;
        raw_parms.no_interpolation = 1;

        if (raw->dcraw_process() != LIBRAW_SUCCESS) {
            spdlog::error("Demosaic: Cannot process data from file: {}", processing->srcFile);
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr) -= 3;
        (*myPools)["writer"]->enqueue(Writer, index, std::ref(processing_entry), fileCntr, myPools);
    } else if (settings.dDemosaic > -1) {
        raw_parms.output_bps = 16;
        raw_parms.user_qual  = settings.dDemosaic;

        if (raw->dcraw_process() != LIBRAW_SUCCESS) {
            spdlog::error("Demosaic: Cannot process data from file: {}", processing->srcFile);
            return;
        }
        processing->setStatus(ProcessingStatus::Demosaiced);

        (*fileCntr)--;
        (*myPools)["dcraw"]->enqueue(Dcraw, index, std::ref(processing_entry), fileCntr, myPools);
    } else {
        spdlog::error("Demosaic: Unknown demosaic mode");
        return;
    }
}

void
Dcraw(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
      std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;

    auto& raw = processing->raw_data;

    spdlog::debug("Dcraw: Processing data from file: {}", processing->srcFile);

    auto& raw_parms      = raw->imgdata.params;
    raw_parms.output_bps = 16;

    processing->raw_image = raw->dcraw_make_mem_image();

    if (!processing->raw_image) {
        spdlog::error("Dcraw: Cannot process data from file: {}", processing->srcFile);
        return;
    }

    (*fileCntr)--;
    (*myPools)["processor"]->enqueue(Processor, index, std::ref(processing_entry), fileCntr, myPools);
}

void
Processor(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
          std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing             = processing_entry;
    std::unique_ptr<LibRaw>& raw = processing->raw_data;

    spdlog::debug("Processor: Processing data from file: {}", processing->srcFile);

    libraw_processed_image_t* image = processing->raw_image;

    spdlog::trace("Processor: RAW Image buffer: {}", reinterpret_cast<uintptr_t>(&image->data));

    OIIO::ImageSpec image_spec(image->width, image->height, image->colors, OIIO::TypeDesc::UINT16);
    OIIO::ImageBuf image_buf(image_spec, image->data);

    EXIF::get_exif(processing->raw_data, image_spec);

    TypeDesc out_format = getTypeDesc(settings.bitDepth != -1 ? settings.bitDepth : settings.defBDepth);

    spdlog::debug("Processor: Output format: {}", formatText(out_format));

    ImageSpec processing_spec = image_spec;

    processing_spec.set_format(out_format);

    ImageBuf lut_buf(processing_spec);
    ImageBuf uns_buf(processing_spec);
    ImageBuf* lut_buf_ptr = &lut_buf;
    ImageBuf* uns_buf_ptr = &uns_buf;
    // LUT Transform
    bool lutValid = false;
    if (processing->lut_preset != "") {
        lutValid = true;
    }
    spdlog::trace("Processor: Input image: {}x{}x{}", image_buf.spec().width, image_buf.spec().height,
                  image_buf.spec().nchannels);
    spdlog::trace("Processor: Input image buffer: {}", reinterpret_cast<uintptr_t>(image_buf.localpixels()));
    spdlog::trace("LUT: Input Image buffer: {}", reinterpret_cast<uintptr_t>(image_buf.localpixels()));

    if (settings.lutMode >= 0 && lutValid) {
        fs::path lutPreset = settings.lut_Preset.at(processing->lut_preset);

        if (settings.perCamera) {
            std::string lut_ext  = lutPreset.extension().string();
            std::string lut_file = lutPreset.stem().string();
            fs::path lut_dir     = lutPreset.parent_path();

            fs::path new_lut = lut_dir
                               / (lut_file + "_" + processing->m_exif.make + "_" + processing->m_exif.model + lut_ext);
            lutPreset = new_lut;
        }

        if (ImageBufAlgo::ociofiletransform(*lut_buf_ptr, image_buf, lutPreset.string(), false, false,
                                            procGlobals.ocio_conf_ptr.get())) {
            spdlog::info("LUT preset {} <{}> applied", processing->lut_preset, lutPreset.string());
            processing_entry->setStatus(ProcessingStatus::Graded);
            image_buf.reset();
            if (!processing_entry->rawCleared) {
                processing_entry->raw_data->dcraw_clear_mem(image);
                processing_entry->rawCleared = true;
            }
        } else {
            spdlog::error("LUT not applied: {}", lut_buf.geterror());
            lut_buf_ptr = &image_buf;
        }
    } else {
        spdlog::debug("LUT transformation disabled");
        lut_buf_ptr = &image_buf;
    }

    (*fileCntr)--;
    spdlog::trace("LUT: Out Image buffer: {}", reinterpret_cast<uintptr_t>(lut_buf_ptr->localpixels()));
    // Apply unsharp mask

    if (settings.sharp_mode != -1) {
        string_view kernel = settings.sharp_kerns[settings.sharp_kernel];
        float width        = settings.sharp_width;
        float contrast     = settings.sharp_contrast;
        float threshold    = settings.sharp_tresh;
        if (ImageBufAlgo::unsharp_mask(*uns_buf_ptr, *lut_buf_ptr, kernel, width, contrast, threshold)) {
            spdlog::debug("Unsharp mask applied: <{}>", kernel.c_str());
            spdlog::trace("Unsharp: Out Image buffer: {}", reinterpret_cast<uintptr_t>(uns_buf_ptr->localpixels()));
            spdlog::debug("Unsharp: kernel: {} width: {} contrast: {} threshold: {}", kernel.data(), width, contrast,
                          threshold);
            processing_entry->setStatus(ProcessingStatus::Unsharped);
            lut_buf_ptr->reset();
            if (!processing_entry->rawCleared) {
                processing_entry->raw_data->dcraw_clear_mem(image);
                processing_entry->rawCleared = true;
            }
        } else {
            spdlog::error("Unsharp mask not applied: {}", uns_buf.geterror());
            uns_buf_ptr = lut_buf_ptr;
        }
    } else {
        spdlog::debug("Unsharp mask disabled");
        uns_buf_ptr = lut_buf_ptr;
    }

    // copy for saving
    ImageBuf* out_buf_ptr = uns_buf_ptr;
    if (settings.lutMode == -1 && settings.sharp_mode == -1 && image_buf.spec().format != out_format) {
        spdlog::debug("Processor: Copying image buffer as format: {}", out_format.basetype);
        if (!ImageBufAlgo::copy(*out_buf_ptr, image_buf, out_format)) {
            spdlog::error("Processor: Cannot copy image buffer");
            spdlog::error("Processor: Cannot copy image buffer: {}", out_buf_ptr->geterror());
            return;
        }
    }

    spdlog::trace("Unsharp: Unsh Image buffer: {}", reinterpret_cast<uintptr_t>(uns_buf_ptr->localpixels()));
    spdlog::trace("Unsharp: Out Image buffer: {}", reinterpret_cast<uintptr_t>(out_buf_ptr->localpixels()));

    spdlog::trace("Processor: Result output image format: {}", out_buf_ptr->spec().format.c_str());

    processing->image   = std::make_unique<ImageBuf>(*out_buf_ptr);
    processing->outSpec = std::make_unique<OIIO::ImageSpec>(out_buf_ptr->spec());

    processing->setStatus(ProcessingStatus::Processed);

    (*fileCntr)--;

    (*myPools)["writer"]->enqueue(Writer, index, std::ref(processing_entry), fileCntr, myPools);
}

void
Writer(int index, std::unique_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
       std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing             = processing_entry;
    std::array<int, 4> crops     = { processing->m_crops.left, processing->m_crops.top, processing->m_crops.width,
                                     processing->m_crops.height };
    std::unique_ptr<LibRaw>& raw = processing->raw_data;

    spdlog::trace("Writer: RAW Image buffer: {}", reinterpret_cast<uintptr_t>(raw->imgdata.image));
    spdlog::trace("Writer: Inp Image buffer: {}", reinterpret_cast<uintptr_t>(processing->image->localpixels()));

    // Check if the output path exists and create it if not
    std::string outDir = outpaths.get_path(processing->outPathIdx);
    if (!outpaths.get_path_status(processing->outPathIdx)) {
        fs::path dir(outDir);
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }
        outpaths.set_path_status(processing->outPathIdx, true);
    }

    std::string outFilePath = outDir + "/" + processing->outFile + processing->outExt;

    if (!makePath(outDir)) {
        spdlog::error("Writer: Cannot create output directory: {}", outFilePath);
        return;
    };

    spdlog::info("Writer: Writing data to file: {}", outFilePath);
    if (settings.dDemosaic == -2) {
        // Write raw data to a file
        outFilePath = outDir + "/" + processing->outFile + ".ppm";
        std::ofstream output(outFilePath, std::ios::binary);
        if (!output) {
            spdlog::error("Writer: Cannot open output file: {}", outFilePath);
            return;
        }

        size_t pix_count      = raw->imgdata.sizes.raw_width * raw->imgdata.sizes.raw_height;
        size_t raw_image_size = pix_count * sizeof(ushort);

        // Write PGM header
        output << "P5\n";
        output << raw->imgdata.sizes.raw_width << " " << raw->imgdata.sizes.raw_height << "\n";
        output << "65535\n";  // Max value for 16-bit data

        // Write raw data with swapped byte order
        for (size_t i = 0; i < pix_count; ++i) {
            ushort value = raw->imgdata.rawdata.raw_image[i];
            value        = (value << 8) | (value >> 8);  // Swap bytes
            output.write(reinterpret_cast<char*>(&value), sizeof(ushort));
        }

        output.close();
    } else if (settings.dDemosaic == -1)  // writing color ppm/tiff using dcraw_ppm_tiff_writer
    {
        if (settings.fileFormat == -1) {
            if (settings.defFormat == 0) {
                raw->imgdata.params.output_tiff = 1;  // TIF
            } else if (settings.defFormat == 7) {
                raw->imgdata.params.output_tiff = 0;  // PPM
            } else {
                spdlog::error("Writer: Unknown file format. Format changed to *.tif");
                outFilePath = outDir + "/" + processing->outFile + ".tif";
            }
        }

        int ret = raw->dcraw_ppm_tiff_writer(outFilePath.c_str());
        if (ret != LIBRAW_SUCCESS) {
            spdlog::error("Writer: Cannot write image to file: {}", outFilePath);
            processing->raw_data.reset();
            return;
        }
    } else {  // Write processed image using oiio
        spdlog::trace("Writer: Inp Image buffer: {}", reinterpret_cast<uintptr_t>(processing->image->localpixels()));
        bool write_ok = img_write(processing->image, processing->outSpec, outFilePath, crops);
        if (!write_ok) {
            spdlog::error("Writer: Error writing: {}", outFilePath);
            return;
        }

        if (!processing->rawCleared) {
            processing->raw_data->dcraw_clear_mem(processing->raw_image);
            processing->rawCleared = true;
        }

        processing->image->reset();
        processing->image.reset();

        processing->outSpec.reset();
        processing->srcSpec.reset();
    }

    processing->setStatus(ProcessingStatus::Written);
    spdlog::debug("Writer: Finished writing data to file: {}", outFilePath);

    const auto preview_enqueue = procGlobals.previewSink.enqueue.load(std::memory_order_acquire);
    if (preview_enqueue != nullptr) {
        spdlog::trace("Preview: notify written '{}'", outFilePath);
        int total_files = 0;
        if (processing->progressTracker != nullptr) {
            total_files = static_cast<int>(processing->progressTracker->totalFiles);
        }
        void* preview_user = procGlobals.previewSink.user.load(std::memory_order_acquire);
        preview_enqueue(preview_user, outFilePath.c_str(), index + 1, total_files);
    }

    processing->raw_data.reset();
    processing->raw_image = nullptr;
    processing.reset();

    (*fileCntr) = 0;
}

void
Dummy(int index, std::shared_ptr<ProcessingParams>& processing_entry, std::atomic_size_t* fileCntr,
      std::map<std::string, std::unique_ptr<ThreadPool>>* myPools)
{
    auto& processing = processing_entry;

    if (!processing->rawCleared) {
        processing->raw_data->dcraw_clear_mem(processing->raw_image);
        processing->rawCleared = true;
    }

    (*fileCntr)--;
}
