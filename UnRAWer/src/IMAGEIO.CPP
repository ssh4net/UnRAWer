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

#include "imageio.h"
#include "settings.h"
#include "Log.h"
#include "exif_parser.h"

int hue = 186;
//void pbar_color_rand(QProgressBar* progressBar) {
void pbar_color_rand(MainWindow* mainWindow) {
    hue = (hue + 45) % 360;
    int saturation = 250;  // Set saturation value
    int value = 205;       // Set value

    QColor color;
    color.setHsv(hue, saturation, value);

    //setPBarColor(progressBar, color.name());
    emit mainWindow->changeProgressBarColorSignal(color);
}

bool m_progress_callback(void* opaque_data, float portion_done) {
    // Cast the opaque_data back to a QProgressBar
    QProgressBar* progressBar = static_cast<QProgressBar*>(opaque_data);

    int value = static_cast<int>(portion_done * 100);

    // You need to use QMetaObject::invokeMethod when you are interacting with the GUI thread from a non-GUI thread
    // Qt::QueuedConnection ensures the change will be made when control returns to the event loop of the GUI thread
    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, value));

    return (portion_done >= 1.f);
}

// settings.bitDepth to OIIO::TypeDesc
TypeDesc getTypeDesc(int bit_depth) {
    switch (bit_depth) {
    /*
    UNKNOWN,            ///< unknown type
    NONE,               ///< void/no type
    UINT8,              ///< 8-bit unsigned int values ranging from 0..255,
                        ///<   (C/C++ `unsigned char`).
    UCHAR=UINT8,
    INT8,               ///< 8-bit int values ranging from -128..127,
                        ///<   (C/C++ `char`).
    CHAR=INT8,
    UINT16,             ///< 16-bit int values ranging from 0..65535,
                        ///<   (C/C++ `unsigned short`).
    USHORT=UINT16,
    INT16,              ///< 16-bit int values ranging from -32768..32767,
                        ///<   (C/C++ `short`).
    SHORT=INT16,
    UINT32,             ///< 32-bit unsigned int values (C/C++ `unsigned int`).
    UINT=UINT32,
    INT32,              ///< signed 32-bit int values (C/C++ `int`).
    INT=INT32,
    UINT64,             ///< 64-bit unsigned int values (C/C++
                        ///<   `unsigned long long` on most architectures).
    ULONGLONG=UINT64,
    INT64,              ///< signed 64-bit int values (C/C++ `long long`
                        ///<   on most architectures).
    LONGLONG=INT64,
    HALF,               ///< 16-bit IEEE floating point values (OpenEXR `half`).
    FLOAT,              ///< 32-bit IEEE floating point values, (C/C++ `float`).
    DOUBLE,             ///< 64-bit IEEE floating point values, (C/C++ `double`).
    STRING,             ///< Character string.
    PTR,                ///< A pointer value.
    LASTBASE
    */
    case 0:
		return OIIO::TypeDesc::UINT8;
    case 1:
        return OIIO::TypeDesc::UINT16;
    case 2:
        return OIIO::TypeDesc::UINT32;
    case 3:
        return OIIO::TypeDesc::UINT64;
	case 4:
	    return OIIO::TypeDesc::HALF;
	case 5:
		return OIIO::TypeDesc::FLOAT;
	case 6:
		return OIIO::TypeDesc::DOUBLE;
	default:
		return OIIO::TypeDesc::UNKNOWN;
	}
}

std::string formatText(TypeDesc format) {
    switch (format.basetype) {
    case TypeDesc::UINT8:
        return "8bit integer";
    case TypeDesc::UINT16:
        return "16bit integer";
    case TypeDesc::UINT32:
        return "32bit integer";
    case TypeDesc::UINT64:
        return "64bit integer";
    case TypeDesc::INT8:
        return "8bit integer (signed)";
    case TypeDesc::INT16:
        return "16bit integer (signed)";
    case TypeDesc::INT32:
        return "32bit integer (signed)";
    case TypeDesc::INT64:
        return "64bit integer (signed)";
    case TypeDesc::HALF:
        return "16bit (half) float";
    case TypeDesc::FLOAT:
        return "32bit float";
    case TypeDesc::DOUBLE:
        return "64bit (double) float";
    default:
        return "UNKNOWN bit depth";
    }
}

void formatFromBuff(ImageBuf& buf) {
	std::string format = buf.file_format_name();
    LOG(info) << "Format: " << format << std::endl;
}

void debugImageBufWrite(const ImageBuf& buf, const std::string& filename) {
	bool write_ok = buf.write(filename);
    if (!write_ok) {
		LOG(error) << "Error: Could not write to " << filename << std::endl;
		LOG(error) << "Error: " << buf.geterror() << std::endl;
	}
}

bool thumb_load(ImageBuf& outBuf, const std::string inputFileName, MainWindow* mainWindow) {

    LibRaw raw_processor;
    libraw_processed_image_t* thumb;
    if (LIBRAW_SUCCESS != raw_processor.open_file(inputFileName.c_str())) {
        LOG(error) << "Cannot open file: " << inputFileName << std::endl;
        return false;
    }

    if (LIBRAW_SUCCESS != raw_processor.unpack_thumb()) {
        LOG(error) << "Cannot unpack thumbnail" << std::endl;
        return false;
    }

    thumb = raw_processor.dcraw_make_mem_thumb();

    if (!thumb) {
        LOG(error) << "Cannot create in-memory thumb representation" << std::endl;
        return false;
    }

    std::ofstream output_file(inputFileName + ".jpg", std::ios::binary);
    output_file.write(reinterpret_cast<char*>(thumb->data), thumb->data_size);
    output_file.close();

    ImageSpec& spec = outBuf.specmod();
    spec.width = thumb->width;
    spec.height = thumb->height;
    spec.nchannels = thumb->colors;
    spec.set_format(TypeDesc::UINT8);
    outBuf.reset(spec, thumb->data);

    LibRaw::dcraw_clear_mem(thumb);
    
    return true;
}

std::pair<bool, std::pair<std::shared_ptr<ImageBuf>, TypeDesc>> 
img_load(const std::string& inputFileName, QProgressBar* progressBar, MainWindow* mainWindow) 
{
    TypeDesc out_format;

    LOG(info) << "READ: " << inputFileName << std::endl;
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
    config["raw:Demosaic"] = settings.demosaic[settings.dDemosaic]; // linear, VNG, PPG, AHD (default), DCB, AHD-Mod, AFD, VCD, Mixed, LMMSE, AMaZE, DHT, AAHD, none
    //
    config["raw:use_camera_wb"] = settings.rawParms.use_camera_wb;     // If 1, use libraw camera white balance adjustment.
    config["raw:use_camera_matrix"] = settings.rawParms.use_camera_matrix; // 0 = never, 1 (default) = only for DNG files, 3 = always.
    config["raw:HighlightMode"] = settings.rawParms.highlight;     // 0 = clip, 1 = unclip, 2 = blend, 3 = reconstruct
    config["raw:aber"] = (settings.rawParms.aber[0], settings.rawParms.aber[1]);   //The default (1,1) means to perform no correction. This is an overall spatial scale, sensible values will be very close to 1.0.
    config["raw:Exposure"] = 1.0f;       // Exposure correction in stops before demosaic. Default: 1.0f

    ImageBuf outBuf(inputFileName, 0, 0, nullptr, &config, nullptr);

    if (!outBuf.init_spec(inputFileName, 0, 0)) {
        LOG(error) << "READ: Error reading " << inputFileName << std::endl;
        LOG(error) << "READ: " << outBuf.geterror() << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN }};
    }

    TypeDesc orig_format = outBuf.spec().format;

    LOG(info) << "READ: File bith depth: " << formatText(orig_format) << std::endl;
///////////////////////////
    
    int last_channel = -1;

    ImageSpec& spec = outBuf.specmod();
    int nchannels = spec.nchannels;

//    outBuf.specmod().attribute("raw:user_flip", settings.rawRot);
//    outBuf.specmod().extra_attribs["raw:user_flip"] = settings.rawRot;

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

    // = TypeDesc::FLOAT;// getTypeDesc(settings.bitDepth);//TypeUnknown; //TypeDesc::FLOAT

    //ImageCache* imageCache = ImageCache::create(false);

    //ImageBuf thumbnail;
    //bool success = imageCache->get_thumbnail(ustring(inputFileName), thumbnail);

    //if (!success) {
    //    std::cerr << "Failed to get thumbnail: " << imageCache->geterror() << std::endl;
    //    return false;
    //}

    
    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, m_progress_callback, progressBar);
    if (!read_ok) {
        LOG(error) << "READ: Error! Could not read input image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return { false, { std::make_shared<OIIO::ImageBuf>(), TypeDesc::UNKNOWN } };
    }
    
#if 0
    debugImageBufWrite(outBuf, "d:/premul_Buf.tif");
    debugImageBufWrite(alphs_rgba, "d:/premul_alphs_rgba.tif");
#endif

    // Get the image's spec
    const ImageSpec& ispec = outBuf.spec();

    // Get the format (bit depth and type)
    TypeDesc load_format = ispec.format;
    out_format = load_format; // copy latest buffer format as an output format

    LOG(info) << "READ: File loaded bit depth: " << formatText(load_format) << std::endl;

    // get the image size
    int width = outBuf.spec().width;
    int height = outBuf.spec().height;
    LOG(info) << "READ: Image size: " << width << "x" << height << std::endl;
    LOG(info) << "READ: Channels: " << outBuf.nchannels() << " Alpha channel index: " << outBuf.spec().alpha_channel << std::endl;

    return { true, {std::make_shared<OIIO::ImageBuf>(outBuf), orig_format} };
}

bool makePath(const std::string& out_path) {
    return true;
}

bool img_write(std::shared_ptr<ImageBuf> out_buf, std::shared_ptr<ImageSpec> out_spec, const std::string& outputFileName,
               TypeDesc out_format, TypeDesc orig_format, QProgressBar* progressBar, MainWindow* mainWindow) {
    
    //ImageSpec& ospec = ;
    // out_buf->specmod();

    //rspec.attribute("oiio:BitsPerSample", bits);
    /*
    "scene_linear" : Color pixel values are known to be scene-linear and using facility-default color primaries as defined by the OpenColorIO configuration. Note that "linear" is treated as a synonym. (Note: when no color config is found, this are presumed to use sRGB/Rec709 color primaries when built against OpenColorIO 2.1 or earlier, or when no OCIO support is available, but is presumed to be ACEScg when built against OCIO 2.2 or higher and using its built-in config.)
    "lin_srgb" : Color pixel values are known to be linear and using sRGB/Rec709 color primaries.
    "sRGB" : Using standard sRGB response and primaries.
    "Rec709" : Using standard Rec709 response and primaries.
    "ACEScg" : ACEScg color space encoding.
    "AdobeRGB" : Adobe RGB color space.
    "KodakLog" : Kodak logarithmic color space.
    "GammaX.Y" : Color values have been gamma corrected (raised to the power ). The X.Y is the numeric value of the gamma exponent.
    arbitrary : The name of any color space known to OpenColorIO (if OCIO support is present).
    */
    // out_spec->attribute("oiio:ColorSpace", "lin_srgb");
    out_spec->attribute("pnm:binary", 1);
    out_spec->attribute("pnm:pfmflip", 0);
    out_spec->attribute("oiio:UnassociatedAlpha", 1);
    out_spec->attribute("jpeg:subsampling", "4:4:4");
    out_spec->attribute("png:compressionLevel", 4);

    switch (settings.fileFormat) {
    case 0: // TIFF
        out_spec->attribute("Compression", "zip");
        break;
    case 1: // OpenEXR
        out_spec->attribute("Compression", "piz");
        break;
    case 2: // PNG
        out_spec->attribute("Compression", "zip");
        break;
    case 3: // JPEG
        out_spec->attribute("Compression", "jpeg:" + std::to_string(settings.quality));
        break;
    case 4: // JPEG-2000
        out_spec->attribute("Compression", "jpeg2000:" + std::to_string(settings.quality)); // Not used
        break;
    case 5: // JPEG-XL
        out_spec->attribute("Compression", "jpegxl:" + std::to_string(settings.quality));
        break;
    case 6: // HEIC
        out_spec->attribute("Compression", "heic:" + std::to_string(settings.quality));
        break;
    default:
        out_spec->attribute("Compression", "zip");
        break;
    }

    //rspec.attribute("tiff:bigtiff", 1);
    //rspec.set_format(TypeDesc::FLOAT); // temporary
    //LOG(info) << "Channels: " << rspec.nchannels << " Alpha channel: " << rspec.alpha_channel << std::endl;
//
    //out_spec->format = getTypeDesc(settings.bitDepth);
    if (getTypeDesc(settings.bitDepth) == TypeDesc::UNKNOWN) {
        out_spec->set_format(orig_format);
    }
    else {
        out_spec->set_format(TypeDesc::UINT16); //getTypeDesc(settings.bitDepth));
    }

    LOG(info) << "Output file format: " << formatText(out_spec->format) << std::endl;

    auto out = ImageOutput::create(outputFileName);
    if (!out) {
        LOG(error) << "Could not create output file: " << outputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }
    out->open(outputFileName, *out_spec, ImageOutput::Create);

    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    LOG(info) << "Writing " << outputFileName << std::endl;

    auto ou_px = out_buf->localpixels();
    auto ou_pst = out_buf->pixel_stride();
    auto ou_bst = out_buf->scanline_stride();
    auto ou_zst = out_buf->z_stride();

    out->write_image(out_format, ou_px, ou_pst, ou_bst, ou_zst, *m_progress_callback, progressBar);
    out->close();
    return true;
}

//std::pair<bool, std::shared_ptr<LibRaw>>
//raw_read(const std::string srcFile){
//    LibRaw RawProcessor;
//
//    if (RawProcessor.open_file(srcFile.c_str()) != LIBRAW_SUCCESS) {
//        LOG(error) << "RAW: Cannot open file" << srcFile << std::endl;
//        return { false, std::make_shared<float>() };
//    }
//    return { true, std::make_shared<uint>(RawProcessor.imgdata.rawdata.raw_image) };
//}
