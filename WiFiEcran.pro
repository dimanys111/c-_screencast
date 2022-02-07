#-------------------------------------------------
#
# Project created by QtCreator 2018-08-28T21:11:39
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = WiFiEcran
TEMPLATE = app

win32 {
    INCLUDEPATH += ../../FFmpeg-master
}

win32 {
    LIBS += -lgdi32 -LD:\Qt\Proj\WiFiEcran\WiFiEcran\64 -lavcodec -lavutil \
    -lavformat -lswscale
}
unix {
    LIBS +=  -lavcodec -lavutil -lavformat -lavdevice -lX11 -lXext -lasound -lswscale
}

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    stream.cpp

HEADERS += \
        mainwindow.h \
    stream.h

FORMS += \
        mainwindow.ui

