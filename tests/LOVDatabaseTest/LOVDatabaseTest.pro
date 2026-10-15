QT += testlib core gui sql xml network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_lovdatabase
DEFINES += VERSION=\\\"test\\\"
INCLUDEPATH += $$PWD/../..
SOURCES += tst_lovdatabase.cpp \
    ../../core/LOVDatabase.cpp \
    ../../core/LOVDownloader.cpp \
    ../../core/LogParam.cpp \
    ../../core/AdifRecovery.cpp \
    ../../data/BandPlan.cpp \
    ../../data/DxccLogStatusCondition.cpp \
    ../../core/FileCompressor.cpp
HEADERS += ../../core/LOVDatabase.h \
    ../../core/LOVDownloader.h \
    ../../core/LogParam.h \
    ../../core/AdifRecovery.h \
    ../../data/BandPlan.h \
    ../../data/DxccLogStatusCondition.h
RESOURCES += lov_fixtures.qrc
!isEmpty(ZLIBINCLUDEPATH): INCLUDEPATH += $$ZLIBINCLUDEPATH
!isEmpty(ZLIBLIBPATH): LIBS += -L$$ZLIBLIBPATH
unix: LIBS += -lz
win32: LIBS += -lzlib
