QT += testlib core gui sql xml network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_databasebackup
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../..
win32 {
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite
    SOURCES += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.c
    HEADERS += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.h
} else {
    LIBS += -lsqlite3
}
SOURCES += tst_databasebackup.cpp \
    DatabaseBackupTestStubs.cpp \
    ../MigrationTest/UnusedImportStubs.cpp \
    ../AdxFormatTest/AdxFormatTestStubs.cpp \
    ../../core/DatabaseBackup.cpp \
    ../../core/LogDatabase.cpp \
    ../../core/LogParam.cpp \
    ../../core/LogLocale.cpp \
    ../../core/AdifRecovery.cpp \
    ../../data/Accents.cpp \
    ../../data/BandPlan.cpp \
    ../../data/DxccLogStatusCondition.cpp \
    ../../logformat/AdxFormat.cpp \
    ../../logformat/AdiFormat.cpp
HEADERS += ../../core/DatabaseBackup.h \
    ../../core/CredentialStore.h \
    ../../core/Migration.h \
    ../../core/AdifRecovery.h \
    ../../core/LogParam.h \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../data/BandPlan.h \
    ../../data/DxccLogStatusCondition.h \
    ../../logformat/AdxFormat.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h
