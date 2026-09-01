QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_cabrilloformat

DEFINES += VERSION=\"test\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_cabrilloformat.cpp \
    test_stubs.cpp \
    ../../core/LogLocale.cpp \
    ../../data/BandPlan.cpp \
    ../../logformat/CabrilloFormat.cpp

HEADERS += \
    ../../core/LogLocale.h \
    ../../data/Band.h \
    ../../data/BandPlan.h \
    ../../data/Data.h \
    ../../logformat/CabrilloFormat.h \
    ../../logformat/LogFormat.h
