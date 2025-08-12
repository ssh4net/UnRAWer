/********************************************************************************
** Form generated from reading UI file 'gui.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GUI_H
#define UI_GUI_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindowClass
{
public:
    QAction *actionRestart;
    QAction *actionExit;
    QWidget *centralWidget;
    QWidget *DropArea;
    QLabel *label;
    QProgressBar *progressBar;
    QPlainTextEdit *plainTextEdit;
    QMenuBar *menuBar;
    QMenu *menuFiles;
    QMenu *menuReset;
    QToolBar *mainToolBar;

    void setupUi(QMainWindow *MainWindowClass)
    {
        if (MainWindowClass->objectName().isEmpty())
            MainWindowClass->setObjectName("MainWindowClass");
        MainWindowClass->resize(700, 500);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(MainWindowClass->sizePolicy().hasHeightForWidth());
        MainWindowClass->setSizePolicy(sizePolicy);
        MainWindowClass->setMinimumSize(QSize(700, 500));
        MainWindowClass->setMaximumSize(QSize(700, 500));
        MainWindowClass->setBaseSize(QSize(700, 500));
        actionRestart = new QAction(MainWindowClass);
        actionRestart->setObjectName("actionRestart");
        actionExit = new QAction(MainWindowClass);
        actionExit->setObjectName("actionExit");
        centralWidget = new QWidget(MainWindowClass);
        centralWidget->setObjectName("centralWidget");
        DropArea = new QWidget(centralWidget);
        DropArea->setObjectName("DropArea");
        DropArea->setGeometry(QRect(10, 10, 480, 361));
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(DropArea->sizePolicy().hasHeightForWidth());
        DropArea->setSizePolicy(sizePolicy1);
        DropArea->setMinimumSize(QSize(480, 361));
        DropArea->setMaximumSize(QSize(480, 1440));
        DropArea->setStyleSheet(QString::fromUtf8("border-radius: 3px; background-color: #E0E0E0; margin-bottom: 4px;"));
        label = new QLabel(DropArea);
        label->setObjectName("label");
        label->setGeometry(QRect(0, 0, 481, 361));
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy2);
        QFont font;
        font.setPointSize(16);
        label->setFont(font);
        label->setAlignment(Qt::AlignCenter);
        progressBar = new QProgressBar(centralWidget);
        progressBar->setObjectName("progressBar");
        progressBar->setGeometry(QRect(10, 380, 480, 20));
        sizePolicy.setHeightForWidth(progressBar->sizePolicy().hasHeightForWidth());
        progressBar->setSizePolicy(sizePolicy);
        progressBar->setStyleSheet(QString::fromUtf8("QProgressBar {border: 0px solid black; border-radius: 3px; background-color: white; color: black; margin-bottom: 4px;}\n"
"QProgressBar::chunk {background-color: #05B8CC;}"));
        progressBar->setValue(24);
        progressBar->setTextVisible(false);
        plainTextEdit = new QPlainTextEdit(centralWidget);
        plainTextEdit->setObjectName("plainTextEdit");
        plainTextEdit->setGeometry(QRect(10, 410, 680, 51));
        plainTextEdit->setMaximumSize(QSize(680, 16777215));
        plainTextEdit->setStyleSheet(QString::fromUtf8("border-radius: 3px; background-color: #E0E0E0; padding-left: 4px; padding-right: 4px;"));
        MainWindowClass->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindowClass);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 700, 21));
        menuFiles = new QMenu(menuBar);
        menuFiles->setObjectName("menuFiles");
        menuReset = new QMenu(menuBar);
        menuReset->setObjectName("menuReset");
        MainWindowClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(MainWindowClass);
        mainToolBar->setObjectName("mainToolBar");
        MainWindowClass->addToolBar(Qt::ToolBarArea::TopToolBarArea, mainToolBar);

        menuBar->addAction(menuFiles->menuAction());
        menuBar->addAction(menuReset->menuAction());
        menuFiles->addAction(actionExit);
        menuReset->addAction(actionRestart);

        retranslateUi(MainWindowClass);

        QMetaObject::connectSlotsByName(MainWindowClass);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindowClass)
    {
        MainWindowClass->setWindowTitle(QCoreApplication::translate("MainWindowClass", "MainWindow", nullptr));
        actionRestart->setText(QCoreApplication::translate("MainWindowClass", "Restart", nullptr));
        actionExit->setText(QCoreApplication::translate("MainWindowClass", "Exit", nullptr));
        label->setText(QCoreApplication::translate("MainWindowClass", "Drag & drop files here", nullptr));
        menuFiles->setTitle(QCoreApplication::translate("MainWindowClass", "Files", nullptr));
        menuReset->setTitle(QCoreApplication::translate("MainWindowClass", "Reset", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindowClass: public Ui_MainWindowClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GUI_H
