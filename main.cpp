#include "mainwindow.h"
#include "misc_util.h"
#include "ffmpeg.h"
#include "videolistitem.h"
#include "findsound.h"

#include <QApplication>
#include <vector>
#include <fstream>
#include <chrono>
#include <iostream>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    qRegisterMetaType<Image>();
    qRegisterMetaType<FileSignal>();
    qRegisterMetaType<FindSoundResult>();

    MainWindow w;
    w.show();

    return a.exec();
}
