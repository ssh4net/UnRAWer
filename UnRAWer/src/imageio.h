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

#include <iostream>
#include <iomanip>
#include <string>
//#include <math.h>
//
//#include "Timer.h"
#include "ui.h"
//
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
//#include <OpenImageIO/imagecache.h>
//#include <OpenImageIO/imagebufalgo.h>
//#include <OpenImageIO/imagebufalgo_util.h>
//#include <Imath/half.h>
//#include <OpenImageIO/half.h>
//#include <libraw/libraw.h>

using namespace OIIO;

//void pbar_color_rand(QProgressBar* progressBar);
void pbar_color_rand(MainWindow* mainWindow);
bool m_progress_callback(void* opaque_data, float portion_done);

TypeDesc getTypeDesc(int bit_depth);
std::string formatText(TypeDesc format);
void formatFromBuff(ImageBuf& buf);

//bool img_load(ImageBuf& outBuf, const std::string& inputFileName, QProgressBar* progressBar, MainWindow* mainWindow);
std::pair<bool, std::pair<std::shared_ptr<ImageBuf>, TypeDesc>>
img_load(const std::string& inputFileName, QProgressBar* progressBar, MainWindow* mainWindow);

//bool img_write(std::unique_ptr<ImageBuf>& out_buf, std::unique_ptr<ImageSpec>& out_spec, const std::string& outputFileName,
//  TypeDesc out_format, TypeDesc orig_format, QProgressBar* progressBar, MainWindow* mainWindow, std::array<int, 4> crops);
bool img_write(std::unique_ptr<ImageBuf>& out_buf, std::unique_ptr<ImageSpec>& out_spec, const std::string& outputFileName, QProgressBar* progressBar, MainWindow* mainWindow, std::array<int, 4> crops);

bool makePath(const std::string& out_path);

bool thumb_load(ImageBuf& outBuf, const std::string inputFileName, MainWindow* mainWindow);

void debugImageBufWrite(const ImageBuf& buf, const std::string& filename);