QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_qsofiltermanager

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_qsofiltermanager.cpp \
    ../../core/QSOFilterManager.cpp \
    ../../models/SqlListModel.cpp

HEADERS += \
    ../../core/QSOFilterManager.h \
    ../../models/SqlListModel.h
