#-------------------------------------------------
#
# Project created by QtCreator 2018-05-11T16:03:50
#
#-------------------------------------------------

QT       -= core gui

TARGET = alsa_lib
TEMPLATE = lib

DEFINES += ALSA_LIB_LIBRARY

SOURCES += alsa_lib.cpp

HEADERS += alsa_lib.h\
        alsa_lib_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
