QT += testlib core sql network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_dxcclogstatus

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_dxcclogstatus.cpp \
    ../BandmapGuideTest/test_stubs.cpp \
    ../../core/AdifRecovery.cpp \
    ../../core/LogParam.cpp \
    ../../data/BandPlan.cpp \
    ../../data/DxccLogStatusCondition.cpp

HEADERS += \
    ../../core/AdifRecovery.h \
    ../../core/LogParam.h \
    ../../data/DxccLogStatusCondition.h
