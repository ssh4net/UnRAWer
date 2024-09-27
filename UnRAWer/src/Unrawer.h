/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
 * Copyright (c) 2022 Erium Vladlen.
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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "ui.h"
#include "fileProcessor.h"
using namespace OIIO;

bool unrawer_main(const std::string& inputFileName, const std::string& outputFileName,
	ColorConfig* colorconfig, std::string* lut_preset,
	QProgressBar* progressBar, MainWindow* mainWindow);

//std::pair<bool, std::shared_ptr<ImageBuf>>
//imgProcessor(ImageBuf& input_buf, ColorConfig* colorconfig, std::string* lut_preset,
//	std::shared_ptr<ProcessingParams>& processing_entry, libraw_processed_image_t* raw_image,
//	QProgressBar* progressBar, MainWindow* mainWindow);