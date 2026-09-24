QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_dxccconfirmation

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_dxccconfirmation.cpp \
    ../../data/DxccConfirmation.cpp \
    ../../data/BandPlan.cpp

HEADERS += \
    ../../data/DxccConfirmation.h \
    ../../data/Dxcc.h \
    ../../data/BandPlan.h \
    ../../core/QuadKeyCache.h
