QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_appguard
INCLUDEPATH += $$PWD/../..
SOURCES += tst_appguard.cpp ../../core/AppGuard.cpp
HEADERS += ../../core/AppGuard.h
unix:!macx:lessThan(QT_MAJOR_VERSION, 6) {
    DEFINES += GUARD_CREATION_WRAPPER
    QMAKE_LFLAGS += -Wl,--wrap=_ZN13QSharedMemory6attachENS_10AccessModeE
}
