QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_adifrecovery

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_adifrecovery.cpp \
    ../../core/AdifRecovery.cpp

HEADERS += \
    ../../core/AdifRecovery.h
