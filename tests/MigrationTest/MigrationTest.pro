QT += testlib sql core widgets network xml
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_migration
DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

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
    UnusedImportStubs.cpp \
    ../../core/Migration.cpp \
    ../../core/LogDatabase.cpp \
    ../../core/DatabaseBackup.cpp \
    ../../core/LogParam.cpp \
    ../../core/AdifRecovery.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../data/BandPlan.cpp \
    ../../data/DxccLogStatusCondition.cpp \
    ../../logformat/AdiFormat.cpp \
    ../../logformat/AdxFormat.cpp

RESOURCES += \
    ../../res/res.qrc

HEADERS += \
    ../../core/Migration.h \
    ../../core/CredentialStore.h \
    ../../core/AdifRecovery.h \
    ../../core/LogLocale.h \
    ../../core/DatabaseBackup.h \
    ../../core/LogParam.h \
    ../../data/Data.h \
    ../../data/DxccLogStatusCondition.h \
    ../../logformat/LogFormat.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/AdxFormat.h
