QT += testlib core gui sql network serialport websockets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_rigreporting

INCLUDEPATH += $$PWD/../..
!isEmpty(HAMLIBINCLUDEPATH): INCLUDEPATH += $$HAMLIBINCLUDEPATH
!isEmpty(PTHREADINCLUDEPATH): INCLUDEPATH += $$PTHREADINCLUDEPATH
!isEmpty(HAMLIBLIBPATH): LIBS += -L$$HAMLIBLIBPATH
!isEmpty(PTHREADLIBPATH): LIBS += -L$$PTHREADLIBPATH
unix: LIBS += -L/usr/local/lib -lhamlib
win32: LIBS += -llibhamlib-4

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
