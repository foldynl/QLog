QT += testlib core sql network serialport
QT -= gui
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_rotatorreporting

INCLUDEPATH += $$PWD/../..
unix: LIBS += -L/usr/local/lib -lhamlib
win32: LIBS += -llibhamlib-4

SOURCES += \
    tst_rotatorreporting.cpp \
    ../../mcpbridge/RuntimeProvider.cpp \
    ../../mcpbridge/RuntimeContext.cpp \
    ../../rotator/Rotator.cpp \
    ../../rotator/RotatorRuntimeProvider.cpp \
    ../../rotator/drivers/GenericRotDrv.cpp \
    ../../rotator/drivers/HamlibRotDrv.cpp \
    ../../rotator/drivers/PSTRotDrv.cpp \
    ../../rotator/RotCaps.cpp \
    ../../data/AntProfile.cpp \
    ../../data/AntennaRuntimeProvider.cpp \
    ../../data/RotProfile.cpp \
    ../../data/SerialPort.cpp

HEADERS += \
    ../../mcpbridge/RuntimeProvider.h \
    ../../mcpbridge/RuntimeContext.h \
    ../../rotator/Rotator.h \
    ../../rotator/RotatorRuntimeProvider.h \
    ../../rotator/drivers/GenericRotDrv.h \
    ../../rotator/drivers/PSTRotDrv.h \
    ../../data/AntProfile.h \
    ../../data/AntennaRuntimeProvider.h \
    ../../data/RotProfile.h \
    ../../data/ProfileManager.h \
    ../../data/SerialPort.h
