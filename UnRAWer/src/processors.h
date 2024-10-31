/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
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
#ifndef PROCESSORS_H
#define PROCESSORS_H

#include "threadpool.h"
#include "fileProcessor.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/color.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>

bool isRaw(QString file, const std::unordered_set<std::string>& raw_ext_set);

void Sorter( int index, QString fileName, std::unique_ptr<ProcessingParams>& processing_entry,
	std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Reader(int index, std::unique_ptr<ProcessingParams>& processing_entry,
	        std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

//void oReader(int index, std::unique_ptr<ProcessingParams>& processing_entry,
//			 std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void rawReader(int index, std::unique_ptr<ProcessingParams>& processing_entry,
			 std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void LUnpacker(int index, std::unique_ptr<ProcessingParams>& processing_entry,
			   std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Unpacker(int index, std::unique_ptr<ProcessingParams>& processing_entry, 
			  std::unique_ptr<std::vector<char>>& raw_buffer_ptr, std::atomic_size_t* fileCntr,
			  std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Demosaic(int index, std::unique_ptr<ProcessingParams>& processing_entry,
			  std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Dcraw(int index, std::unique_ptr<ProcessingParams>& processing_entry,
	       std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Processor(int index, std::unique_ptr<ProcessingParams>& processing_entry,
			   std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

//void OProcessor(int index, std::unique_ptr<ProcessingParams>& processing_entry,
//	std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Writer(int index, std::unique_ptr<ProcessingParams>& processing_entry,
	        std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Dummy(int index, std::unique_ptr<ProcessingParams>& processing_entry,
		   std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

#endif // !PROCESSORS_H
