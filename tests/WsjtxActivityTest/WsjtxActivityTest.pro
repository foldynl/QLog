QT += testlib core

CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_wsjtxactivity

INCLUDEPATH += $$PWD/../..

SOURCES += \
    $$PWD/tst_wsjtxactivity.cpp \
    $$PWD/../../models/WsjtxActivity.cpp

HEADERS += \
    $$PWD/../../models/WsjtxActivity.h
