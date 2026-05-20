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

static int
clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int
half_ceil(int value)
{
    if (value <= 0) {
        return 0;
    }
    return (value + 1) / 2;
}

static std::array<int, 4>
normalize_write_crop(const ImageBuf& out_buf, const std::array<int, 4>& input_crop)
{
    const ImageSpec& spec = out_buf.spec();
    const int image_width = spec.width;
    const int image_height = spec.height;

    if (image_width <= 0 || image_height <= 0) {
        return { 0, 0, 0, 0 };
    }

    if (settings.crop_mode == -1) {
        return { 0, 0, image_width, image_height };
    }

    std::array<int, 4> crop = input_crop;
    if (crop[2] <= 0 || crop[3] <= 0) {
        spdlog::warn("Writer: invalid crop {}x{}; writing full image", crop[2], crop[3]);
        return { 0, 0, image_width, image_height };
    }

    const bool crop_outside_image = crop[0] < 0 || crop[1] < 0 || crop[0] >= image_width || crop[1] >= image_height
                                    || crop[0] + crop[2] > image_width || crop[1] + crop[3] > image_height;

    if (settings.rawParms.half_size && crop_outside_image) {
        std::array<int, 4> half_crop = { half_ceil(crop[0]), half_ceil(crop[1]), half_ceil(crop[2]),
                                         half_ceil(crop[3]) };
        const bool half_crop_fits = half_crop[0] >= 0 && half_crop[1] >= 0 && half_crop[0] < image_width
                                    && half_crop[1] < image_height && half_crop[0] + half_crop[2] <= image_width
                                    && half_crop[1] + half_crop[3] <= image_height;
        if (half_crop_fits) {
            spdlog::debug("Writer: scaled crop for half-size output: {} {} {} {}", half_crop[0], half_crop[1],
                          half_crop[2], half_crop[3]);
            return half_crop;
        }
        crop = half_crop;
    }

    const int left = clamp_int(crop[0], 0, image_width - 1);
    const int top = clamp_int(crop[1], 0, image_height - 1);
    const int right = clamp_int(crop[0] + crop[2], left + 1, image_width);
    const int bottom = clamp_int(crop[1] + crop[3], top + 1, image_height);

    std::array<int, 4> normalized = { left, top, right - left, bottom - top };
    if (normalized != input_crop) {
        spdlog::warn("Writer: crop {} {} {} {} adjusted to {} {} {} {} for image {}x{}", input_crop[0],
                     input_crop[1], input_crop[2], input_crop[3], normalized[0], normalized[1], normalized[2],
                     normalized[3], image_width, image_height);
    }

    return normalized;
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

    const std::array<int, 4> write_crop = normalize_write_crop(*out_buf, crops);
    if (write_crop[2] <= 0 || write_crop[3] <= 0) {
        spdlog::error("Writer: invalid output image size for {}", outputFileName);
        return false;
    }

    ImageSpec write_spec = *out_spec.get();

    if (settings.crop_mode != -1) {
        write_spec.width       = write_crop[2];
        write_spec.height      = write_crop[3];
        write_spec.full_width  = write_crop[2];
        write_spec.full_height = write_crop[3];
    }
    spdlog::info("Output file format: {}", formatText(write_spec.format));

    auto out = ImageOutput::create(outputFileName);

    if (!out) {
        spdlog::error("Could not create output file: {}", outputFileName);
        return false;
    }

    if (!out->open(outputFileName, write_spec, ImageOutput::Create)) {
        spdlog::error("Could not open output file {}: {}", outputFileName, out->geterror());
        return false;
    }

    spdlog::info("Writing {}", outputFileName);

    auto ou_px  = out_buf->pixeladdr(write_crop[0], write_crop[1], 0);
    auto ou_pst = out_buf->pixel_stride();
    auto ou_bst = out_buf->scanline_stride();
    auto ou_zst = out_buf->z_stride();

    spdlog::info("Writing image: {} Pixel address: {} Pixel stride: {} Scanline stride: {} Z stride: {}",
                 outputFileName, reinterpret_cast<uintptr_t>(ou_px), ou_pst, ou_bst, ou_zst);

    if (ou_px == nullptr) {
        spdlog::error("Writer: crop {} {} {} {} is outside image buffer for {}", write_crop[0], write_crop[1],
                      write_crop[2], write_crop[3], outputFileName);
        out->close();
        return false;
    }

    if (!out->write_image(write_spec.format, ou_px, ou_pst, ou_bst, ou_zst, m_progress_callback, nullptr)) {
        spdlog::error("Error writing {}: {}", outputFileName, out->geterror());
        out->close();
        return false;
    }
    out->close();

    return true;
}
