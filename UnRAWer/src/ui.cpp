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
#include "ui.h"

#include "process.h"
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

//MainWindow::MainWindow();

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

    // file menu
    QMenu* f_menu = new QMenu("Files", menuBar);
    QAction* f_RelConf = new QAction("Reload Config", f_menu);
    QAction* f_Restart = new QAction("Restart", f_menu);
    QAction* f_Exit = new QAction("Exit", f_menu);


    //menuBar->setStyleSheet(
    //    "QMenu { color: #E0E0E0; }"
    //    "QMenu::item { color: #E0E0E0; }"
    //    "QMenu::item:selected { background-color: #405a80; color: #F0F0F0; }"
    //    "QMenu::item:hover { color: #181818; background-color: #3d79cc; }"
    //    "QMenu::separator { background-color: #181818; height: 1px; }"
    //);

	// RAW menu
    QMenu* r_menu = new QMenu("RAW", menuBar);
	
    zero_raw = new QAction("Disable RAW", r_menu);

    QMenu* raw_submenu = new QMenu("RAW Rotation", r_menu);
	QMenu* dem_submenu = new QMenu("Demosaic", r_menu);
	QMenu* denoise_submenu = new QMenu("Denoise", r_menu);
	QMenu* rclr_submenu = new QMenu("RAW ColorSpace", r_menu);
	QMenu* mtx_submenu = new QMenu("Camera Matrix", r_menu);
	QMenu* hlt_submenu = new QMenu("Highlights", r_menu);

    halfSizeRaw = new QAction("Half Resolution", r_menu);
	halfSizeRaw->setCheckable(true);
	halfSizeRaw->setChecked(settings.rawParms.half_size == 0 ? false : true);

	auto_wb = new QAction("Auto WB", r_menu);
	auto_wb->setCheckable(true);
	auto_wb->setChecked(settings.rawParms.use_auto_wb == 0 ? false : true);
	camera_wb = new QAction("Camera WB", r_menu);
	camera_wb->setCheckable(true);
	camera_wb->setChecked(settings.rawParms.use_camera_wb == 0 ? false : true);

	// Processing menu
    QMenu* p_menu = new QMenu("Processing", menuBar);
	zero_proc = new QAction("Disable Processing", p_menu);

	lut_exif = new QAction("Per Camera", p_menu);
	lut_exif->setCheckable(true);
	lut_exif->setChecked(settings.perCamera);

	QMenu* lut_submenu = new QMenu("LUT transform", p_menu);
	QMenu* lut_p_submenu = new QMenu("LUT Presets", p_menu);

	QMenu* sharp_submenu = new QMenu("Unsharp", p_menu);
	QMenu* sharp_k_submenu = new QMenu("Unsharp kernel", p_menu);
	QMenu* crop_submenu = new QMenu("Crop", p_menu);

	// Outputs menu
    QMenu* o_menu = new QMenu("Outputs", menuBar);
    QAction* useSubfldr = new QAction("Export Subfolders", o_menu);
    useSubfldr->setCheckable(true);
    useSubfldr->setChecked(settings.useSbFldr);

    QMenu* rng_submenu = new QMenu("Floats type", o_menu);
	QMenu* fmt_submenu = new QMenu("Formats", o_menu);
	QMenu* bit_submenu = new QMenu("Bits Depth", o_menu);

	// Settings menu
    QMenu* s_menu = new QMenu("Settings", menuBar);
    QAction* con_enable = new QAction("Enable Console", s_menu);
    con_enable->setCheckable(true);
    con_enable->setChecked(settings.conEnable);
    QAction* prnt_settings = new QAction("Print Settings", s_menu);
    prnt_settings->setCheckable(false);

	// debug menu
    QMenu* d_menu = new QMenu("Debug", menuBar);
	QMenu* verb_submenu = new QMenu("Verbosity", d_menu);
	QMenu* dump_submenu = new QMenu("Create Dump", d_menu);

    QActionGroup* RangeGroup = new QActionGroup(rng_submenu);
    QActionGroup* FrmtGroup = new QActionGroup(fmt_submenu);
    QActionGroup* BitsGroup = new QActionGroup(bit_submenu);
    QActionGroup* RawGroup = new QActionGroup(raw_submenu);
    QActionGroup* DemGroup = new QActionGroup(dem_submenu);
    QActionGroup* DenoiseGroup = new QActionGroup(dem_submenu);
    QActionGroup* RclrGroup = new QActionGroup(rclr_submenu);
	QActionGroup* MtxGroup = new QActionGroup(mtx_submenu);
	QActionGroup* HltGroup = new QActionGroup(hlt_submenu);
    QActionGroup* LutGroup = new QActionGroup(lut_submenu);
    QActionGroup* SharpGroup = new QActionGroup(sharp_submenu);
    QActionGroup* SharpKGroup = new QActionGroup(sharp_k_submenu);
    QActionGroup* LutPresetGroup = new QActionGroup(lut_p_submenu);
	QActionGroup* CropGroup = new QActionGroup(crop_submenu);

    QActionGroup* VerbGroup = new QActionGroup(verb_submenu);
	QActionGroup* DumpGroup = new QActionGroup(dump_submenu);

    auto createAction = [](const QString& title, QActionGroup* group, QMenu* menu, bool checkable = true, bool checked = false) {
        QAction* action = new QAction(title, menu);
        action->setCheckable(checkable);
        action->setChecked(checked);
        group->addAction(action);
        menu->addAction(action);
        return action;
    };
	// Dump menu
	std::vector<QString> dumpMenu = { "Normal", "Full" };
	for (QString& title : dumpMenu) {
		QAction* action = createAction(title, DumpGroup, dump_submenu, false, false);
		dumpActions.push_back(action);
	}

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
		{"Original", -1}, {"TIFF", 0}, {"OpenEXR", 1}, {"PNG", 2}, {"JPEG", 3}, {"JPEG-2000", 4}, {"JPEG-XL", 5}, {"HEIC", 6}, {"PPM", 7}};
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
		{"Auto EXIF", -1}, {"0 Horizontal", 0}, {"180 Horisontal", 3},
		{"-90 Vertical", 5}, {"+90 Vertical", 6} };
    for (auto& [title, value] : rawMenu) {
        QAction* action = createAction(title, RawGroup, raw_submenu, true, (settings.rawRot == value));
        rawActions.push_back(action);
    }
	// camera matrix
	std::vector<std::pair<const QString, int>> mtxMenu = {
        {"Don't use", 1 }, { "DNG Embedded", 0 }, { "Always", 0 } };
	for (auto& [title, value] : mtxMenu) {
		QAction* action = createAction(title, MtxGroup, mtx_submenu, true, (settings.rawParms.use_camera_matrix == value));
		mtxActions.push_back(action);
	}
	// highlights
	std::vector<std::pair<const QString, int>> hltMenu = {
		{"Clip", 0}, {"Unclip", 1}, {"Blend", 2}, {"Rebuild", 3} };
	for (auto& [title, value] : hltMenu) {
		QAction* action = createAction(title, HltGroup, hlt_submenu, true, (settings.rawParms.highlight == value));
		hltActions.push_back(action);
	}
    // demosaic
    std::vector<std::pair<const QString, int>> demMenu = {
        // "raw data", "none", "linear", "VNG", "PPG", "AHD", "DCB", "", "", "", "", "", "", "DHT", "AAHD"
        {"RAW data", -2}, {"none", -1}, {"linear", 0}, {"VNG", 1}, {"PPG", 2},
        {"AHD", 3}, {"DCB", 4}, 
        //{"", 5}, {"", 6}, 
        //{"", 7}, {"", 8}, {"", 9}, {"", 10}, 
        {"DHT", 11}, {"AAHD", 12} };
    for (auto& [title, value] : demMenu) {
        QAction* action = createAction(title, DemGroup, dem_submenu, true, (settings.dDemosaic == value));
        demActions.push_back(action);
    }
    // Denoise
    std::vector<std::pair<const QString, uint>> denoiseMenu = { {"Disabled", 0}, {"Wavelet", 1}, {"FBDD", 2}, {"Both", 3} };
    for (auto& [title, value] : denoiseMenu) {
		QAction* action = createAction(title, DenoiseGroup, denoise_submenu, true, (settings.denoise_mode == value));
        denoiseActions.push_back(action);
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
    // crop
	std::vector<std::pair<const QString, int>> cropMenu = {
		{"Disabled", -1}, {"Auto", 0}, {"Forced", 1} };
    for (auto& [title, value] : cropMenu) {
        QAction* action = createAction(title, CropGroup, crop_submenu, true, (settings.crop_mode == value));
        cropActions.push_back(action);
    }
    // lut transform
    std::vector<std::pair<const QString, int>> lutMenu = {
        {"Disabled", -1}, {"Smart", 0}, {"Forced", 1} };
    for (auto& [title, value] : lutMenu) {
		QAction* action = createAction(title, LutGroup, lut_submenu, true, (settings.lutMode == value));
		lutActions.push_back(action);
	}
    //lut presets
	// check if lut_Preset is not empty
    if (!settings.lut_Preset.empty()) {
        for (auto& [key, value] : settings.lut_Preset) {
            QAction* action = createAction(key.c_str(), LutPresetGroup, lut_p_submenu, true, settings.dLutPreset == key);
            lutPActions.push_back(action);
        }
    }
    // sharpening
    std::vector<std::pair<const QString, int>> unsharpMenu = {
		{"Disabled", -1}, {"Smart", 0}, {"Forced", 1} };
    for (auto& [title, value] : unsharpMenu) {
        QAction* action = createAction(title, SharpGroup, sharp_submenu, true, (settings.sharp_mode == value));
        sharpActions.push_back(action);
    }
    //sharpen kernels
    for (auto& key : settings.sharp_kerns) {
        std::string kernel = settings.sharp_kerns[settings.sharp_kernel];
        QAction* action = createAction(key.c_str(), SharpKGroup, sharp_k_submenu, true, (kernel == key));
        sharpKActions.push_back(action);
    }
    //
    menuBar->addMenu(f_menu);
    menuBar->addMenu(r_menu);
    menuBar->addMenu(p_menu);
    menuBar->addMenu(o_menu);
    menuBar->addMenu(s_menu);
	menuBar->addMenu(d_menu);
    //
    d_menu->addMenu(verb_submenu);
    d_menu->addSeparator();
    d_menu->addAction(con_enable);
    d_menu->addSeparator();
	d_menu->addMenu(dump_submenu);
    //
    s_menu->addAction(prnt_settings);
	//
	r_menu->addAction(zero_raw);
    r_menu->addMenu(raw_submenu);
    r_menu->addMenu(dem_submenu);
    r_menu->addMenu(rclr_submenu);
    r_menu->addSeparator();
    r_menu->addMenu(denoise_submenu);
	r_menu->addAction(auto_wb);
	r_menu->addAction(camera_wb);
	r_menu->addMenu(mtx_submenu);
	r_menu->addMenu(hlt_submenu);
    r_menu->addSeparator();
    r_menu->addAction(halfSizeRaw);
    r_menu->addSeparator();
    //
	p_menu->addAction(zero_proc);
	p_menu->addMenu(crop_submenu);
	p_menu->addSeparator();
	p_menu->addAction(lut_exif);
    p_menu->addMenu(lut_submenu);
    p_menu->addMenu(lut_p_submenu);
    p_menu->addSeparator();
    p_menu->addMenu(sharp_submenu);
    p_menu->addMenu(sharp_k_submenu);
    p_menu->addSeparator();
    //
    o_menu->addMenu(rng_submenu);
    o_menu->addSeparator();
    o_menu->addMenu(fmt_submenu);
    o_menu->addMenu(bit_submenu);
    o_menu->addSeparator();
    o_menu->addAction(useSubfldr);
    o_menu->addSeparator();
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

    connect(zero_raw, &QAction::triggered, this, &MainWindow::zero);
	connect(auto_wb, &QAction::triggered, this, &MainWindow::autoWbSettings);
	connect(camera_wb, &QAction::triggered, this, &MainWindow::autoWbSettings);

	for (QAction* action : mtxActions) {
		connect(action, &QAction::triggered, this, &MainWindow::mtxSettings);
	}
	for (QAction* action : hltActions) {
		connect(action, &QAction::triggered, this, &MainWindow::hltSettings);
	}

    for (QAction* action : rawActions) {
		connect(action, &QAction::triggered, this, &MainWindow::rawSettings);
	}
    for (QAction* action : demActions) {
        connect(action, &QAction::triggered, this, &MainWindow::demSettings);
    }
    for (QAction* action : denoiseActions) {
        connect(action, &QAction::triggered, this, &MainWindow::denoiseSettings);
    }
    for (QAction* action : rclrActions) {
		connect(action, &QAction::triggered, this, &MainWindow::rclrSettings);
	}

	connect(zero_proc, &QAction::triggered, this, &MainWindow::zero);

	for (QAction* action : cropActions) {
		connect(action, &QAction::triggered, this, &MainWindow::cropSettings);
	}

	connect(lut_exif, &QAction::triggered, this, &MainWindow::lutCameraSettings);
    
    for (QAction* action : lutActions) {
        connect(action, &QAction::triggered, this, &MainWindow::lutSettings);
    }
    for (QAction* action : lutPActions) {
        connect(action, &QAction::triggered, this, &MainWindow::lutPSettings);
    }
    for (QAction* action : sharpActions) {
        connect(action, &QAction::triggered, this, &MainWindow::sharpSettings);
    }
    for (QAction* action : sharpKActions) {
		connect(action, &QAction::triggered, this, &MainWindow::sharpKSettings);
	}
    for (QAction* action : verbActions) {
		connect(action, &QAction::triggered, this, &MainWindow::verbLevel);
	}
    for (QAction* action : dumpActions) {
        connect(action, &QAction::triggered, this, &MainWindow::createMemoryDump);
    }

    connect(con_enable, &QAction::toggled, this, &MainWindow::toggleConsole);
    connect(prnt_settings, &QAction::triggered, this, &MainWindow::prntSettings);
    connect(useSubfldr, &QAction::toggled, this, &MainWindow::toggleSubfldr);
    connect(halfSizeRaw, &QAction::toggled, this, &MainWindow::halfSizeSettings);
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
        {"0 - Fatal", 0}, {"1 - Error", 1}, {"2 - Warning", 2}, {"3 - Info", 3}, {"4 - Debug", 4}, {"0 - Trace", 5}
    };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < verbActions.size() && i < actionMap.size(); ++i) {
        if (action == verbActions[i]) {
			settings.verbosity = actionMap[i].second;
            //Log_SetVerbosity(settings.verbosity);
			spdlog::set_level(static_cast<spdlog::level::level_enum>(5 - settings.verbosity));
			emit updateTextSignal(QString("Verbosity level set to %1").arg(settings.verbosity));
			spdlog::info("Verbosity level set to {}", settings.verbosity);
			break;
		}
	}
}

void SuspendAllThreadsExceptCurrent() {
    DWORD currentThreadId = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32ThreadID != currentThreadId) {
                HANDLE threadHandle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (threadHandle) {
                    SuspendThread(threadHandle);
                    CloseHandle(threadHandle);
                }
            }
        } while (Thread32Next(snapshot, &te));
    }
    CloseHandle(snapshot);
}

void ResumeAllThreadsExceptCurrent() {
    DWORD currentThreadId = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32ThreadID != currentThreadId) {
                HANDLE threadHandle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (threadHandle) {
                    ResumeThread(threadHandle);  // Resume the thread
                    CloseHandle(threadHandle);
                }
            }
        } while (Thread32Next(snapshot, &te));
    }
    CloseHandle(snapshot);
}

void MainWindow::createMemoryDump() {
	QAction* action = qobject_cast<QAction*>(sender());
	MINIDUMP_TYPE dumpType = MiniDumpWithFullMemory;
	if (action == dumpActions[0]) {
		dumpType = MiniDumpNormal;
		emit updateTextSignal("Normal dump");
		spdlog::info("Normal dump");
	}
	else if (action == dumpActions[1]) {
		dumpType = (MINIDUMP_TYPE)(
			MiniDumpWithFullMemory |                 // Captures the full memory of the process
			MiniDumpWithDataSegs |                   // Includes static and global variables
			MiniDumpWithThreadInfo |                 // Includes detailed thread information
			MiniDumpWithFullMemoryInfo |             // Includes detailed memory region info
			MiniDumpWithHandleData                   // Includes handle information
			);
		//MiniDumpWithFullMemory;
		emit updateTextSignal("Full dump");
		spdlog::info("Full dump");
	}

	// Generate a unique dump file name
	SYSTEMTIME st;
	GetSystemTime(&st);
	char dumpFileName[128];
	sprintf_s(dumpFileName, "crash_dump_%04d%02d%02d_%02d%02d%02d.dmp",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	HANDLE dumpFile = CreateFileA(dumpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (dumpFile != INVALID_HANDLE_VALUE) {
		//	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
		//	dumpInfo.ThreadId = GetCurrentThreadId();
		//	dumpInfo.ExceptionPointers = NULL;
		//	dumpInfo.ClientPointers = FALSE;

		//	// Write the dump
		//	MiniDumpWriteDump(
		//		GetCurrentProcess(),
		//		GetCurrentProcessId(),
		//		dumpFile,
		//		dumpType,
		//		&dumpInfo,
		//		NULL,
		//		NULL);

		// Suspend all threads to ensure consistency
		SuspendAllThreadsExceptCurrent();

		//// Define dump type
		//MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
		//	MiniDumpWithFullMemory |
		//	MiniDumpWithDataSegs |
		//	MiniDumpWithThreadInfo |
		//	MiniDumpWithHandleData |
		//	MiniDumpWithFullMemoryInfo
		//	);

		// Write the dump
		BOOL success = MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			dumpFile,
			dumpType,
			NULL,  // No exception information
			NULL,  // No custom streams (yet)
			NULL
		);

		if (!success) {
			DWORD error = GetLastError();
			printf("MiniDumpWriteDump failed with error code: %lu\n", error);
		}

		// Resume all threads after dumping
		ResumeAllThreadsExceptCurrent();

		CloseHandle(dumpFile);
	}
	/// 
	printf("UnRAWer has crashed. A dump file has been saved as %s\n", dumpFileName);
};


void MainWindow::prntSettings(){
    printSettings(settings);
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
            //qDebug() << qPrintable(QString("Bit precision: %1 ").arg(actionMap[i].first));
			spdlog::info("Bit precision: {}", actionMap[i].first.toStdString());
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
        {"JPEG-XL",  5},
        {"HEIC",     6},
		{"PPM",      7}
	};

    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < frmtActions.size() && i < actionMap.size(); ++i) {
		if (action == frmtActions[i]) {
			settings.fileFormat = actionMap[i].second;
			emit updateTextSignal(QString("Output file format: %1 ").arg(actionMap[i].first));
			//qDebug() << qPrintable(QString("Output file format: %1 ").arg(actionMap[i].first));
			spdlog::info("Output file format: {}", actionMap[i].first.toStdString());
			break;
		}
	}
}

void MainWindow::zero() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == zero_raw) {
		spdlog::info("Zeroing RAW processing and disable demosaic:");
        settings.rawRot = settings.raw_rot[1];
		rawActions[1]->setChecked(true);
		spdlog::info("Camera Raw rotation set to 0 degree - Unrotatate/Horizontal");
		settings.dDemosaic = -2;
		demActions[0]->setChecked(true);
		spdlog::info("Demosaic set to RAW data");
        settings.rawSpace = 0;
		rclrActions[0]->setChecked(true);
		spdlog::info("RAW colorspace set to RAW");
		settings.denoise_mode = 0;
		denoiseActions[0]->setChecked(true);
		spdlog::info("Denoise set to Disabled");
		settings.rawParms.half_size = 0;
		halfSizeRaw->setChecked(false);
		spdlog::info("RAW half size set to 0 (full-size)");
		settings.rawParms.use_auto_wb = 0;
		auto_wb->setChecked(false);
		spdlog::info("Auto White Balance set to Disabled");
		settings.rawParms.use_camera_wb = 0;
		camera_wb->setChecked(false);
		spdlog::info("Camera White Balance set to Disabled");
		settings.rawParms.use_camera_matrix = 0;
		mtxActions[0]->setChecked(true);
		spdlog::info("Camera matrix set to Disabled");
		settings.rawParms.highlight = 1;
		hltActions[1]->setChecked(true);
		spdlog::info("Highlights set to Unclip");
        settings.fileFormat = settings.out_formats.size() - 1;
		frmtActions[settings.out_formats.size()]->setChecked(true);
		spdlog::info("Output file format set to PPM");
		settings.bitDepth = 1;
		bitActions[2]->setChecked(true);
		spdlog::info("Bit precision set to 16 bits int");
    }
    else if (action == zero_proc) {
        //qDebug() << "Zeroing Image processing";
		settings.crop_mode = 0;
		cropActions[0]->setChecked(true);
		//qDebug() << "Crop set to Disabled";
		settings.perCamera = 0;
		lut_exif->setChecked(false);
		spdlog::info("LUT per camera set to Disabled");
		settings.lutMode = -1;
		lutActions[0]->setChecked(true);
		spdlog::info("LUT transform set to Disabled");
		settings.sharp_mode = -1;
		sharpActions[0]->setChecked(true);
		spdlog::info("Sharpening set to Disabled");
    }
}

void MainWindow::autoWbSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == auto_wb) {
        settings.rawParms.use_auto_wb = auto_wb->isChecked();
		spdlog::info("Auto White Balance: {}", settings.rawParms.use_auto_wb ? "enabled" : "disabled");
    }
    else if (action == camera_wb) {
        settings.rawParms.use_camera_wb = camera_wb->isChecked();
		spdlog::info("Camera White Balance: {}", settings.rawParms.use_camera_wb ? "enabled" : "disabled");
    }
}

void MainWindow::mtxSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == mtxActions[0]) {
        settings.rawParms.use_camera_matrix = 0;
		spdlog::info("do not use embedded color profile matrix");
    }
    else if (action == mtxActions[1]) {
        settings.rawParms.use_camera_matrix = 1;
		spdlog::info("use embedded color profile matrix for DNG files");
    }
    else if (action == mtxActions[2]) {
        settings.rawParms.use_camera_matrix = 2;
		spdlog::info("always use embedded color data (if present)");
    }
};
void MainWindow::hltSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == hltActions[0]) {
        settings.rawParms.highlight = 0;
		spdlog::info("Clip Highlights");
    }
    else if (action == hltActions[1]) {
        settings.rawParms.highlight = 1;
		spdlog::info("Unclip Highlights");
    }
    else if (action == hltActions[2]) {
        settings.rawParms.highlight = 2;
		spdlog::info("Blend Highlights");
    }
    else if (action == hltActions[3]) {
        settings.rawParms.highlight = 3;
		spdlog::info("Rebuild Highlights");
    }
};

void MainWindow::rngSettings() {
	QAction* action = qobject_cast<QAction*>(sender());
    if (action == rngActions[0]) {
        settings.rangeMode = 0;
		emit updateTextSignal("Unsigned floats");
		spdlog::info("32/16 bit float Normals are in [0.0 ~ 1.0] range");
	}
    else if (action == rngActions[1]) {
        settings.rangeMode = 1;
		emit updateTextSignal("Signed floats");
		spdlog::info("32/16 bit floats Normals are in [-1.0 ~ 1.0] range");
	}
    else if (action == rngActions[2]) {
		settings.rangeMode = 2;
        emit updateTextSignal("Signed <> Unsigned");
		spdlog::info("32/16 bit floats Normals are in [-1.0 ~ 1.0] range converted to [0.0 ~ 1.0]");
    }
    else if (action == rngActions[3]) {
        settings.rangeMode = 3;
        emit updateTextSignal("Unsigned <> Signed");
		spdlog::info("32/16 bit floats Normals are in [0.0 ~ 1.0] range converted to [-1.0 ~ 1.0]");
    }
}

void MainWindow::rawSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    // -1 - Auto EXIF, 0 - Unrotated/Horisontal, 3 - 180 Horisontal, 5 - 90 CW Vertical, 6 - 90 CCW Vertical
	// const int raw_rot[5] = { -1, 0, 3, 5, 6 }; 0 -1; 1 - 0; 2 - 3; 3 - 5; 4 - 6
    if (action == rawActions[0]) {
        settings.rawRot = settings.raw_rot[0];
        emit updateTextSignal("Camera Raw rotation - Auto");
		spdlog::info("Camera Raw rotation set to EXIF Auto");
    }
    else if (action == rawActions[1]) {
        settings.rawRot = settings.raw_rot[1];
        emit updateTextSignal("Camera Raw rotation - 0 (Unrotate)");
		spdlog::info("Camera Raw rotation set to 0 degree - Unrotatate/Horizontal");
    }
    else if (action == rawActions[2]) {
		settings.rawRot = settings.raw_rot[2];
        emit updateTextSignal("Camera Raw rotation - 180 (Horisontal)");
		spdlog::info("Camera Raw rotation set to 180 degree (Horizontal)");
	}
    else if (action == rawActions[3]) {
		settings.rawRot = settings.raw_rot[3];
		emit updateTextSignal("Camera Raw rotation - 90 CCW (Vertical)");
		spdlog::info("Camera Raw rotation set to 90 degree CCW (Vertical)");
	}
    else if (action == rawActions[4]) {
		settings.rawRot = settings.raw_rot[4];
		emit updateTextSignal("Camera Raw rotation - 90 CW (Vertical)");
		spdlog::info("Camera Raw rotation set to 90 degree CW (Vertical)");
	}
}

void MainWindow::halfSizeSettings(bool checked) {

		settings.rawParms.half_size = checked ? 1 : 0;
		emit updateTextSignal(QString("Half size raw - %1 ").arg(checked ? "Enabled" : "Disabled"));
		spdlog::info("Half size raw - {}", checked ? "Enabled" : "Disabled");
}

void MainWindow::demSettings() {
    std::vector<std::pair<QString, int>> actionMap = { 
        // "raw data", "none", "linear", "VNG", "PPG", "AHD", "DCB", "", "", "", "", "", "", "DHT", "AAHD"
        {"raw data", -2}, {"none", -1}, {"linear", 0}, {"VNG", 2},
        {"PPG", 2},  {"AHD", 3},    {"DCB", 4}, 
        //{"", 5}, {"", 6}, {"", 7}, 
        //{"", 8}, {"", 9}, {"", 10},
        {"DHT", 11}, {"AAHD", 12}
    };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < demActions.size() && i < actionMap.size(); ++i) {
        if (action == demActions[i]) {
			settings.dDemosaic = actionMap[i].second;
			emit updateTextSignal(QString("Demosaic - %1 ").arg(actionMap[i].first));
			spdlog::info("Demosaic - {}", actionMap[i].first.toStdString());
			break;
		}
    }
}

void MainWindow::denoiseSettings() {
	std::vector<std::pair<QString, uint>> actionMap = { {"Disabled", 0}, {"Wavelet", 1}, {"FBDD", 2}, {"Both", 3} };
	QAction* action = qobject_cast<QAction*>(sender());
	for (int i = 0; i < denoiseActions.size() && i < actionMap.size(); ++i) {
		if (action == denoiseActions[i]) {
			settings.denoise_mode = actionMap[i].second;
			emit updateTextSignal(QString("Denoise - %1 ").arg(actionMap[i].first));
			spdlog::info("Denoise - {}", actionMap[i].first.toStdString());
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
			spdlog::info("Raw color space - {}", actionMap[i].first.toStdString());
        }
    }
}

void MainWindow::cropSettings() {
	std::vector<std::pair<QString, int>> actionMap = { {"Disabled", -1}, {"Auto", 0}, {"Forced", 1} };

	QAction* action = qobject_cast<QAction*>(sender());
	for (int i = 0; i < cropActions.size() && i < actionMap.size(); ++i) {
		if (action == cropActions[i]) {
			settings.crop_mode = actionMap[i].second;
			emit updateTextSignal(QString("Crop - %1 ").arg(actionMap[i].first));
			spdlog::info("Crop - {}", actionMap[i].first.toStdString());
			break;
		}
	}
}

void MainWindow::lutCameraSettings() {
	QAction* action = qobject_cast<QAction*>(sender());
	settings.perCamera = action->isChecked();
	emit updateTextSignal(QString("LUT per camera - %1").arg(settings.perCamera ? "On" : "Off"));
	qDebug() << qPrintable(QString("Per Camera Model LUT - %1").arg(settings.perCamera ? "On" : "Off"));
    if (settings.perCamera) {
		spdlog::info("LUT will be load as path_to/lut_preset_Make_Model.csp");
	}
	else {
		spdlog::info("LUT will be load as path_to/lut_preset.csp");
	}
}

void MainWindow::lutSettings() {
    std::vector<std::pair<QString, int>> actionMap = { {"Off", -1}, {"Smart", 0}, {"Forced", 1} };

    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < lutActions.size() && i < actionMap.size(); ++i) {
		if (action == lutActions[i]) {
			settings.lutMode = actionMap[i].second;
			emit updateTextSignal(QString("LUT transform: %1 ").arg(actionMap[i].first));
			spdlog::info("LUT transform: {}", actionMap[i].first.toStdString());
			break;
		}
	}
}

void MainWindow::lutPSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < lutPActions.size() && i < settings.lut_Preset.size(); ++i) {
        if (action == lutPActions[i]) {
            settings.dLutPreset = lutPActions[i]->text().toStdString();
            emit updateTextSignal(QString("LUT preset: %1 ").arg(settings.dLutPreset.c_str()));
			spdlog::info("LUT preset: {}", settings.dLutPreset);
        }
    }
}

void MainWindow::sharpSettings() {
    std::vector<std::pair<QString, int>> actionMap = { {"Off", -1}, {"Smart", 0}, {"Forced", 1} };

    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < sharpActions.size() && i < actionMap.size(); ++i) {
        if (action == sharpActions[i]) {
			settings.sharp_mode = actionMap[i].second;
			emit updateTextSignal(QString("Unsharp - %1 ").arg(actionMap[i].first));
			spdlog::info("Unsharp - {}", actionMap[i].first.toStdString());
			break;
		}
	}
}

void MainWindow::sharpKSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    uint s = sizeof(settings.sharp_kerns) / sizeof(settings.sharp_kerns[0]);
    uint a = sharpKActions.size();
    for (int i = 0; i < a && i < s ; ++i) {
        if (action == sharpKActions[i]) {
			settings.sharp_kernel = i;
            QString kernel = settings.sharp_kerns[settings.sharp_kernel].c_str();
			emit updateTextSignal(QString("Unsharp kernel: %1 ").arg(kernel));
			spdlog::info("Unsharp kernel: {}", kernel.toStdString());
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
	spdlog::info("Use subfolders - {}", checked ? "On" : "Off");
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
		spdlog::error("Can not load [unrw_config.toml] Using default settings.");
        settings.reSettings();
    }
    printSettings(settings);
}

void MainWindow::setPBarColor(QProgressBar* progressBar, const QColor& color) {
    QString style = QString(
        "QProgressBar {"
        "border: 0px solid black; border-radius: 3px; background-color: black; color: white;"
        "margin-bottom: 4px;"
        "}"
        "QProgressBar::chunk {background-color: %1;}"
    ).arg(color.name());

    progressBar->setStyleSheet(style);
}