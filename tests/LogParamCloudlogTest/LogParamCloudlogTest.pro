QT += testlib core widgets sql network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_logparamcloudlog

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_logparamcloudlog.cpp \
    test_stubs.cpp \
    ../../core/CredentialStore.cpp \
    ../../core/PasswordCipher.cpp \
    ../../core/AdifRecovery.cpp \
    ../../core/LogParam.cpp \
    ../../data/DxccLogStatusCondition.cpp

HEADERS += \
    ../../core/CredentialStore.h \
    ../../core/PasswordCipher.h \
    ../../core/AdifRecovery.h \
    ../../core/LogParam.h

# QtKeychain
!isEmpty(QTKEYCHAININCLUDEPATH) {
   INCLUDEPATH += $$QTKEYCHAININCLUDEPATH
}

!isEmpty(QTKEYCHAINLIBPATH) {
   LIBS += -L$$QTKEYCHAINLIBPATH
}

equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain

# OpenSSL (for PasswordCipher)
!isEmpty(OPENSSLINCLUDEPATH) {
    INCLUDEPATH += $$OPENSSLINCLUDEPATH
}
!isEmpty(OPENSSLLIBPATH) {
    LIBS += -L$$OPENSSLLIBPATH
}
INCLUDEPATH += /usr/local/include /opt/homebrew/include /opt/local/include

win32 {
    LIBS += -llibcrypto
} else {
    LIBS += -lcrypto
}

macx: {
    INCLUDEPATH += /usr/local/include /opt/homebrew/include /opt/local/include
    LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lhamlib -lsqlite3 -lz -L/opt/local/lib -lssl -lcrypto
}
