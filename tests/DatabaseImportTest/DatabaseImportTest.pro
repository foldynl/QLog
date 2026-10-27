QT += testlib core widgets sql network xml
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_databaseimport
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../CredentialStoreImportTest/fakes $$PWD/../..
unix:!macx {
    DEFINES += IMPORT_INTERRUPTION_WRAPPERS
    QMAKE_LFLAGS += -Wl,--wrap=_ZN5QFile6renameERK7QStringS2_ \
                   -Wl,--wrap=_ZN5QFile6renameERK7QString \
                   -Wl,--wrap=_ZN9QSaveFile6commitEv \
                   -Wl,--wrap=_ZN5QFile6removeERK7QString \
                   -Wl,--wrap=_ZN5QFile6removeEv
}
win32 {
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite
    SOURCES += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.c
    HEADERS += $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.h
} else {
    LIBS += -lsqlite3
}
SOURCES += tst_databaseimport.cpp \
    ../MigrationTest/MigrationTestStubs.cpp \
    ../../core/LogDatabase.cpp ../../core/PlatformParameterManager.cpp \
    ../../core/Migration.cpp ../../core/DatabaseBackup.cpp \
    ../../core/LogParam.cpp ../../core/CredentialStore.cpp ../../core/PasswordCipher.cpp \
    ../../core/LOVDatabase.cpp ../../core/LOVDownloader.cpp \
    ../../core/AdifRecovery.cpp ../../core/LogLocale.cpp ../../core/FileCompressor.cpp \
    ../../data/BandPlan.cpp ../../data/Accents.cpp ../../data/DxccLogStatusCondition.cpp \
    ../../logformat/AdiFormat.cpp ../../logformat/AdxFormat.cpp
HEADERS += ../../core/LogDatabase.h ../../core/Migration.h ../../core/DatabaseBackup.h \
    ../../core/LogParam.h ../../core/CredentialStore.h ../../core/PasswordCipher.h \
    ../../core/LOVDatabase.h ../../core/LOVDownloader.h ../../data/Data.h \
    ../../core/AdifRecovery.h ../../core/LogLocale.h ../../core/FileCompressor.h \
    ../../data/DxccLogStatusCondition.h \
    ../../logformat/LogFormat.h ../../logformat/AdiFormat.h ../../logformat/AdxFormat.h \
    ../CredentialStoreImportTest/fakes/KeychainJobs.h
RESOURCES += ../../res/res.qrc
!isEmpty(OPENSSLINCLUDEPATH): INCLUDEPATH += $$OPENSSLINCLUDEPATH
!isEmpty(OPENSSLLIBPATH): LIBS += -L$$OPENSSLLIBPATH
win32: LIBS += -llibcrypto
else: LIBS += -lcrypto
!isEmpty(ZLIBINCLUDEPATH): INCLUDEPATH += $$ZLIBINCLUDEPATH
!isEmpty(ZLIBLIBPATH): LIBS += -L$$ZLIBLIBPATH
unix: LIBS += -lz
win32: LIBS += -lzlib
macx: {
    INCLUDEPATH += /usr/local/include /opt/homebrew/include /opt/local/include
    LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lhamlib -lsqlite3 -lz -L/opt/local/lib -lssl -lcrypto
}
