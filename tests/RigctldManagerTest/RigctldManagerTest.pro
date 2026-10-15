QT += testlib core network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_rigctldmanager

# Local stubs first, then project includes
INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_rigctldmanager.cpp \
    ../../rig/RigctldManager.cpp \
    ../../data/SerialPort.cpp \
    test_stubs.cpp

HEADERS += \
    ../../rig/RigctldManager.h \
    ../../data/SerialPort.h \
    data/RigProfile.h \
    core/debug.h

include(../hamlib.pri)
