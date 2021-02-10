QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    execution_timer.cpp \
    ffmpeg.cpp \
    findsound.cpp \
    main.cpp \
    mainwindow.cpp \
    misc_util.cpp \
    signals.cpp \
    videolistitem.cpp

HEADERS += \
    cute_files.h \
    execution_timer.h \
    ffmpeg.h \
    findsound.h \
    mainwindow.h \
    misc_util.h \
    signals.h \
    videolistitem.h

FORMS += \
    mainwindow.ui \
    videolistitem.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_CXXFLAGS+=-openmp

win32: LIBS += -L$$PWD/third_party/ffmpeg/ -lavcodec -lavformat -lavutil -lswresample -lswscale

INCLUDEPATH += $$PWD/third_party/ffmpeg
DEPENDPATH += $$PWD/third_party/ffmpeg

win32: LIBS += -L$$PWD/third_party/fftw/lib/ -llibfftw3f-3

INCLUDEPATH += $$PWD/third_party/fftw/lib
DEPENDPATH += $$PWD/third_party/fftw/lib

copydata.commands = $(COPY_DIR) $$shell_quote($$shell_path($$PWD/third_party/bin)) $$shell_quote($$shell_path($$OUT_PWD))
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata
