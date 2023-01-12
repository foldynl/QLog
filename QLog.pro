#-------------------------------------------------
#
# Project created by QtCreator 2019-06-10T09:13:09
#
#-------------------------------------------------

QT       += core gui sql network xml charts webenginewidgets serialport dbus quickwidgets

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET = qlog
TEMPLATE = app
VERSION = 0.18.0dev

DEFINES += VERSION=\\\"$$VERSION\\\"

# Define paths to HAMLIB. Leave empty if system libraries should be used
#HAMLIBINCLUDEPATH =
#HAMLIBLIBPATH =
# Define Hamlib version. Leave empty if pkg-config should detect the version (lib must be installed and registered)
#HAMLIBVERSION_MAJOR =
#HAMLIBVERSION_MINOR =
#HAMLIBVERSION_PATCH =

# Define paths to QTKeyChain. Leave empty if system libraries should be used
#QTKEYCHAININCLUDEPATH =
#QTKEYCHAINLIBPATH =

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS QT_MESSAGELOGCONTEXT

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
macx:QT_CONFIG -= no-pkg-config

CONFIG += c++11
CONFIG *= link_pkgconfig

SOURCES += \
        core/AlertEvaluator.cpp \
        core/AppGuard.cpp \
        core/CWCatKey.cpp \
        core/CWDummyKey.cpp \
        core/CWKey.cpp \
        core/CWKeyer.cpp \
        core/CWWinKey.cpp \
        core/CallbookManager.cpp \
        core/Callsign.cpp \
        core/ClubLog.cpp \
        core/Conditions.cpp \
        core/CredentialStore.cpp \
        core/Eqsl.cpp \
        core/Fldigi.cpp \
        core/GenericCallbook.cpp \
        core/Gridsquare.cpp \
        core/HamQTH.cpp \
        core/HostsPortString.cpp \
        core/LOVDownloader.cpp \
        core/LogParam.cpp \
        core/Lotw.cpp \
        core/Migration.cpp \
        core/NetworkNotification.cpp \
        core/PaperQSL.cpp \
        core/QRZ.cpp \
        core/Rig.cpp \
        core/Rotator.cpp \
        core/SerialPort.cpp \
        core/Wsjtx.cpp \
        core/debug.cpp \
        core/main.cpp \
        core/zonedetect.c \
        data/AntProfile.cpp \
        data/CWKeyProfile.cpp \
        data/CWShortcutProfile.cpp \
        data/Data.cpp \
        data/Dxcc.cpp \
        data/RigProfile.cpp \
        data/RotProfile.cpp \
        data/StationProfile.cpp \
        logformat/AdiFormat.cpp \
        logformat/AdxFormat.cpp \
        logformat/JsonFormat.cpp \
        logformat/LogFormat.cpp \
        models/AlertTableModel.cpp \
        models/AwardsTableModel.cpp \
        models/DxccTableModel.cpp \
        models/LogbookModel.cpp \
        models/RigTypeModel.cpp \
        models/RotTypeModel.cpp \
        models/SqlListModel.cpp \
        models/WsjtxTableModel.cpp \
        ui/AlertRuleDetail.cpp \
        ui/AlertSettingDialog.cpp \
        ui/AlertWidget.cpp \
        ui/AwardsDialog.cpp \
        ui/BandmapWidget.cpp \
        ui/CWConsoleWidget.cpp \
        ui/ClockWidget.cpp \
        ui/ClublogDialog.cpp \
        ui/ColumnSettingDialog.cpp \
        ui/DxFilterDialog.cpp \
        ui/DxWidget.cpp \
        ui/DxccTableWidget.cpp \
        ui/Eqsldialog.cpp \
        ui/ExportDialog.cpp \
        ui/ImportDialog.cpp \
        ui/LogbookWidget.cpp \
        ui/LotwDialog.cpp \
        ui/LotwShowUploadDialog.cpp \
        ui/MainWindow.cpp \
        ui/MapWidget.cpp \
        ui/NewContactWidget.cpp \
        ui/OnlineMapWidget.cpp \
        ui/PaperQSLDialog.cpp \
        ui/QSLImportStatDialog.cpp \
        ui/QSODetailDialog.cpp \
        ui/QSOFilterDetail.cpp \
        ui/QSOFilterDialog.cpp \
        ui/QTableQSOView.cpp \
        ui/QrzDialog.cpp \
        ui/RigWidget.cpp \
        ui/RotatorWidget.cpp \
        ui/SettingsDialog.cpp \
        ui/StatisticsWidget.cpp \
        ui/SwitchButton.cpp \
        ui/WsjtxFilterDialog.cpp \
        ui/WsjtxWidget.cpp

HEADERS += \
        core/AlertEvaluator.h \
        core/AppGuard.h \
        core/CWCatKey.h \
        core/CWDummyKey.h \
        core/CWKey.h \
        core/CWKeyer.h \
        core/CWWinKey.h \
        core/CallbookManager.h \
        core/Callsign.h \
        core/ClubLog.h \
        core/Conditions.h \
        core/CredentialStore.h \
        core/Eqsl.h \
        core/Fldigi.h \
        core/GenericCallbook.h \
        core/Gridsquare.h \
        core/HamQTH.h \
        core/HostsPortString.h \
        core/LOVDownloader.h \
        core/LogParam.h \
        core/Lotw.h \
        core/Migration.h \
        core/NetworkNotification.h \
        core/PaperQSL.h \
        core/QRZ.h \
        core/Rig.h \
        core/Rotator.h \
        core/SerialPort.h \
        core/Wsjtx.h \
        core/debug.h \
        core/zonedetect.h \
        data/AntProfile.h \
        data/Band.h \
        data/CWKeyProfile.h \
        data/CWShortcutProfile.h \
        data/Data.h \
        data/DxSpot.h \
        data/Dxcc.h \
        data/POTAEntity.h \
        data/ProfileManager.h \
        data/RigProfile.h \
        data/RotProfile.h \
        data/SOTAEntity.h \
        data/SpotAlert.h \
        data/StationProfile.h \
        data/ToAllSpot.h \
        data/WCYSpot.h \
        data/WWFFEntity.h \
        data/WWVSpot.h \
        data/WsjtxEntry.h \
        logformat/AdiFormat.h \
        logformat/AdxFormat.h \
        logformat/JsonFormat.h \
        logformat/LogFormat.h \
        models/AlertTableModel.h \
        models/AwardsTableModel.h \
        models/DxccTableModel.h \
        models/LogbookModel.h \
        models/RigTypeModel.h \
        models/RotTypeModel.h \
        models/SqlListModel.h \
        models/WsjtxTableModel.h \
        ui/AlertRuleDetail.h \
        ui/AlertSettingDialog.h \
        ui/AlertWidget.h \
        ui/AwardsDialog.h \
        ui/BandmapWidget.h \
        ui/ButtonStyle.h \
        ui/CWConsoleWidget.h \
        ui/ClockWidget.h \
        ui/ClublogDialog.h \
        ui/ColumnSettingDialog.h \
        ui/DxFilterDialog.h \
        ui/DxWidget.h \
        ui/DxccTableWidget.h \
        ui/Eqsldialog.h \
        ui/ExportDialog.h \
        ui/ImportDialog.h \
        ui/LogbookWidget.h \
        ui/LotwDialog.h \
        ui/LotwShowUploadDialog.h \
        ui/MainWindow.h \
        ui/MapWidget.h \
        ui/NewContactWidget.h \
        ui/OnlineMapWidget.h \
        ui/PaperQSLDialog.h \
        ui/QSLImportStatDialog.h \
        ui/QSODetailDialog.h \
        ui/QSOFilterDetail.h \
        ui/QSOFilterDialog.h \
        ui/QTableQSOView.h \
        ui/QrzDialog.h \
        ui/SplashScreen.h \
        ui/StyleItemDelegate.h \
        ui/RigWidget.h \
        ui/RotatorWidget.h \
        ui/SettingsDialog.h \
        ui/StatisticsWidget.h \
        ui/SwitchButton.h \
        ui/WsjtxFilterDialog.h \
        ui/WsjtxWidget.h

FORMS += \
        ui/AlertRuleDetail.ui \
        ui/AlertSettingDialog.ui \
        ui/AlertWidget.ui \
        ui/AwardsDialog.ui \
        ui/BandmapWidget.ui \
        ui/CWConsoleWidget.ui \
        ui/ClockWidget.ui \
        ui/ClublogDialog.ui \
        ui/ColumnSettingDialog.ui \
        ui/ColumnSettingSimpleDialog.ui \
        ui/DxFilterDialog.ui \
        ui/DxWidget.ui \
        ui/Eqsldialog.ui \
        ui/ExportDialog.ui \
        ui/ImportDialog.ui \
        ui/LogbookWidget.ui \
        ui/LotwDialog.ui \
        ui/LotwShowUploadDialog.ui \
        ui/MainWindow.ui \
        ui/NewContactWidget.ui \
        ui/PaperQSLDialog.ui \
        ui/QSLImportStatDialog.ui \
        ui/QSODetailDialog.ui \
        ui/QSOFilterDetail.ui \
        ui/QSOFilterDialog.ui \
        ui/QrzDialog.ui \
        ui/RigWidget.ui \
        ui/RotatorWidget.ui \
        ui/SettingsDialog.ui \
        ui/StatisticsWidget.ui \
        ui/WsjtxFilterDialog.ui \
        ui/WsjtxWidget.ui

RESOURCES += \
    i18n/i18n.qrc \
    res/flags/flags.qrc \
    res/icons/icons.qrc \
    res/res.qrc

OTHER_FILES += \
    res/stylesheet.css \
    res/qlog.rc \
    res/qlog.desktop

TRANSLATIONS = i18n/qlog_de.ts \
               i18n/qlog_cs.ts

RC_ICONS = res/qlog.ico
ICON = res/qlog.icns

# https://stackoverflow.com/questions/56734224/qmake-and-pkg-config?rq=1
defineReplace(findPackage) {
    pkg = $${1}Version
    !defined($$pkg, var) {
        $$pkg = $$system($$pkgConfigExecutable() --modversion $$1)
        isEmpty($$pkg): $$pkg = 0
        cache($$pkg, stash)
    }
    return($$eval($$pkg))
}

isEmpty(HAMLIBVERSION_MAJOR) {
   HAMLIBVERSIONSTRING =  $$findPackage(hamlib)
   HAMLIBVERSIONS = $$split(HAMLIBVERSIONSTRING, ".")
   HAMLIBVERSION_MAJOR = $$member(HAMLIBVERSIONS, 0)
   HAMLIBVERSION_MINOR = $$member(HAMLIBVERSIONS, 1)
   HAMLIBVERSION_PATCH = $$member(HAMLIBVERSIONS, 2)

   isEmpty(HAMLIBVERSION_MINOR){
      HAMLIBVERSION_MINOR=0
   }
   isEmpty(HAMLIBVERSION_PATCH){
     HAMLIBVERSION_PATCH=0
   }
}

INCLUDEPATH += $$HAMLIBINCLUDEPATH $$QTKEYCHAININCLUDEPATH
LIBS += -L$$HAMLIBLIBPATH -L$$QTKEYCHAINLIBPATH

unix:!macx {
   isEmpty(PREFIX) {
     PREFIX = /usr/local
   }

   target.path = $$PREFIX/bin

   desktop.path = $$PREFIX/share/applications/
   desktop.files += res/$${TARGET}.desktop

   icon.path = $$PREFIX/share/icons/hicolor/256x256/apps
   icon.files += res/$${TARGET}.png

   INSTALLS += target desktop icon

   INCLUDEPATH += /usr/local/include
   LIBS += -L/usr/local/lib -lhamlib
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
}

macx: {
   INCLUDEPATH += /usr/local/include
   LIBS += /usr/local/lib -lhamlib
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
   DISTFILES +=
}

win32: {
   TARGET = qlog
   QMAKE_TARGET_COMPANY = OK1MLG
   QMAKE_TARGET_DESCRIPTION = Hamradio logging

   LIBS += -lws2_32 -lhamlib
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
}

DEFINES += HAMLIBVERSION_MAJOR=$$HAMLIBVERSION_MAJOR
DEFINES += HAMLIBVERSION_MINOR=$$HAMLIBVERSION_MINOR
DEFINES += HAMLIBVERSION_PATCH=$$HAMLIBVERSION_PATCH

DISTFILES += \
    Changelog \
    res/data/sat_modes
