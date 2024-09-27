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

#include <string>
#include <math.h>

#include "Timer.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/color.h>

#include "Log.h"
#include "Unrawer.h"
//#include "imageio.h"
#include "settings.h"
//#include "processing.h"

using namespace OIIO;

/*
std::pair<bool, std::shared_ptr<ImageBuf>> 
imgProcessor(ImageBuf& input_buf, ColorConfig* colorconfig, std::string* c_lut_preset, 
             std::shared_ptr<ProcessingParams>& processing_entry, libraw_processed_image_t* raw_image,
             QProgressBar* progressBar, MainWindow* mainWindow) 
{

    ImageBuf out_buf; // result_buf, rgba_buf, original_alpha, bit_alpha_buf;
    ImageBuf lut_buf;
    ImageBuf uns_buf;
    ImageBuf* out_buf_ptr = &out_buf;
    ImageBuf* lut_buf_ptr = &lut_buf;
    ImageBuf* uns_buf_ptr = &uns_buf;
    // LUT Transform
    bool lutValid = false;
    // check if lut_preset is not nullptr set lutValid to true
    if (*c_lut_preset != "") {
        lutValid = true;
    }
    //auto test = input_buf.spec();
    //std::cout << test.width << " " << test.height << " " << test.nchannels << std::endl;
    LOG(trace) << "Input image: " << input_buf.spec().width << "x" << input_buf.spec().height << "x" << input_buf.spec().nchannels << std::endl;
    LOG(trace) << "Input image: " << input_buf.spec().format << std::endl;

    if (settings.lutMode >= 0 && lutValid) {
        auto lutPreset = settings.lut_Preset[settings.dLutPreset];
        if (ImageBufAlgo::ociofiletransform(*lut_buf_ptr, input_buf, lutPreset, false, false, colorconfig)) {
            LOG(info) << "LUT preset " << settings.dLutPreset << " <" << lutPreset << "> " << " applied" << std::endl;
            processing_entry->setStatus(ProcessingStatus::Graded);
            input_buf.clear();
            if (!processing_entry->rawCleared) {
                processing_entry->raw_data->dcraw_clear_mem(raw_image);
                processing_entry->rawCleared = true;
            }
        }
        else {
            LOG(error) << "LUT not applied: " << lut_buf.geterror() << std::endl;
            lut_buf_ptr = &input_buf;
        }
    }
    else {
        LOG(debug) << "LUT transformation disabled" << std::endl;
        lut_buf_ptr = &input_buf;
    }

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
            if (!processing_entry->rawCleared) {
                processing_entry->raw_data->dcraw_clear_mem(raw_image);
                processing_entry->rawCleared = true;
            }
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

    //if (!rawCleared) {
    //    processing_entry->raw_data->dcraw_clear_mem(raw_image);
    //    rawCleared = true;
    //}

    return { true, std::make_shared<ImageBuf>(*out_buf_ptr) };
}
/*
bool unrawer_main(const std::string& inputFileName, const std::string& outputFileName, 
    ColorConfig* colorconfig, std::string* lut_preset,
    QProgressBar* progressBar, MainWindow* mainWindow) 
{
    Timer g_timer;
//////////////////////////////////////////////////
/// Image loading
/// 
    auto [load_ok, load_result] = img_load(inputFileName, progressBar, mainWindow);
    //bool load_ok = thumb_load(input_buf, inputFileName, mainWindow);
    if (!load_ok) {
		LOG(error) << "Error reading " << inputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}
//////////////////////////////////////////////////
/// Image processing
/// 
    auto [process_ok, out_buf] = imgProcessor(*load_result.first, colorconfig, lut_preset, progressBar, mainWindow);
    if (!process_ok) {
        LOG(error) << "Error processing " << inputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }
//////////////////////////////////////////////////
/// Image saving
/// 
    bool write_ok = img_write(*out_buf, outputFileName, load_result.second, load_result.first->spec().format, progressBar, mainWindow);
    if (!write_ok) {
		LOG(error) << "Error writing " << outputFileName << std::endl;
		mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}
//////////////////////////////////////////////////

    LOG(info) << "File processing time : " << g_timer.nowText() << std::endl;
    pbar_color_rand(mainWindow);
    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    return true;
}
*/