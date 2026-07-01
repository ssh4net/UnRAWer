#pragma once

#include "pathutils.h"
#include "settings.h"

#include <OpenImageIO/imageio.h>

#include <cctype>

inline std::string
encoderCompressionWithLevel(const char* codec, int level)
{
    return std::string(codec) + ":" + std::to_string(level);
}

inline std::string
encoderLowerExtension(const std::string& outputFileName)
{
    std::string ext = pathToUtf8(pathFromUtf8(outputFileName).extension());
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

inline const char*
encoderTiffCompression(int compression)
{
    switch (compression) {
    case TiffCompression_Lzw: return "lzw";
    case TiffCompression_PackBits: return "packbits";
    case TiffCompression_None: return "none";
    default: return "zip";
    }
}

inline const char*
encoderExrCompression(int compression)
{
    switch (compression) {
    case ExrCompression_Zips: return "zips";
    case ExrCompression_Piz: return "piz";
    case ExrCompression_Pxr24: return "pxr24";
    case ExrCompression_Rle: return "rle";
    case ExrCompression_B44: return "b44";
    case ExrCompression_B44A: return "b44a";
    case ExrCompression_Dwaa: return "dwaa";
    case ExrCompression_Dwab: return "dwab";
    case ExrCompression_Htj2k256: return "htj2k256";
    case ExrCompression_Htj2k32: return "htj2k32";
    case ExrCompression_None: return "none";
    default: return "zip";
    }
}

inline const char*
encoderPngCompression(int strategy)
{
    switch (strategy) {
    case PngCompression_Filtered: return "filtered";
    case PngCompression_Huffman: return "huffman";
    case PngCompression_Rle: return "rle";
    case PngCompression_Fixed: return "fixed";
    case PngCompression_Fast: return "pngfast";
    case PngCompression_None: return "none";
    default: return "default";
    }
}

inline const char*
encoderJpegSubsampling(int subsampling)
{
    switch (subsampling) {
    case JpegSubsampling_422: return "4:2:2";
    case JpegSubsampling_420: return "4:2:0";
    case JpegSubsampling_411: return "4:1:1";
    default: return "4:4:4";
    }
}

inline void
setEncoderCompression(OIIO::ImageSpec& spec, const std::string& compression)
{
    spec.attribute("Compression", compression.c_str());
}

inline void
applyEncoderSettings(OIIO::ImageSpec& spec, const std::string& outputFileName)
{
    const std::string ext = encoderLowerExtension(outputFileName);

    spec.attribute("pnm:binary", 1);
    spec.attribute("pnm:pfmflip", 0);
    spec.attribute("oiio:UnassociatedAlpha", 1);

    if (ext == ".tif" || ext == ".tiff") {
        const char* compression = encoderTiffCompression(settings.tiffCompression);
        if (settings.tiffCompression == TiffCompression_Zip) {
            setEncoderCompression(spec, encoderCompressionWithLevel(compression, settings.tiffZipLevel));
        } else {
            setEncoderCompression(spec, compression);
        }
    } else if (ext == ".exr" || ext == ".sxr" || ext == ".mxr") {
        const char* compression = encoderExrCompression(settings.exrCompression);
        if (settings.exrCompression == ExrCompression_Zip || settings.exrCompression == ExrCompression_Zips) {
            setEncoderCompression(spec, encoderCompressionWithLevel(compression, settings.exrZipLevel));
        } else if (settings.exrCompression == ExrCompression_Dwaa || settings.exrCompression == ExrCompression_Dwab) {
            setEncoderCompression(spec, encoderCompressionWithLevel(compression, settings.exrDwaLevel));
        } else {
            setEncoderCompression(spec, compression);
        }
    } else if (ext == ".png") {
        spec.attribute("png:compressionLevel", settings.pngCompressionLevel);
        setEncoderCompression(spec, encoderPngCompression(settings.pngStrategy));
    } else if (ext == ".jpg" || ext == ".jpeg" || ext == ".jpe" || ext == ".jfif") {
        spec.attribute("jpeg:subsampling", encoderJpegSubsampling(settings.jpegSubsampling));
        setEncoderCompression(spec, encoderCompressionWithLevel("jpeg", settings.jpegQuality));
    } else if (ext == ".jp2" || ext == ".j2k") {
        setEncoderCompression(spec, "jpeg2000");
    } else if (ext == ".jph" || ext == ".j2c") {
        setEncoderCompression(spec, "htj2k");
        if (settings.jpeg2000QStep > 0.0f) {
            spec.attribute("jph:qstep", settings.jpeg2000QStep);
        }
    } else if (ext == ".heic" || ext == ".heif" || ext == ".heics" || ext == ".hif") {
        setEncoderCompression(spec, encoderCompressionWithLevel("heic", settings.heicQuality));
    } else if (ext == ".avif") {
        setEncoderCompression(spec, encoderCompressionWithLevel("avif", settings.heicQuality));
    } else if (ext == ".jxl") {
        spec.attribute("jpegxl:effort", settings.jpegxlEffort);
        spec.attribute("jpegxl:speed", settings.jpegxlSpeed);
        setEncoderCompression(spec, encoderCompressionWithLevel("jpegxl", settings.jpegxlQuality));
    } else {
        setEncoderCompression(spec, "zip");
    }
}
