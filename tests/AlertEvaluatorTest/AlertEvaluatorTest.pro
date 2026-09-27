QT += testlib core sql network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_alertevaluator

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_alertevaluator.cpp \
    ../../core/AlertEvaluator.cpp \
    ../../data/BandPlan.cpp \
    ../../notification/AlertNotification.cpp

HEADERS += \
    ../../core/AlertEvaluator.h \
    ../../data/DxSpot.h \
    ../../data/WsjtxEntry.h \
    ../../data/SpotAlert.h \
    ../../notification/DesktopNotificationCategory.h \
    ../../notification/AlertNotification.h \
    ../../data/BandPlan.h
