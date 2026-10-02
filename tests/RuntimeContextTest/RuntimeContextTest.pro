QT += testlib core sql network serialport
QT -= gui
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_runtimecontext

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_runtimecontext.cpp \
    ../../mcpbridge/RuntimeProvider.cpp \
    ../../mcpbridge/RuntimeContext.cpp \
    ../../rig/RigRuntimeProvider.cpp \
    ../../rotator/RotatorRuntimeProvider.cpp \
    ../../data/StationProfile.cpp \
    ../../data/StationRuntimeProvider.cpp \
    ../../data/AntProfile.cpp \
    ../../data/AntennaRuntimeProvider.cpp \
    ../../data/Gridsquare.cpp \
    ../../core/LogLocale.cpp

HEADERS += \
    ../../mcpbridge/RuntimeProvider.h \
    ../../mcpbridge/RuntimeContext.h \
    ../../rig/RigRuntimeProvider.h \
    ../../rotator/RotatorRuntimeProvider.h \
    ../../data/StationProfile.h \
    ../../data/StationRuntimeProvider.h \
    ../../data/AntProfile.h \
    ../../data/AntennaRuntimeProvider.h \
    ../../data/ProfileManager.h
