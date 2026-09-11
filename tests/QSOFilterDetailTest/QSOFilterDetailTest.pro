QT += testlib core gui sql widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_qsofilterdetail
INCLUDEPATH += $$PWD/../..
SOURCES += tst_qsofilterdetail.cpp \
           ../../core/QSOFilterManager.cpp \
           ../../core/QSOFilterDateRange.cpp \
           ../../core/LogLocale.cpp \
           ../../models/SqlListModel.cpp \
           ../../ui/QSOFilterDetail.cpp \
           ../../ui/component/QSOFilterDateRangeEdit.cpp \
           ../../ui/component/LogbookFieldComboBox.cpp
HEADERS += ../../core/QSOFilterManager.h \
           ../../models/SqlListModel.h \
           ../../data/Data.h \
           ../../ui/QSOFilterDetail.h \
           ../../ui/component/QSOFilterDateRangeEdit.h
FORMS += ../../ui/QSOFilterDetail.ui \
         ../../ui/QSOFilterRule.ui \
         ../../ui/component/QSOFilterDateRangeEdit.ui \
         ../../ui/component/QSOFilterDateBoundary.ui \
         ../../ui/component/QSOFilterDateRangeDialog.ui
