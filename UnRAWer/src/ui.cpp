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
#include "ui.h"

#include "processing.h"
#include "settings.h"

class MainWindow;

DropArea::DropArea() {
    QVBoxLayout* layout = new QVBoxLayout;
    QLabel* label = new QLabel("Drag & drop files here");

    QFont font = this->font();
    font.setPointSize(16);

    label->setFont(font);
    label->setStyleSheet("QLabel { color: #808080; }");
    label->setAlignment(Qt::AlignCenter);
    
    layout->addWidget(label, 0, Qt::AlignCenter);
    setLayout(layout);
    setAcceptDrops(true);
}

void DropArea::dragEnterEvent(QDragEnterEvent* event) {
    event->acceptProposedAction();
}

void DropArea::dropEvent(QDropEvent* event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        emit filesDropped(urls);  // Emit the signal when files are dropped
    }
}

MainWindow::MainWindow() {
    this->setStyleSheet(" color: #E0E0E0; background-color: #101010; ");

    QVBoxLayout* layout = new QVBoxLayout;  // Create a vertical layout

    DropArea* dropArea = new DropArea;
    // set size of drop area to fill layout area
    dropArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dropArea->setAutoFillBackground(true);  // Fill the background with the color below
    dropArea->setStyleSheet("border-radius: 3px; background-color: #181818; margin-bottom: 4px;");
    layout->addWidget(dropArea);

    // Progress bar
    progressBar = new QProgressBar;
    progressBar->setFixedHeight(20);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(
        "QProgressBar {border: 0px solid black; border-radius: 3px; background-color: black; color: white; margin-bottom: 4px;}"
        "QProgressBar::chunk {background-color: #05B8CC;}");
    layout->addWidget(progressBar);

    // Create a new QPlainTextEdit
    QPlainTextEdit* textOutput = new QPlainTextEdit;
    textOutput->setReadOnly(true);  // Make it read-only so users can't edit the text
    textOutput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textOutput->setFixedHeight(52);
    textOutput->setStyleSheet("border-radius: 3px; background-color: #181818; color: #E0E0E0; padding-left: 4px; padding-right: 4px;");
    // text size
    QFont font = this->font();
    font.setPointSize(8);
    font.setStyleHint(QFont::Monospace);
    textOutput->setFont(font);

    textOutput->setPlainText("Waiting for user inputs...");
    textOutput->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(textOutput);

    // widgets

    QWidget* centralWidget = new QWidget;
    centralWidget->setLayout(layout);  // Set the layout for the central widget
    setCentralWidget(centralWidget);  // Set the central widget of the MainWindow

    QMenuBar* menuBar = new QMenuBar;

    menuBar->setStyleSheet(
        "QMenuBar { background-color: #181818; color: #E0E0E0; }"
        "QMenuBar::item { color: #E0E0E0; }"
        "QMenuBar::item:selected { background-color: #405a80; color: #F0F0F0; }"
        "QMenuBar::item:hover { color: #181818; background-color: #3d79cc; }"
        "QMenu { background-color: #282828; color: #E0E0E0; }"
        "QMenu::item { color: #E0E0E0; }"
        "QMenu::item:selected { background-color: #405a80; color: #F0F0F0; }"
        "QMenu::item:hover { color: #181818; background-color: #3d79cc; }"
        "QMenu::separator { background-color: #181818; height: 1px; }"
    );

    QMenu* f_menu = new QMenu("Files", menuBar);
    QMenu* p_menu = new QMenu("Processing", menuBar);
    QMenu* s_menu = new QMenu("Settings", menuBar);

    //menuBar->setStyleSheet(
    //    "QMenu { color: #E0E0E0; }"
    //    "QMenu::item { color: #E0E0E0; }"
    //    "QMenu::item:selected { background-color: #405a80; color: #F0F0F0; }"
    //    "QMenu::item:hover { color: #181818; background-color: #3d79cc; }"
    //    "QMenu::separator { background-color: #181818; height: 1px; }"
    //);

    // file menu
    QAction* f_RelConf = new QAction("Reload Config", f_menu);
    QAction* f_Restart = new QAction("Restart", f_menu);
    QAction* f_Exit = new QAction("Exit", f_menu);
    
    QAction* con_enable = new QAction("Enable Console", s_menu);
    con_enable->setCheckable(true);
    con_enable->setChecked(settings.conEnable);

    QAction* useSubfldr = new QAction("Export Subfolders", p_menu);
    useSubfldr->setCheckable(true);
    useSubfldr->setChecked(settings.useSbFldr);

    // Submenu
    QMenu* rng_submenu = new QMenu("Floats type", p_menu);
    QMenu* fmt_submenu = new QMenu("Formats", p_menu);
    QMenu* bit_submenu = new QMenu("Bits Depth", p_menu);
    QMenu* raw_submenu = new QMenu("Camera Raw", p_menu);
    QMenu* dem_submenu = new QMenu("Demosaic", p_menu);
    QMenu* rclr_submenu = new QMenu("RAW ColorSpace", p_menu);
    QMenu* lut_submenu = new QMenu("LUT transform", p_menu);
    
    QMenu* lut_p_submenu = new QMenu("LUT Presets", lut_submenu);

    QMenu* verb_submenu = new QMenu("Verbosity", s_menu);
    
    QActionGroup* RangeGroup = new QActionGroup(rng_submenu);
    QActionGroup* FrmtGroup = new QActionGroup(fmt_submenu);
    QActionGroup* BitsGroup = new QActionGroup(bit_submenu);
    QActionGroup* RawGroup = new QActionGroup(raw_submenu);
    QActionGroup* DemGroup = new QActionGroup(dem_submenu);
    QActionGroup* RclrGroup = new QActionGroup(rclr_submenu);
    QActionGroup* LutGroup = new QActionGroup(lut_submenu);
    QActionGroup* LutPresetGroup = new QActionGroup(lut_p_submenu);

    QActionGroup* VerbGroup = new QActionGroup(verb_submenu);

    auto createAction = [](const QString& title, QActionGroup* group, QMenu* menu, bool checkable = true, bool checked = false) {
        QAction* action = new QAction(title, menu);
        action->setCheckable(checkable);
        action->setChecked(checked);
        group->addAction(action);
        menu->addAction(action);
        return action;
    };
    // Verbose level
    std::vector<std::pair<const QString, int>> verbMenu = {
        {"0 - Fatal", 0}, {"1 - Error", 1}, {"2 - Warning", 2}, {"3 - Info", 3}, {"4 - Debug", 4}, {"5 - Trace", 5} };
    for (auto& [title, value] : verbMenu) {
        QAction* action = createAction(title, VerbGroup, verb_submenu, true, (settings.verbosity == value));
        verbActions.push_back(action);
    }

    // Range
    std::vector<std::pair<const QString, int>> rngMenu = {
		{"Unsigned", 0}, {"Signed", 1}, {"Signed > Unsigned", 2}, {"Unsigned > Signed", 3} };
    for (auto& [title, value] : rngMenu) {
		QAction* action = createAction(title, RangeGroup, rng_submenu, true, (settings.rangeMode == value));
		rngActions.push_back(action);
	}
    // File Formats
    std::vector<std::pair<const QString, int>> frmtMenu = {
		{"Original", -1}, {"TIFF", 0}, {"OpenEXR", 1}, {"PNG", 2}, {"JPEG", 3}, {"JPEG-2000", 4}, {"PPM", 5} };
    for (auto& [title, value] : frmtMenu) {
        QAction* action = createAction(title, FrmtGroup, fmt_submenu, true, (settings.fileFormat == value));
        frmtActions.push_back(action);
    }       
    // Bits Depth
    std::vector<std::pair<const QString, int>> bitMenu = {
        {"Original", -1},   {"8 bits int", 0},  {"16 bits int", 1},
        {"32 bits int", 2}, {"64 bits int", 3}, {"16 bits float", 4}, 
        {"32 bits float", 5}, {"64 bits float", 6} };
    for (auto& [title, value] : bitMenu) {
		QAction* action = createAction(title, BitsGroup, bit_submenu, true, (settings.bitDepth == value));
		bitActions.push_back(action);
	}
    // camera raw rotation
    std::vector<std::pair<const QString, int>> rawMenu = {
		{"Auto EXIF", -1}, {"0 Horizontal", 0}, {"-90 Vertical", 3},
		{"+90 Vertical", 5}, {"180 Horizontal", 6} };
    for (auto& [title, value] : rawMenu) {
        QAction* action = createAction(title, RawGroup, raw_submenu, true, (settings.rawRot == value));
        rawActions.push_back(action);
    }
    // demosaic
    std::vector<std::pair<const QString, int>> demMenu = {
        // "none", "linear", "VNG", "PPG", "AHD", "DCB", "AHD-Mod", "AFD", "VCD", "Mixed", "LMMSE", "AMaZE", "DHT", "AAHD"
        {"none", 0}, {"linear", 1}, {"VNG", 2}, {"PPG", 3}, 
        {"AHD", 4}, {"DCB", 5}, {"AHD-Mod", 6}, {"AFD", 7}, 
        {"VCD", 8}, {"Mixed", 9}, {"LMMSE", 10}, {"AMaZE", 11}, 
        {"DHT", 12}, {"AAHD", 13} };
    for (auto& [title, value] : demMenu) {
        QAction* action = createAction(title, DemGroup, dem_submenu, true, (settings.dDemosaic == value));
        demActions.push_back(action);
    }
    // raw colorspace
    std::vector<std::pair<const QString, int>> rclrMenu = {
        // "Raw", "sRGB", "sRGB-linear", "Adobe", "Wide", "ProPhoto", "ProPhoto-linear", "XYZ", "ACES", "DCI-P3", "Rec2020"
        {"Raw", 0}, {"sRGB", 1}, {"sRGB-linear", 2}, {"Adobe", 3}, 
		{"Wide", 4}, {"ProPhoto", 5}, {"ProPhoto-linear", 6}, {"XYZ", 7}, 
		{"ACES", 8}, {"DCI-P3", 9}, {"Rec2020", 10} };
    for (auto& [title, value] : rclrMenu) {
		QAction* action = createAction(title, RclrGroup, rclr_submenu, true, (settings.rawSpace == value));
		rclrActions.push_back(action);
	}
    // lut transform
    std::vector<std::pair<const QString, int>> lutMenu = {
        {"Disabled", -1}, {"Smart", 0}, {"Forced", 1} };
    for (auto& [title, value] : lutMenu) {
		QAction* action = createAction(title, LutGroup, lut_submenu, true, (settings.lutMode == value));
		lutActions.push_back(action);
	}
    //lut presets
    for (auto& key : settings.lut_Preset) {
        // const std::string to QString
		QAction* action = createAction(QString::fromStdString(key.first), LutPresetGroup, lut_p_submenu, true, (settings.dLutPreset == key.first));
		lutActions.push_back(action);
	}
    //
    menuBar->addMenu(f_menu);
    menuBar->addMenu(p_menu);
    menuBar->addMenu(s_menu);
    //
    s_menu->addMenu(verb_submenu);
    s_menu->addSeparator();
    s_menu->addAction(con_enable);
	//
    p_menu->addMenu(raw_submenu);
    p_menu->addMenu(dem_submenu);
    p_menu->addMenu(rclr_submenu);
    p_menu->addSeparator();
    p_menu->addMenu(lut_submenu);
    p_menu->addMenu(lut_p_submenu);
    p_menu->addSeparator();
    p_menu->addMenu(rng_submenu);
    p_menu->addSeparator();
    p_menu->addMenu(fmt_submenu);
    p_menu->addMenu(bit_submenu);
    p_menu->addSeparator();
    p_menu->addAction(useSubfldr);
    p_menu->addSeparator();
    //
    f_menu->addAction(f_RelConf);
    f_menu->addAction(f_Restart);
    f_menu->addSeparator();
    f_menu->addAction(f_Exit);

    setMenuBar(menuBar);

    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowTitle(QString("UnRAWer ToolBox %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR));
    setFixedSize(500, 500);

    // Connect the signal from the drop area to the slot in the main window
    connect(dropArea, &DropArea::filesDropped, this, &MainWindow::startProcessing);

    // Connect the Settings action's triggered signal

//QList<QAction*> rng_act = { rng_Unsg, rng_Sign, rng_US, rng_SU };
    for (QAction* action : rngActions) {
        connect(action, &QAction::triggered, this, &MainWindow::rngSettings);
    }
    for (QAction* action : frmtActions) {
        connect(action, &QAction::triggered, this, &MainWindow::frmtSettings);
    }
    for (QAction* action : bitActions) {
		connect(action, &QAction::triggered, this, &MainWindow::bitSettings);
	}
    for (QAction* action : rawActions) {
		connect(action, &QAction::triggered, this, &MainWindow::rawSettings);
	}
    for (QAction* action : demActions) {
        connect(action, &QAction::triggered, this, &MainWindow::demSettings);
    }
    for (QAction* action : rclrActions) {
		connect(action, &QAction::triggered, this, &MainWindow::rclrSettings);
	}
    for (QAction* action : lutActions) {
        connect(action, &QAction::triggered, this, &MainWindow::lutSettings);
    }
    for (QAction* action : verbActions) {
		connect(action, &QAction::triggered, this, &MainWindow::verbLevel);
	}

    connect(con_enable, &QAction::toggled, this, &MainWindow::toggleConsole);
    connect(useSubfldr, &QAction::toggled, this, &MainWindow::toggleSubfldr);
    // Add new connection for updating the textOutput
    connect(this, &MainWindow::updateTextSignal, textOutput, &QPlainTextEdit::setPlainText);

    // Connect the Exit action's triggered signal to QApplication's quit slot
    connect(f_Exit, &QAction::triggered, qApp, &QApplication::quit);
    connect(f_Restart, &QAction::triggered, this, &MainWindow::restartApp);
    connect(f_RelConf, &QAction::triggered, this, &MainWindow::reloadConfig);

    connect(this, &MainWindow::changeProgressBarColorSignal, this, &MainWindow::changeProgressBarColorSlot);
}

void MainWindow::verbLevel() {
    std::vector<std::pair<QString, int>> actionMap = {
        {"0 - Fatal", 0}, {"1 - Error", 1}, {"2 - Warning", 2}, {"3 - Info", 3}, {"4 - Debug", 4}, {"5 - Trace", 5}
    };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < verbActions.size() && i < actionMap.size(); ++i) {
        if (action == verbActions[i]) {
			settings.verbosity = actionMap[i].second;
            Log_SetVerbosity(settings.verbosity);
			emit updateTextSignal(QString("Verbosity level set to %1").arg(settings.verbosity));
            qDebug() << qPrintable(QString("Verbosity level set to %1").arg(settings.verbosity));
			break;
		}
	}
}

void MainWindow::bitSettings() {
    std::vector<std::pair<QString, int>> actionMap = {
        {"original", -1},
        {"8 bits int", 0},
        {"16 bits int", 1},
        {"32 bits int", 2},
        {"64 bits int", 3},
        {"16 bits float", 4},
        {"32 bits float", 5},
        {"64 bits float", 6}
    };
    QAction* action = qobject_cast<QAction*>(sender());

    //   if (bitActions.size() != actionMap.size()) {
    //	qDebug() << "Error! bitActions size is not equal to actionMap size";
    //       return;
	//}

    for (int i = 0; i < bitActions.size() && i < actionMap.size(); ++i) {
        if (action == bitActions[i]) {
            settings.bitDepth = actionMap[i].second;
            emit updateTextSignal(QString("Bit precision: %1 ").arg(actionMap[i].first));
            qDebug() << qPrintable(QString("Bit precision: %1 ").arg(actionMap[i].first));
            break;
        }
    }
}

void MainWindow::frmtSettings() {
    std::vector<std::pair<QString, int>> actionMap = {
		{"Original",-1},
		{"TIFF",     0},
		{"OpenEXR",  1},
		{"PNG",      2},
		{"JPEG",     3},
		{"JPEG2000", 4},
		{"PPM",      5}
	};

    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < frmtActions.size() && i < actionMap.size(); ++i) {
		if (action == frmtActions[i]) {
			settings.fileFormat = actionMap[i].second;
			emit updateTextSignal(QString("Output file format: %1 ").arg(actionMap[i].first));
			qDebug() << qPrintable(QString("Output file format: %1 ").arg(actionMap[i].first));
			break;
		}
	}
}

void MainWindow::rngSettings() {
	QAction* action = qobject_cast<QAction*>(sender());
    if (action == rngActions[0]) {
        settings.rangeMode = 0;
		emit updateTextSignal("Unsigned floats");
        qDebug() << "32/16 bit float Normals are in [0.0 ~ 1.0] range";
	}
    else if (action == rngActions[1]) {
        settings.rangeMode = 1;
		emit updateTextSignal("Signed floats");
        qDebug() << "32/16 bit floats Normals are in [-1.0 ~ 1.0] range";
	}
    else if (action == rngActions[2]) {
		settings.rangeMode = 2;
        emit updateTextSignal("Signed <> Unsigned");
        qDebug() << "32/16 bit floats Normals are in [-1.0 ~ 1.0] range converted to [0.0 ~ 1.0]";
    }
    else if (action == rngActions[3]) {
        settings.rangeMode = 3;
        emit updateTextSignal("Unsigned <> Signed");
        qDebug() << "32/16 bit floats Normals are in [0.0 ~ 1.0] range converted to [-1.0 ~ 1.0]";
    }
}

void MainWindow::rawSettings() {
    QAction* action = qobject_cast<QAction*>(sender());

    if (action == rawActions[0]) {
        settings.rawRot = settings.raw_rot[0];
        emit updateTextSignal("Camera Raw rotation - Auto");
        qDebug() << "Camera Raw rotation set to EXIF Auto";
    }
    else if (action == rawActions[1]) {
        settings.rawRot = settings.raw_rot[1];
        emit updateTextSignal("Camera Raw rotation - 0 (Unrotate)");
        qDebug() << "Camera Raw rotation set to 0 degree - Unrotatate/Horizontal";
    }
    else if (action == rawActions[2]) {
		settings.rawRot = settings.raw_rot[3];
        emit updateTextSignal("Camera Raw rotation - 90 CW (Vertical)");
		qDebug() << "Camera Raw rotation set to 90 degree CW - Vertical";
	}
    else if (action == rawActions[3]) {
		settings.rawRot = settings.raw_rot[2];
		emit updateTextSignal("Camera Raw rotation - 180 (Horizontal)");
        qDebug() << "Camera Raw rotation set to 180 degree (Horizontal)";
	}
    else if (action == rawActions[4]) {
		settings.rawRot = settings.raw_rot[4];
		emit updateTextSignal("Camera Raw rotation - 90 CW (Vertical)");
		qDebug() << "Camera Raw rotation set to 90 degree CW (Vertical)";
	}
}

void MainWindow::demSettings() {
    std::vector<std::pair<QString, int>> actionMap = { 
        // "none", "linear", "VNG", "PPG", "AHD", "DCB", "AHD-Mod", "AFD", "VCD", "Mixed", "LMMSE", "AMaZE", "DHT", "AAHD"
        {"none", 0}, {"linear", 1}, {"VNG", 2}, 
        {"PPG", 3},  {"AHD", 4},    {"DCB", 5}, 
        {"AHD-Mod", 6}, {"AFD", 7}, {"VCD", 8}, 
        {"Mixed", 9}, {"LMMSE", 10}, {"AMaZE", 11}, 
        {"DHT", 12}, {"AAHD", 13}
    };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < demActions.size() && i < actionMap.size(); ++i) {
        if (action == demActions[i]) {
			settings.dDemosaic = actionMap[i].second;
			emit updateTextSignal(QString("Demosaic - %1 ").arg(actionMap[i].first));
			qDebug() << qPrintable(QString("Demosaic - %1 ").arg(actionMap[i].first));
			break;
		}
    }
}

void MainWindow::rclrSettings() {
    std::vector<std::pair<QString, int>> actionMap = {
        // "Raw", "sRGB", "sRGB-linear", "Adobe", "Wide", "ProPhoto", "ProPhoto-linear", "XYZ", "ACES", "DCI-P3", "Rec2020"
        {"Raw", 0}, {"sRGB", 1}, {"sRGB-linear", 2},
		{"Adobe", 3}, {"Wide", 4}, {"ProPhoto", 5},
		{"ProPhoto-linear", 6}, {"XYZ", 7}, {"ACES", 8},
		{"DCI-P3", 9}, {"Rec2020", 10}
    };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < rclrActions.size() && i < actionMap.size(); ++i) {
        if (action == rclrActions[i]) {
            settings.rawSpace = actionMap[i].second;
            emit updateTextSignal(QString("Raw color space - %1 ").arg(actionMap[i].first));
            qDebug() << qPrintable(QString("Raw color space - %1 ").arg(actionMap[i].first));
        }
    }
}

void MainWindow::lutSettings() {
    std::vector<std::pair<QString, int>> actionMap = { {"Off", -1}, {"Smart", 0}, {"Forced", 1} };

    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < lutActions.size() && i < actionMap.size(); ++i) {
		if (action == lutActions[i]) {
			settings.lutMode = actionMap[i].second;
			emit updateTextSignal(QString("LUT transform - %1 ").arg(actionMap[i].first));
			qDebug() << qPrintable(QString("LUT transform - %1 ").arg(actionMap[i].first));
			break;
		}
	}
}

void MainWindow::startProcessing(QList<QUrl> urls) {
    // Move the processing to a separate thread
    QFuture<bool> future = QtConcurrent::run(doProcessing, urls, progressBar, this);

    processingWatcher.setFuture(future);

    connect(&processingWatcher, &QFutureWatcher<bool>::progressValueChanged, progressBar, &QProgressBar::setValue);
}

void MainWindow::toggleConsole(bool checked) {
    if (checked) {
        // Show console
        ShowWindow(GetConsoleWindow(), SW_SHOW);
    }
    else {
        // Hide console
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
}

void MainWindow::toggleSubfldr(bool checked) {
	settings.useSbFldr = checked;
    emit updateTextSignal(QString("Use subfolders - %1").arg(checked ? "On" : "Off"));
    qDebug() << qPrintable(QString("Use subfolders - %1").arg(checked ? "On" : "Off"));
}

void MainWindow::restartApp() {
    QStringList arguments;

    ShowWindow(GetConsoleWindow(), SW_HIDE);
    FreeConsole();

    QProcess::startDetached(QApplication::applicationFilePath(), arguments);

    QApplication::quit();
}

void MainWindow::reloadConfig() {
    if (!loadSettings(settings, "unrw_config.toml")) {
        LOG(error) << "Can not load [unrw_config.toml] Using default settings." << std::endl;
        settings.reSettings();
    }
    printSettings(settings);
}

void setPBarColor(QProgressBar* progressBar, const QColor& color) {
    QString style = QString(
        "QProgressBar {"
        "border: 0px solid black; border-radius: 3px; background-color: black; color: white;"
        "margin-bottom: 4px;"
        "}"
        "QProgressBar::chunk {background-color: %1;}"
    ).arg(color.name());

    progressBar->setStyleSheet(style);
}