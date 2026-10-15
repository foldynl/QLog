QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_appguard
INCLUDEPATH += $$PWD/../..
SOURCES += tst_appguard.cpp ../../core/AppGuard.cpp
HEADERS += ../../core/AppGuard.h
