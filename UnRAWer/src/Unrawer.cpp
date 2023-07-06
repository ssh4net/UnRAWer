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

#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>

#include "Timer.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/color.h>

#include "Log.h"
#include "Unrawer.h"
#include "imageio.h"
#include "settings.h"
#include "processing.h"

using namespace OIIO;

std::pair<bool, std::shared_ptr<ImageBuf>> 
imgProcessor(ImageBuf& input_buf, ColorConfig* colorconfig, std::string* lut_preset, 
             QProgressBar* progressBar, MainWindow* mainWindow) 
{

    ImageBuf out_buf; // result_buf, rgba_buf, original_alpha, bit_alpha_buf;
    ImageBuf lut_buf;
    ImageBuf uns_buf;
    // LUT Transform
    bool lutValid = false;
    // check if lut_preset is not nullptr set lutValid to true
    if (*lut_preset != "") {
        lutValid = true;
    }

    if (settings.lutMode >= 0 && lutValid) {
        //auto lutPreset = settings.lut_Preset[settings.dLutPreset];
        if (ImageBufAlgo::ociofiletransform(lut_buf, input_buf, settings.lut_Preset[*lut_preset], false, false, colorconfig)) {
            LOG(info) << "LUT preset " << lut_preset << " <" << settings.lut_Preset[*lut_preset] << "> " << " applied" << std::endl;
        }
        else {
            LOG(error) << "LUT not applied: " << lut_buf.geterror() << std::endl;
            lut_buf = input_buf;
        }
    }
    else {
        lut_buf = input_buf;
    }

    // Apply denoise
    // Apply unsharp mask
    if (true) {
        if (ImageBufAlgo::unsharp_mask(uns_buf, lut_buf, "gaussian", 1.0f, 0.5f, 0.0f)) {
            LOG(info) << "Unsharp mask applied: <Gaussian>" << std::endl;
        }
        else {
            LOG(error) << "Unsharp mask not applied: " << uns_buf.geterror() << std::endl;
            uns_buf = lut_buf;
        }
    }
    else
    {
        uns_buf = lut_buf;
    }

    // temp copy for saving
    out_buf = uns_buf;

    return { true, std::make_shared<ImageBuf>(input_buf) };
}

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
