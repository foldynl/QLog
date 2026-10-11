QT += testlib core widgets sql network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_databaseimport
INCLUDEPATH += $$PWD/../..
# Link only the inspection path, following DatabaseBackupTest.
!win32-msvc: QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
unix:!macx: QMAKE_LFLAGS += -Wl,--gc-sections
win32-g++: QMAKE_LFLAGS += -Wl,--gc-sections
macx: QMAKE_LFLAGS += -Wl,-dead_strip
win32-msvc {
    QMAKE_CXXFLAGS += /Gy /Gw
    QMAKE_LFLAGS += /OPT:REF
}
LIBS += -lsqlite3
SOURCES += tst_databaseimport.cpp ../../core/LogDatabase.cpp ../../core/PlatformParameterManager.cpp
HEADERS += ../../core/LogDatabase.h
