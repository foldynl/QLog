QT += core sql testlib
QT -= gui
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_macroexpander
INCLUDEPATH += $$PWD/../..
SOURCES += tst_macroexpander.cpp ../../core/MacroExpander.cpp
HEADERS += ../../core/MacroExpander.h
