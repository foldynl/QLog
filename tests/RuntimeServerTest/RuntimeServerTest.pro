QT += testlib core network
QT -= gui
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_runtimeserver

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_runtimeserver.cpp \
    ../../mcpbridge/RuntimeProvider.cpp \
    ../../mcpbridge/RuntimeContext.cpp \
    ../../mcpbridge/RuntimeServer.cpp

HEADERS += \
    ../../mcpbridge/RuntimeProvider.h \
    ../../mcpbridge/RuntimeContext.h \
    ../../mcpbridge/RuntimeServer.h
