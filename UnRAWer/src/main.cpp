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

#include "stdafx.h"
#include "settings.h"

int main(int argc, char* argv[]) {
    // Allocate console and redirect std output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    Log_Init();

    if (!loadSettings(settings, "unrw_config.toml")) {
        LOG(error) << "Can not load [unrw_config.toml] Using default settings." << std::endl;
        settings.reSettings();
    }

    ShowWindow(GetConsoleWindow(), (settings.conEnable) ? SW_SHOW : SW_HIDE);
    qDebug() << qPrintable(QString("UnRAWer %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR)) << "Debug output:";
    printSettings(settings);


    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/MainWindow/unrw.ico"));

    MainWindow window;
    window.show();

    return app.exec();
}