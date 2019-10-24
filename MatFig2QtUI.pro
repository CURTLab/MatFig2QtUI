# CURTLab
# University of Applied Sciences Upper Austria
# School of Medical Engineering and Applied Social Sciences
# Garnisonstraße 21, 4020 Linz, Austria
#
# MatFig2QtUI
# Tool to convert Matlab Fig files to Qt UI form files
#
# GNU GENERAL PUBLIC LICENSE Version 3
#
# Written by Fabian Hauser 2019, fabian.hauser@fh-linz.at

# Icons from https://www.fatcow.com/free-icons
# under the CC BY 3.0 US LICENSE
# https://creativecommons.org/licenses/by/3.0/us/

QT       += widgets core gui uitools designer

TARGET = MatFig2QtUI
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++1z

include(libmatio/libMatIO.pri)

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    QConvertFig.cpp \
    QMatIO.cpp \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    MainWindow.h \
    QConvertFig.h \
    QMatIO.h

FORMS += \
    MainWindow.ui

RESOURCES += \
    resources/res.qrc
