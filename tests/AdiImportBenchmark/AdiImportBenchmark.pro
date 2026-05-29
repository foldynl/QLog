QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_adiimportbenchmark

DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_adiimportbenchmark.cpp \
    ../AdiFormatTest/test_stubs.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../logformat/AdiFormat.cpp

HEADERS += \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h

RESOURCES += \
    ../../res/res.qrc
