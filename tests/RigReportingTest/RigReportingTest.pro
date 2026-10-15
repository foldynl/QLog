QT += testlib core gui sql network serialport websockets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_rigreporting

INCLUDEPATH += $$PWD/../..
include(../hamlib.pri)

SOURCES += \
    tst_rigreporting.cpp \
    test_stubs.cpp \
    ../../rig/RigCaps.cpp \
    ../../rig/drivers/GenericRigDrv.cpp \
    ../../rig/drivers/FlrigRigDrv.cpp \
    ../../rig/drivers/TCIRigDrv.cpp \
    ../../rig/drivers/HamlibRigDrv.cpp \
    ../../data/SerialPort.cpp

HEADERS += \
    ../../rig/drivers/GenericRigDrv.h \
    ../../rig/drivers/FlrigRigDrv.h \
    ../../rig/drivers/TCIRigDrv.h \
    ../../data/SerialPort.h
