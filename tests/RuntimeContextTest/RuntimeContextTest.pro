QT += testlib core gui sql network serialport
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_runtimecontext

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_runtimecontext.cpp \
    test_stubs.cpp \
    ../../mcpbridge/RuntimeProvider.cpp \
    ../../mcpbridge/RuntimeContext.cpp \
    ../../rig/RigRuntimeProvider.cpp \
    ../../rotator/RotatorRuntimeProvider.cpp \
    ../../data/StationProfile.cpp \
    ../../data/StationRuntimeProvider.cpp \
    ../../data/AntProfile.cpp \
    ../../data/AntennaRuntimeProvider.cpp \
    ../../data/ActivityProfile.cpp \
    ../../data/ActivityRuntimeProvider.cpp \
    ../../data/MainLayoutProfile.cpp \
    ../../data/SerialPort.cpp \
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
    ../../data/ActivityProfile.h \
    ../../data/ActivityRuntimeProvider.h \
    ../../data/RigProfile.h \
    ../../data/MainLayoutProfile.h \
    ../../data/SerialPort.h \
    ../../data/ProfileManager.h
