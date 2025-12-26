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
#include "pch.h"

#include "imageio.h"
#include "settings.h"
#include "exif_parser.h"

bool
m_progress_callback(void* opaque_data, float portion_done)
{
    // Simplified progress reporting
    // In a GUI app, you'd update a shared state here
    // if (opaque_data) { ... }
    return (portion_done >= 1.f);
}

// settings.bitDepth to OIIO::TypeDesc
TypeDesc
getTypeDesc(int bit_depth)
{
    switch (bit_depth) {
    case 0: return OIIO::TypeDesc::UINT8;
    case 1: return OIIO::TypeDesc::UINT16;
    case 2: return OIIO::TypeDesc::UINT32;
    case 3: return OIIO::TypeDesc::UINT64;
    case 4: return OIIO::TypeDesc::HALF;
    case 5: return OIIO::TypeDesc::FLOAT;
    case 6: return OIIO::TypeDesc::DOUBLE;
    default: return OIIO::TypeDesc::UNKNOWN;
    }
}

std::string
formatText(TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::UINT8: return "8bit integer";
    case TypeDesc::UINT16: return "16bit integer";
    case TypeDesc::UINT32: return "32bit integer";
    case TypeDesc::UINT64: return "64bit integer";
    case TypeDesc::INT8: return "8bit integer (signed)";
    case TypeDesc::INT16: return "16bit integer (signed)";
    case TypeDesc::INT32: return "32bit integer (signed)";
    case TypeDesc::INT64: return "64bit integer (signed)";
    case TypeDesc::HALF: return "16bit (half) float";
    case TypeDesc::FLOAT: return "32bit float";
    case TypeDesc::DOUBLE: return "64bit (double) float";
    default: return "UNKNOWN bit depth";
    }
}

void
formatFromBuff(ImageBuf& buf)
{
    std::string format = buf.file_format_name();
    spdlog::info("Format: {}", format);
}

void
debugImageBufWrite(const ImageBuf& buf, const std::string& filename)
{
    bool write_ok = buf.write(filename);
    if (!write_ok) {
        spdlog::error("Error: Could not write to {}", filename);
        spdlog::error("Error: {}", buf.geterror());
    }
}

bool
thumb_load(ImageBuf& outBuf, const std::string inputFileName)
{
    LibRaw raw_processor;
    libraw_processed_image_t* thumb;
    if (LIBRAW_SUCCESS != raw_processor.open_file(inputFileName.c_str())) {
        spdlog::error("Cannot open file: {}", inputFileName);
        return false;
    }

    if (LIBRAW_SUCCESS != raw_processor.unpack_thumb()) {
        spdlog::error("Cannot unpack thumbnail");
        return false;
    }

    thumb = raw_processor.dcraw_make_mem_thumb();

    if (!thumb) {
        spdlog::error("Cannot create in-memory thumb representation");
        return false;
    }

    std::ofstream output_file(inputFileName + ".jpg", std::ios::binary);
    output_file.write(reinterpret_cast<char*>(thumb->data), thumb->data_size);
    output_file.close();

    ImageSpec& spec = outBuf.specmod();
    spec.width      = thumb->width;
    spec.height     = thumb->height;
    spec.nchannels  = thumb->colors;
    spec.set_format(TypeDesc::UINT8);
    outBuf.reset(spec, thumb->data);

    LibRaw::dcraw_clear_mem(thumb);

    return true;
}

std::pair<bool, std::pair<std::shared_ptr<ImageBuf>, TypeDesc>>
img_load(const std::string& inputFileName)
{
    TypeDesc out_format;

    spdlog::info("READ: {}", inputFileName);
    ImageSpec config;
    config["raw:user_flip"] = settings.rawRot;

    config["oiio:UnassociatedAlpha"] = 0;
    config["tiff:UnassociatedAlpha"] = 0;
    config["oiio:ColorSpace"]        = "Linear";

    config["raw:ColorSpace"] = settings.rawCspace[settings.rawSpace];
    config["raw:Demosaic"]   = settings.demosaic[settings.dDemosaic];

    config["raw:use_camera_wb"]     = (int)settings.rawParms.use_camera_wb;
    config["raw:use_camera_matrix"] = (int)settings.rawParms.use_camera_matrix;
    config["raw:HighlightMode"]     = settings.rawParms.highlight;
    config["raw:aber"]              = (settings.rawParms.aber[0], settings.rawParms.aber[1]);
    config["raw:Exposure"]          = 1.0f;

    ImageBuf outBuf(inputFileName, 0, 0, nullptr, &config, nullptr);

    if (!outBuf.init_spec(inputFileName, 0, 0)) {
        spdlog::error("READ: Error reading {}", inputFileName);
        spdlog::error("READ: {}", outBuf.geterror());
        return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN } };
    }

    TypeDesc orig_format = outBuf.spec().format;

    spdlog::info("READ: File bith depth: {}", formatText(orig_format));

    int last_channel = -1;

    ImageSpec& spec = outBuf.specmod();
    int nchannels   = spec.nchannels;

    spdlog::info("READ: CameraRaw Rotations: {}", settings.rawRot);

    TypeDesc o_format = spec.format;

    switch (o_format.basetype) {
    case TypeDesc::UINT8: o_format = TypeDesc::UINT16; break;
    case TypeDesc::HALF: o_format = TypeDesc::FLOAT; break;
    default: break;
    }

    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, m_progress_callback, nullptr);
    if (!read_ok) {
        spdlog::error("READ: Error! Could not read input image");
        return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN } };
    }

    // Get the image's spec
    const ImageSpec& ispec = outBuf.spec();

    // Get the format (bit depth and type)
    TypeDesc load_format = ispec.format;
    out_format           = load_format;  // copy latest buffer format as an output format

    spdlog::info("READ: File loaded bit depth: {}", formatText(load_format));

    // get the image size
    int width  = outBuf.spec().width;
    int height = outBuf.spec().height;
    spdlog::info("READ: Image size: {}x{}", width, height);
    spdlog::info("READ: Channels: {} Alpha channel index: {}", outBuf.nchannels(), outBuf.spec().alpha_channel);

    return { true, { std::make_shared<OIIO::ImageBuf>(outBuf), orig_format } };
}

bool
makePath(const std::string& out_path)
{
    namespace fs = std::filesystem;
    try {
        fs::path p(out_path);
        if (fs::exists(p)) {
            return true;
        }
        return fs::create_directories(p);
    } catch (const std::exception& e) {
        spdlog::error("Error creating path {}: {}", out_path, e.what());
        return false;
    }
}

bool
img_write(std::unique_ptr<ImageBuf>& out_buf, std::unique_ptr<ImageSpec>& out_spec, const std::string& outputFileName,
          std::array<int, 4> crops)
{
    out_spec->attribute("pnm:binary", 1);
    out_spec->attribute("pnm:pfmflip", 0);
    out_spec->attribute("oiio:UnassociatedAlpha", 1);
    out_spec->attribute("jpeg:subsampling", "4:4:4");
    out_spec->attribute("png:compressionLevel", 4);

    switch (settings.fileFormat) {
    case 0:  // TIFF
        out_spec->attribute("Compression", "zip");
        out_spec->attribute("tiff:zipquality", "9");
        break;
    case 1:  // OpenEXR
        out_spec->attribute("Compression", "piz");
        break;
    case 2:  // PNG
        out_spec->attribute("Compression", "zip");
        break;
    case 3:  // JPEG
        out_spec->attribute("Compression", "jpeg:" + std::to_string(settings.quality));
        break;
    case 4:                                                                                  // JPEG-2000
        out_spec->attribute("Compression", "jpeg2000:" + std::to_string(settings.quality));  // Not used
        break;
    case 5:  // JPEG-XL
        out_spec->attribute("Compression", "jpegxl:" + std::to_string(settings.quality));
        break;
    case 6:  // HEIC
        out_spec->attribute("Compression", "heic:" + std::to_string(settings.quality));
        break;
    default: out_spec->attribute("Compression", "zip"); break;
    }

    ImageSpec write_spec = *out_spec.get();

    if (settings.crop_mode != -1) {
        write_spec.width       = crops[2];  // .width;
        write_spec.height      = crops[3];  // .height;
        write_spec.full_width  = crops[2];  // .width;
        write_spec.full_height = crops[3];  // .height;
    }
    spdlog::info("Output file format: {}", formatText(write_spec.format));

    auto out = ImageOutput::create(outputFileName);

    if (!out) {
        spdlog::error("Could not create output file: {}", outputFileName);
        return false;
    }

    if (settings.crop_mode != -1) {
        out->open(outputFileName, write_spec, ImageOutput::Create);
    } else {
        out->open(outputFileName, *out_spec.get(), ImageOutput::Create);
    }

    spdlog::info("Writing {}", outputFileName);

    auto ou_px  = out_buf->pixeladdr(crops[0], crops[1], 0);
    auto ou_pst = out_buf->pixel_stride();
    auto ou_bst = out_buf->scanline_stride();
    auto ou_zst = out_buf->z_stride();

    spdlog::info("Writing image: {} Pixel address: {} Pixel stride: {} Scanline stride: {} Z stride: {}",
                 outputFileName, reinterpret_cast<uintptr_t>(ou_px), ou_pst, ou_bst, ou_zst);

    out->write_image(write_spec.format, ou_px, ou_pst, ou_bst, ou_zst, m_progress_callback, nullptr);
    out->close();

    return true;
}