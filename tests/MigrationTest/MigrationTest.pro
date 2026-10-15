QT += testlib sql core widgets network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_migration

INCLUDEPATH += $$PWD/../..

# Keep unrelated database import and backup export paths out of this test.
!win32-msvc: QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
unix:!macx: QMAKE_LFLAGS += -Wl,--gc-sections
win32-g++: QMAKE_LFLAGS += -Wl,--gc-sections
macx: QMAKE_LFLAGS += -Wl,-dead_strip
win32-msvc {
    QMAKE_CXXFLAGS += /Gy /Gw
    QMAKE_LFLAGS += /OPT:REF
}
win32 {
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite
    SOURCES += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.c
    HEADERS += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.h
} else {
    LIBS += -lsqlite3
}

SOURCES += \
    tst_migration.cpp \
    test_stubs.cpp \
    ../../core/Migration.cpp \
    ../../core/LogDatabase.cpp \
    ../../core/DatabaseBackup.cpp \
    ../../core/LogParam.cpp \
    ../../data/Accents.cpp \
    ../../data/BandPlan.cpp

RESOURCES += \
    ../../res/res.qrc

HEADERS += \
    ../../core/Migration.h \
    ../../core/DatabaseBackup.h \
    ../../core/LogParam.h \
    ../../data/Data.h
