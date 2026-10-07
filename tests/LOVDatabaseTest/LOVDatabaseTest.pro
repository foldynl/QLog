QT += testlib core gui sql xml network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_lovdatabase
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../..
!win32-msvc: QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
unix:!macx: QMAKE_LFLAGS += -Wl,--gc-sections
win32-g++: QMAKE_LFLAGS += -Wl,--gc-sections
macx: QMAKE_LFLAGS += -Wl,-dead_strip
win32-msvc {
    QMAKE_CXXFLAGS += /Gy /Gw
    QMAKE_LFLAGS += /OPT:REF
}
SOURCES += tst_lovdatabase.cpp \
    ../AdxFormatTest/test_stubs.cpp \
    ../../core/LOVDatabase.cpp \
    ../../core/LOVDownloader.cpp \
    ../../core/LogParam.cpp \
    ../../core/FileCompressor.cpp
HEADERS += ../../core/LOVDatabase.h \
    ../../core/LOVDownloader.h \
    ../../core/LogParam.h \
    ../../data/Data.h \
    ../../logformat/LogFormat.h
RESOURCES += lov_fixtures.qrc
!isEmpty(ZLIBINCLUDEPATH): INCLUDEPATH += $$ZLIBINCLUDEPATH
!isEmpty(ZLIBLIBPATH): LIBS += -L$$ZLIBLIBPATH
unix: LIBS += -lz
win32: LIBS += -lzlib
