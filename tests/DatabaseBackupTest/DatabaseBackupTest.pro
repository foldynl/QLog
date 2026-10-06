QT += testlib core gui sql xml network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_databasebackup
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../..
# Only the database and parameter paths used by this suite are linked.
!win32-msvc: QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
unix:!macx: QMAKE_LFLAGS += -Wl,--gc-sections
win32-g++: QMAKE_LFLAGS += -Wl,--gc-sections
macx: QMAKE_LFLAGS += -Wl,-dead_strip
win32-msvc {
    QMAKE_CXXFLAGS += /Gy /Gw
    QMAKE_LFLAGS += /OPT:REF
}
LIBS += -lsqlite3
SOURCES += tst_databasebackup.cpp \
    ../AdxFormatTest/test_stubs.cpp \
    ../../core/DatabaseBackup.cpp \
    ../../core/LogDatabase.cpp \
    ../../core/LogParam.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../logformat/AdxFormat.cpp \
    ../../logformat/AdiFormat.cpp
HEADERS += ../../core/DatabaseBackup.h \
    ../../core/LogParam.h \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../logformat/AdxFormat.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h
