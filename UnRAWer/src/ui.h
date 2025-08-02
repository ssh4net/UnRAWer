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

#include "process.h"
#include <QtCore/QFutureWatcher>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtGui/QDropEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>


#define VERSION_MAJOR 1
#define VERSION_MINOR 64
#define VERSION_PATCH 0

class DropArea : public QLabel {
    Q_OBJECT  // Macro needed to handle signals and slots

public:
    DropArea();

signals:
    void filesDropped(QList<QUrl> urls);  // New signal to be emitted when files are dropped

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT  // Macro needed to handle signals and slots
public:
    MainWindow();
    //explicit MainWindow(const QStringList& args, QWidget* parent = nullptr);
    virtual void emitUpdateTextSignal(const QString& text) { emit updateTextSignal(text); }  // Public method to emit the signal
	void setPBarColor(QProgressBar* progressBar, const QColor& color = QColor("#05B8CC"));

signals:
    void updateTextSignal(const QString& text);
    void changeProgressBarColorSignal(QColor color);

public slots:
    void setProgressBarValueSlot(int value) { progressBar->setValue(value); }
    void changeProgressBarColorSlot(const QColor& color) { setPBarColor(progressBar, color); }

private slots:
    void restartApp();
    void reloadConfig();
    
    void toggleConsole(bool checked);
    void verbLevel();
	//void createMemoryDump();

    void toggleSubfldr(bool checked);
    void startProcessing(QList<QUrl> urls);
    void rngSettings();
    void frmtSettings();
    void bitSettings();
    
    void zero();
	void autoWbSettings();
    void mtxSettings();
    void hltSettings();

    void rawSettings();
    void halfSizeSettings(bool checked);
    void demSettings();
    void rclrSettings();
    void cropSettings();
    void lutCameraSettings();
    void lutSettings();
    void lutPSettings();
    void denoiseSettings();
    void sharpSettings();
    void sharpKSettings();

    void prntSettings();

private:
    QStringList commandLineArgs;

    QFutureWatcher<bool> processingWatcher;
    QProgressBar* progressBar;

    QAction* lut_exif;
    QAction* halfSizeRaw;
    QAction* con_enable;
    QAction* auto_wb;
    QAction* camera_wb;
    QAction* zero_raw;
    QAction* zero_proc;

    QList<QAction*> verbActions;
	QList<QAction*> dumpActions;

    QList<QAction*> rngActions;
    QList<QAction*> demActions;
    QList<QAction*> frmtActions;
    QList<QAction*> bitActions;
    QList<QAction*> rawActions;

	QList<QAction*> mtxActions;
    QList<QAction*> hltActions;

	QList<QAction*> cropActions;
    QList<QAction*> lutActions;
    QList<QAction*> lutPActions;
    QList<QAction*> denoiseActions;
    QList<QAction*> sharpActions;
    QList<QAction*> sharpKActions;
    QList<QAction*> rclrActions;
};


class DummyWindow : public MainWindow {
    Q_OBJECT  // Macro needed to handle signals and slots
public:
    DummyWindow() : MainWindow() {};
    void emitUpdateTextSignal(const QString& text) override {};  // Public method to emit the signal
};

class DummyProgressBar : public QProgressBar {
    Q_OBJECT  // Macro needed to handle signals and slots
public:
    DummyProgressBar() : QProgressBar() {};
};