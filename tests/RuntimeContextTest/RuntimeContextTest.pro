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
    ../../rig/RigRuntimeProvider.cpp

HEADERS += \
    ../../mcpbridge/RuntimeProvider.h \
    ../../mcpbridge/RuntimeContext.h \
    ../../rig/RigRuntimeProvider.h
