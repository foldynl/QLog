QT += testlib core gui sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_bandmapguide

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_bandmapguide.cpp \
    test_stubs.cpp \
    ../../data/BandmapGuide.cpp \
    ../../data/BandPlan.cpp \
    ../../core/AdifRecovery.cpp \
    ../../core/LogParam.cpp

HEADERS += \
    ../../data/BandmapGuide.h \
    ../../data/BandPlan.h \
    ../../data/Band.h \
    ../../core/AdifRecovery.h \
    ../../core/LogParam.h
