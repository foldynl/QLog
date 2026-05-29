QT += testlib core sql xml
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_adxformat

DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_adxformat.cpp \
    test_stubs.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../logformat/AdxFormat.cpp \
    ../../logformat/AdiFormat.cpp

HEADERS += \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../logformat/AdxFormat.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h
