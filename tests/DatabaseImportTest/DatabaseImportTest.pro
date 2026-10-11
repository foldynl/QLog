QT += testlib core widgets sql network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_databaseimport
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../CredentialStoreImportTest/fakes $$PWD/../..
# Keep unrelated services and backup export paths out of this focused target.
!win32-msvc: QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
unix:!macx: QMAKE_LFLAGS += -Wl,--gc-sections
unix:!macx {
    DEFINES += IMPORT_INTERRUPTION_WRAPPERS
    QMAKE_LFLAGS += -Wl,--wrap=_ZN5QFile6renameERK7QStringS2_ \
                   -Wl,--wrap=_ZN9QSaveFile6commitEv \
                   -Wl,--wrap=_ZN5QFile6removeERK7QString
}
win32-g++: QMAKE_LFLAGS += -Wl,--gc-sections
macx: QMAKE_LFLAGS += -Wl,-dead_strip
win32-msvc {
    QMAKE_CXXFLAGS += /Gy /Gw
    QMAKE_LFLAGS += /OPT:REF
}
LIBS += -lsqlite3
SOURCES += tst_databaseimport.cpp \
    ../MigrationTest/test_stubs.cpp \
    ../../core/LogDatabase.cpp ../../core/PlatformParameterManager.cpp \
    ../../core/Migration.cpp ../../core/DatabaseBackup.cpp \
    ../../core/LogParam.cpp ../../core/CredentialStore.cpp ../../core/PasswordCipher.cpp \
    ../../core/LOVDatabase.cpp ../../core/LOVDownloader.cpp \
    ../../data/BandPlan.cpp ../../data/Accents.cpp
HEADERS += ../../core/LogDatabase.h ../../core/Migration.h ../../core/DatabaseBackup.h \
    ../../core/LogParam.h ../../core/CredentialStore.h ../../core/PasswordCipher.h \
    ../../core/LOVDatabase.h ../../core/LOVDownloader.h ../../data/Data.h \
    ../CredentialStoreImportTest/fakes/KeychainJobs.h
RESOURCES += ../../res/res.qrc
!isEmpty(OPENSSLINCLUDEPATH): INCLUDEPATH += $$OPENSSLINCLUDEPATH
!isEmpty(OPENSSLLIBPATH): LIBS += -L$$OPENSSLLIBPATH
win32: LIBS += -llibcrypto
else: LIBS += -lcrypto
