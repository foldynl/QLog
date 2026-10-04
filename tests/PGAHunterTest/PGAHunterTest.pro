QT += testlib core sql widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_pgahunter
INCLUDEPATH += $$PWD/../..
SOURCES += tst_pgahunter.cpp \
           ../../awards/AwardPGAHunter.cpp \
           ../../awards/AwardDefinition.cpp
HEADERS += ../../awards/AwardPGAHunter.h ../../awards/AwardDefinition.h
FORMS += ../../awards/PGAHunterTable.ui
