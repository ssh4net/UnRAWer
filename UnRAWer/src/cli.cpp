/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
 * Copyright (c) 2024 Erium Vladlen.
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


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <cctype>

#include "ui.h"
#include "cli.h"
#include "settings.h"
#include "process.h"

// Check if the string ends with the given suffix (case-insensitive)
bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

// Check if the string starts with the given prefix (case-insensitive)
bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), str.begin(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

// Check if the string is a batch file - ends with ".txt"
bool isBatchFile(const std::string& str) {
    const std::string suffix = ".txt";
    return endsWith(str, suffix);
}

// Read the batch file and return the list of URLs
std::vector<std::string> readBatchFile(const std::string& file) {
	std::vector<std::string> URLs;
	std::ifstream batchFile(file);
	if (!batchFile.is_open()) {
		std::cerr << "Error: Can not open the batch file [" << file << "]" << std::endl;
		return URLs;
	}

	std::string line;
	while (std::getline(batchFile, line)) {
		URLs.push_back(line);
	}

	return URLs;
}

int cli_main(int argc, char* argv[]) {
	int i = 1;
	int verbosity = 3;

    const std::string shortFlag = "-v=";
    const std::string longFlag = "-verbose=";
	if (std::string(argv[1]).find(shortFlag) != std::string::npos || std::string(argv[1]).find(longFlag) != std::string::npos) {
		//verbosity = std::stoi(std::string(argv[1]).substr(std::string(argv[1]).find("=") + 1));
		//i++;
		if (std::string(argv[1]).find("=") != std::string::npos) {
			verbosity = std::stoi(std::string(argv[1]).substr(std::string(argv[1]).find("=") + 1));
			i++;
        }
        else {
			std::cerr << "Error: Invalid verbosity flag. Using default verbosity." << std::endl;
        }
	}
	std::cout << "Verbosity: " << verbosity << std::endl;
	Log_SetVerbosity(std::max(0, std::min(verbosity, 5)));
////////////////////////////////////////////////////

    const std::string configSuffix = ".toml";

    // Variables to store categorized arguments
    std::string configFile = "";
    std::vector<std::string> batchFiles;
	QList<QUrl> URLs;

    // Iterate through command-line arguments
    for (; i < argc; ++i) {
        char* arg = argv[i];

        if (endsWith(arg, configSuffix)) {
            if (configFile.empty()) {
                configFile = arg;
            }
            else {
                std::cerr << "Warning: Multiple config files provided. Using the first one: "
                    << configFile << std::endl;
            }
        }
        else if (isBatchFile(arg)) {
            batchFiles.push_back(arg);
        }
        else {
			URLs.append(QUrl::fromLocalFile(QString(argv[i])));
        }
    }

	// Output the categorized arguments
    std::cout << "Config File: ";
    if (!configFile.empty()) {
        std::cout << configFile << std::endl;
    }
    else {
        std::cout << "Use default settings" << std::endl;
        configFile = "unrw_config.toml";
    }

    std::cout << "Batch Files (" << batchFiles.size() << "):" << std::endl;
    for (const auto& file : batchFiles) {
        std::cout << "  " << file << std::endl;
		for (const auto& url : readBatchFile(file)) {
			URLs.append(QUrl::fromLocalFile(QString(url.c_str())));
		}
    }

	std::cout << "URLs/Paths (" << URLs.size() << "):" << std::endl;
	for (const auto& url : URLs) {
		std::cout << "  " << url.toLocalFile().toStdString() << std::endl;
	}

	// Load settings from the config file
	if (!loadSettings(settings, configFile)) {
		std::cerr << "Error: Can not load [" << configFile << "]. Using default settings." << std::endl;
		settings.reSettings();
	}

	if (verbosity > 2) {
		printSettings(settings);
	}

	Log_SetVerbosity(std::max(0, std::min(verbosity, 5)));

	//// construct URLs from command line arguments
	//for (; i < argc; i++) {
	//	URLs.append(QUrl::fromLocalFile(QString(argv[i])));
	//}
	QApplication app(argc, argv);

	DummyWindow dummyWindow;
	DummyProgressBar dummyProgressBar;

	bool ok = doProcessing(URLs, &dummyProgressBar, &dummyWindow);
	if (!ok) {
		std::cerr << "Error: No raw files found!" << std::endl;
		return 1;
	}
	return 0;
}

/////////////////////////////////////////////////////