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
#include "pch.h"

#include <DbgHelp.h>

#include "settings.h"
#include "cli.h"
////////////////////////////////////////

LONG WINAPI CustomExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    // Generate a unique dump file name
    SYSTEMTIME st;
    GetSystemTime(&st);
    char dumpFileName[128];
    sprintf_s(dumpFileName, "crash_dump_%04d%02d%02d_%02d%02d%02d.dmp",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE dumpFile = CreateFileA(dumpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);

    if (dumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();  // Fix ambiguity
        dumpInfo.ExceptionPointers = exceptionInfo;
        dumpInfo.ClientPointers = FALSE;            // Ensure FALSE is defined

        // Write the dump
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            dumpFile,
            MiniDumpWithFullMemory,
            &dumpInfo,
            NULL,
            NULL);

        CloseHandle(dumpFile);
    }

	printf("UnRAWer has crashed. A dump file has been saved as %s\n", dumpFileName);

    // Return to terminate the application
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char* argv[]) {
    SetUnhandledExceptionFilter(CustomExceptionHandler);

    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow == NULL) {
        // Allocate console and redirect std output
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    //Log_Init();
    //Log_SetVerbosity(3);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%^[%l]%$<%t> %v");

    time_t timestamp;
    time(&timestamp);
    std::cout << std::fixed << std::setprecision(8);


    //qDebug() << qPrintable(QString("UnRAWer %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR));
    //qDebug() << qPrintable(QString("Build from: %1, %2").arg(__DATE__).arg(__TIME__));
    //qDebug() << "Debug output:";

	spdlog::info("UnRAWer {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	spdlog::info("Build from: {} {}", __DATE__, __TIME__);
    spdlog::info("Log started at: {}", ctime(&timestamp), "%Y-%m-%d %H:%M:%S");
    //int* crash = nullptr;
    //*crash = 42;

    // check if arguments are passed
    if (argc == 1) {

		if (!loadSettings(settings, "unrw_config.toml")) {
			//LOG(error) << "Can not load [unrw_config.toml] Using default settings." << std::endl;
			spdlog::error("Can not load [unrw_config.toml] Using default settings.");
			settings.reSettings();
		}
		
        ShowWindow(GetConsoleWindow(), (settings.conEnable) ? SW_SHOW : SW_HIDE);
		printSettings(settings);

		QApplication app(argc, argv);
		app.setWindowIcon(QIcon(":/MainWindow/unrw.ico"));

		MainWindow window;
		window.show();

		return app.exec();
    }
    else {
		return cli_main(argc, argv);
    }

	return 0;
}
