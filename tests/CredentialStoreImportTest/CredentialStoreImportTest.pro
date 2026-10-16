QT += testlib core widgets sql network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_credentialstoreimport
INCLUDEPATH += $$PWD/fakes $$PWD/../..
SOURCES += tst_credentialstoreimport.cpp \
    ../CredentialStoreTest/test_stubs.cpp \
    ../../core/CredentialStore.cpp \
    ../../core/PasswordCipher.cpp \
    ../../core/AdifRecovery.cpp \
    ../../core/LogParam.cpp \
    ../../data/DxccLogStatusCondition.cpp
HEADERS += fakes/KeychainJobs.h \
    ../../core/CredentialStore.h \
    ../../core/PasswordCipher.h \
    ../../core/AdifRecovery.h \
    ../../core/LogParam.h
!isEmpty(OPENSSLINCLUDEPATH): INCLUDEPATH += $$OPENSSLINCLUDEPATH
!isEmpty(OPENSSLLIBPATH): LIBS += -L$$OPENSSLLIBPATH
win32: LIBS += -llibcrypto
else: LIBS += -lcrypto
macx: {
    INCLUDEPATH += /usr/local/include /opt/homebrew/include /opt/local/include
    LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lhamlib -lsqlite3 -lz -L/opt/local/lib -lssl -lcrypto
}
