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
#ifndef PROCESSORS_H
#define PROCESSORS_H

#include "threadpool.h"
#include "fileProcessor.h"

#include <OpenImageIO/color.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>

bool isRaw(QString file, const std::unordered_set<std::string>& raw_ext_set);

void Sorter(int index, QString fileName, std::shared_ptr<ProcessingParams>& processing_entry, 
			std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Reader(int index, std::shared_ptr<ProcessingParams>& processing_entry,
	        std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Unpacker(int index, std::shared_ptr<ProcessingParams>& processing_entry, std::shared_ptr<std::vector<char>> raw_buffer_ptr,
		      std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Demosaic(int index, std::shared_ptr<ProcessingParams>& processing_entry,
			  std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Processor(int index, std::shared_ptr<ProcessingParams>& processing_entry,
			   std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

void Writer(int index, std::shared_ptr<ProcessingParams>& processing_entry,
	        std::atomic_size_t* fileCntr, std::map<std::string, std::unique_ptr<ThreadPool>>* myPools);

#endif // !PROCESSORS_H
