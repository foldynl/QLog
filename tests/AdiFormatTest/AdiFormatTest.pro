QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_adiformat

DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_adiformat.cpp \
    test_stubs.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../logformat/AdiFormat.cpp

HEADERS += \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h
