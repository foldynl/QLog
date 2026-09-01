QT += core gui sql widgets testlib

CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_freqqspinbox

INCLUDEPATH += ../..

SOURCES += \
    tst_freqqspinbox.cpp \
    ../../data/BandPlan.cpp \
    ../../ui/component/BaseDoubleSpinBox.cpp \
    ../../ui/component/FreqQSpinBox.cpp

HEADERS += \
    ../../data/Band.h \
    ../../data/BandPlan.h \
    ../../ui/component/BaseDoubleSpinBox.h \
    ../../ui/component/FreqQSpinBox.h
