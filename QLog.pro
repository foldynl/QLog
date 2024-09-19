#-------------------------------------------------
#
# Project created by QtCreator 2019-06-10T09:13:09
#
#-------------------------------------------------

QT       += core gui sql network xml charts webenginewidgets serialport dbus quickwidgets webchannel websockets

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET = qlog
TEMPLATE = app
VERSION = 0.38.0

DEFINES += VERSION=\\\"$$VERSION\\\"

# Define paths to HAMLIB. Leave empty if system libraries should be used
#HAMLIBINCLUDEPATH =
#HAMLIBLIBPATH =
# Define Hamlib version. Leave empty if pkg-config should detect the version (lib must be installed and registered)
#HAMLIBVERSION_MAJOR =
#HAMLIBVERSION_MINOR =
#HAMLIBVERSION_PATCH =

# Define paths to pthread - needed for Hamlib4.5 and later. Leave empty if system libraries should be used
#PTHREADINCLUDEPATH =
#PTHREADLIBPATH =

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

CONFIG += c++11 force_debug_info
CONFIG *= link_pkgconfig

SOURCES += \
        core/AlertEvaluator.cpp \
        core/AppGuard.cpp \
        core/CallbookManager.cpp \
        core/Callsign.cpp \
        core/ClubLog.cpp \
        core/CredentialStore.cpp \
        core/DxServerString.cpp \
        core/Eqsl.cpp \
        core/Fldigi.cpp \
        core/GenericCallbook.cpp \
        core/Gridsquare.cpp \
        core/HRDLog.cpp \
        core/HamQTH.cpp \
        core/HostsPortString.cpp \
        core/KSTChat.cpp \
        core/LOVDownloader.cpp \
        core/LogLocale.cpp \
        core/LogParam.cpp \
        core/Lotw.cpp \
        core/MembershipQE.cpp \
        core/Migration.cpp \
        core/MultiselectCompleter.cpp \
        core/NetworkNotification.cpp \
        core/PropConditions.cpp \
        core/QRZ.cpp \
        core/QSLStorage.cpp \
        core/SerialPort.cpp \
        core/UpdatableSQLRecord.cpp \
        core/Wsjtx.cpp \
        core/debug.cpp \
        core/main.cpp \
        core/zonedetect.c \
        cwkey/CWKeyer.cpp \
        cwkey/drivers/CWCatKey.cpp \
        cwkey/drivers/CWDaemonKey.cpp \
        cwkey/drivers/CWDummyKey.cpp \
        cwkey/drivers/CWFldigiKey.cpp \
        cwkey/drivers/CWKey.cpp \
        cwkey/drivers/CWWinKey.cpp \
        data/AntProfile.cpp \
        data/BandPlan.cpp \
        data/CWKeyProfile.cpp \
        data/CWShortcutProfile.cpp \
        data/Data.cpp \
        data/Dxcc.cpp \
        data/MainLayoutProfile.cpp \
        data/RigProfile.cpp \
        data/RotProfile.cpp \
        data/RotUsrButtonsProfile.cpp \
        data/StationProfile.cpp \
        logformat/AdiFormat.cpp \
        logformat/AdxFormat.cpp \
        logformat/CSVFormat.cpp \
        logformat/JsonFormat.cpp \
        logformat/LogFormat.cpp \
        models/AlertTableModel.cpp \
        models/AwardsTableModel.cpp \
        models/DxccTableModel.cpp \
        models/LogbookModel.cpp \
        models/RigTypeModel.cpp \
        models/RotTypeModel.cpp \
        models/ShortcutEditorModel.cpp \
        models/SqlListModel.cpp \
        models/WsjtxTableModel.cpp \
        rig/Rig.cpp \
        rig/RigCaps.cpp \
        rig/drivers/GenericRigDrv.cpp \
        rig/drivers/HamlibRigDrv.cpp \
        rig/drivers/TCIRigDrv.cpp \
        rotator/RotCaps.cpp \
        rotator/Rotator.cpp \
        rotator/drivers/GenericRotDrv.cpp \
        rotator/drivers/HamlibRotDrv.cpp \
        rotator/drivers/PSTRotDrv.cpp \
        ui/AlertRuleDetail.cpp \
        ui/AlertSettingDialog.cpp \
        ui/AlertWidget.cpp \
        ui/AwardsDialog.cpp \
        ui/BandmapWidget.cpp \
        ui/CWConsoleWidget.cpp \
        ui/ChatWidget.cpp \
        ui/ClockWidget.cpp \
        ui/ClublogDialog.cpp \
        ui/ColumnSettingDialog.cpp \
        ui/DxFilterDialog.cpp \
        ui/DxWidget.cpp \
        ui/DxccTableWidget.cpp \
        ui/EditLayoutDialog.cpp \
        ui/EditLine.cpp \
        ui/Eqsldialog.cpp \
        ui/ExportDialog.cpp \
        ui/FreqQSpinBox.cpp \
        ui/HRDLogDialog.cpp \
        ui/ImportDialog.cpp \
        ui/InputPasswordDialog.cpp \
        ui/KSTChatWidget.cpp \
        ui/KSTHighlightRuleDetail.cpp \
        ui/KSTHighlighterSettingDialog.cpp \
        ui/LogbookWidget.cpp \
        ui/LotwDialog.cpp \
        ui/MainLayoutEditor.cpp \
        ui/MainWindow.cpp \
        ui/MapWebChannelHandler.cpp \
        ui/MapWidget.cpp \
        ui/NewContactWidget.cpp \
        ui/OnlineMapWidget.cpp \
        ui/PaperQSLDialog.cpp \
        ui/ProfileImageWidget.cpp \
        ui/QSLImportStatDialog.cpp \
        ui/QSODetailDialog.cpp \
        ui/QSOFilterDetail.cpp \
        ui/QSOFilterDialog.cpp \
        ui/QTableQSOView.cpp \
        ui/QrzDialog.cpp \
        ui/RigWidget.cpp \
        ui/RotatorWidget.cpp \
        ui/SettingsDialog.cpp \
        ui/ShowUploadDialog.cpp \
        ui/StatisticsWidget.cpp \
        ui/SwitchButton.cpp \
        ui/WebEnginePage.cpp \
        ui/WsjtxFilterDialog.cpp \
        ui/WsjtxWidget.cpp \
        ui/multiqsoupload.cpp

HEADERS += \
        core/AlertEvaluator.h \
        core/AppGuard.h \
        core/CallbookManager.h \
        core/Callsign.h \
        core/ClubLog.h \
        core/CredentialStore.h \
        core/DxServerString.h \
        core/Eqsl.h \
        core/Fldigi.h \
        core/GenericCallbook.h \
        core/Gridsquare.h \
        core/HRDLog.h \
        core/HamQTH.h \
        core/HostsPortString.h \
        core/KSTChat.h \
        core/LOVDownloader.h \
        core/LogLocale.h \
        core/LogParam.h \
        core/Lotw.h \
        core/MembershipQE.h \
        core/Migration.h \
        core/MultiselectCompleter.h \
        core/NetworkNotification.h \
        core/PropConditions.h \
        core/QRZ.h \
        core/QSLStorage.h \
        core/SerialPort.h \
        core/UpdatableSQLRecord.h \
        core/Wsjtx.h \
        core/debug.h \
        core/zonedetect.h \
        cwkey/CWKeyer.h \
        cwkey/drivers/CWCatKey.h \
        cwkey/drivers/CWDaemonKey.h \
        cwkey/drivers/CWDummyKey.h \
        cwkey/drivers/CWFldigiKey.h \
        cwkey/drivers/CWKey.h \
        cwkey/drivers/CWWinKey.h \
        data/AntProfile.h \
        data/Band.h \
        data/BandPlan.h \
        data/CWKeyProfile.h \
        data/CWShortcutProfile.h \
        data/Data.h \
        data/DxSpot.h \
        data/Dxcc.h \
        data/MainLayoutProfile.h \
        data/POTAEntity.h \
        data/ProfileManager.h \
        data/RigProfile.h \
        data/RotProfile.h \
        data/RotUsrButtonsProfile.h \
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
        logformat/CSVFormat.h \
        logformat/JsonFormat.h \
        logformat/LogFormat.h \
        models/AlertTableModel.h \
        models/AwardsTableModel.h \
        models/DxccTableModel.h \
        models/LogbookModel.h \
        models/RigTypeModel.h \
        models/RotTypeModel.h \
        models/ShortcutEditorModel.h \
        models/SqlListModel.h \
        models/WsjtxTableModel.h \
        rig/Rig.h \
        rig/RigCaps.h \
        rig/drivers/GenericRigDrv.h \
        rig/drivers/HamlibRigDrv.h \
        rig/drivers/TCIRigDrv.h \
        rig/macros.h \
        rotator/RotCaps.h \
        rotator/Rotator.h \
        rotator/drivers/GenericRotDrv.h \
        rotator/drivers/HamlibRotDrv.h \
        rotator/drivers/PSTRotDrv.h \
        ui/AlertRuleDetail.h \
        ui/AlertSettingDialog.h \
        ui/AlertWidget.h \
        ui/AwardsDialog.h \
        ui/BandmapWidget.h \
        ui/ButtonStyle.h \
        ui/CWConsoleWidget.h \
        ui/ChatWidget.h \
        ui/ClockWidget.h \
        ui/ClublogDialog.h \
        ui/ColumnSettingDialog.h \
        ui/DxFilterDialog.h \
        ui/DxWidget.h \
        ui/DxccTableWidget.h \
        ui/EditLayoutDialog.h \
        ui/EditLine.h \
        ui/Eqsldialog.h \
        ui/ExportDialog.h \
        ui/FreqQSpinBox.h \
        ui/HRDLogDialog.h \
        ui/ImportDialog.h \
        ui/InputPasswordDialog.h \
        ui/KSTChatWidget.h \
        ui/KSTHighlightRuleDetail.h \
        ui/KSTHighlighterSettingDialog.h \
        ui/LogbookWidget.h \
        ui/LotwDialog.h \
        ui/MainLayoutEditor.h \
        ui/MainWindow.h \
        ui/MapWebChannelHandler.h \
        ui/MapWidget.h \
        ui/NewContactWidget.h \
        ui/OnlineMapWidget.h \
        ui/PaperQSLDialog.h \
        ui/ProfileImageWidget.h \
        ui/QSLImportStatDialog.h \
        ui/QSODetailDialog.h \
        ui/QSOFilterDetail.h \
        ui/QSOFilterDialog.h \
        ui/QTableQSOView.h \
        ui/QrzDialog.h \
        ui/ShowUploadDialog.h \
        ui/SplashScreen.h \
        ui/StyleItemDelegate.h \
        ui/RigWidget.h \
        ui/RotatorWidget.h \
        ui/SettingsDialog.h \
        ui/StatisticsWidget.h \
        ui/SwitchButton.h \
        ui/WebEnginePage.h \
        ui/WsjtxFilterDialog.h \
        ui/WsjtxWidget.h \
        i18n/dbstrings.tri \
        i18n/datastrings.tri \
        ui/multiqsoupload.h

FORMS += \
        ui/AlertRuleDetail.ui \
        ui/AlertSettingDialog.ui \
        ui/AlertWidget.ui \
        ui/AwardsDialog.ui \
        ui/BandmapWidget.ui \
        ui/CWConsoleWidget.ui \
        ui/ChatWidget.ui \
        ui/ClockWidget.ui \
        ui/ClublogDialog.ui \
        ui/ColumnSettingDialog.ui \
        ui/ColumnSettingSimpleDialog.ui \
        ui/DxFilterDialog.ui \
        ui/DxWidget.ui \
        ui/EditLayoutDialog.ui \
        ui/Eqsldialog.ui \
        ui/ExportDialog.ui \
        ui/HRDLogDialog.ui \
        ui/ImportDialog.ui \
        ui/InputPasswordDialog.ui \
        ui/KSTChatWidget.ui \
        ui/KSTHighlightRuleDetail.ui \
        ui/KSTHighlighterSettingDialog.ui \
        ui/LogbookWidget.ui \
        ui/LotwDialog.ui \
        ui/MainLayoutEditor.ui \
        ui/MainWindow.ui \
        ui/NewContactWidget.ui \
        ui/PaperQSLDialog.ui \
        ui/ProfileImageWidget.ui \
        ui/QSLImportStatDialog.ui \
        ui/QSODetailDialog.ui \
        ui/QSOFilterDetail.ui \
        ui/QSOFilterDialog.ui \
        ui/QrzDialog.ui \
        ui/RigWidget.ui \
        ui/RotatorWidget.ui \
        ui/SettingsDialog.ui \
        ui/ShowUploadDialog.ui \
        ui/StatisticsWidget.ui \
        ui/WsjtxFilterDialog.ui \
        ui/WsjtxWidget.ui \
        ui/multiqsoupload.ui

RESOURCES += \
    i18n/i18n.qrc \
    res/flags/flags.qrc \
    res/icons/icons.qrc \
    res/res.qrc

OTHER_FILES += \
    res/stylesheet.css \
    res/qlog.rc \
    res/qlog.desktop \
    res/io.github.foldynl.QLog.metainfo.xml

TRANSLATIONS = i18n/qlog_cs.ts \
               i18n/qlog_de.ts \
               i18n/qlog_es.ts \
               i18n/qlog_it.ts \
               i18n/qlog_zh_CN.ts

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

defineReplace(removeNonDigi) {
    output = $$1
    output = $$replace(output, [^0-9], " ")
    output = $$split(output, " ")
    return($$member(output, 0))
}

isEmpty(HAMLIBVERSION_MAJOR) {
   HAMLIBVERSIONSTRING =  $$findPackage(hamlib)
   HAMLIBVERSIONS = $$split(HAMLIBVERSIONSTRING, ".")
   HAMLIBVERSION_MAJOR = $$member(HAMLIBVERSIONS, 0)
   HAMLIBVERSION_MINOR = $$member(HAMLIBVERSIONS, 1)
   HAMLIBVERSION_PATCH = $$member(HAMLIBVERSIONS, 2)
}

HAMLIBVERSION_MINOR = $$removeNonDigi($$HAMLIBVERSION_MINOR)

isEmpty(HAMLIBVERSION_MINOR){
   HAMLIBVERSION_MINOR=0
}

HAMLIBVERSION_PATCH = $$removeNonDigi($$HAMLIBVERSION_PATCH)

isEmpty(HAMLIBVERSION_PATCH){
  HAMLIBVERSION_PATCH=0
}

!isEmpty(HAMLIBINCLUDEPATH) {
   INCLUDEPATH += $$HAMLIBINCLUDEPATH
}

!isEmpty(QTKEYCHAININCLUDEPATH) {
   INCLUDEPATH += $$QTKEYCHAININCLUDEPATH
}

!isEmpty(PTHREADINCLUDEPATH) {
   INCLUDEPATH += $$PTHREADINCLUDEPATH
}

!isEmpty(HAMLIBLIBPATH) {
   LIBS += -L$$HAMLIBLIBPATH
}

!isEmpty(QTKEYCHAINLIBPATH) {
   LIBS += -L$$QTKEYCHAINLIBPATH
}

!isEmpty(PTHREADINCLUDEPATH) {
   LIBS += -L$$PTHREADINCLUDEPATH
}

unix:!macx {
   isEmpty(PREFIX) {
     PREFIX = /usr/local
   }

   target.path = $$PREFIX/bin

   desktop.path = $$PREFIX/share/applications/
   desktop.files += res/$${TARGET}.desktop

   icon.path = $$PREFIX/share/icons/hicolor/256x256/apps
   icon.files += res/$${TARGET}.png

   metainfo.path = $$PREFIX/share/metainfo/
   metainfo.files += res/io.github.foldynl.QLog.metainfo.xml

   INSTALLS += target desktop icon metainfo

   INCLUDEPATH += /usr/local/include
   LIBS += -L/usr/local/lib -lhamlib -lsqlite3
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
}

macx: {
   # This allows the app to be shipped in a non-bundeled version
   !isEmpty(PREFIX) {
      target.path = $$PREFIX
      INSTALLS += target
   }

   INCLUDEPATH += /usr/local/include /opt/homebrew/include
   LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lhamlib -lsqlite3
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
   DISTFILES +=
}

win32: {
   QT += axcontainer
   TYPELIBS = $$system($$[QT_INSTALL_BINS]/dumpcpp -getfile {4FE359C5-A58F-459D-BE95-CA559FB4F270})
   isEmpty(TYPELIBS){
       error("Omnirig v1 is not found - required")
   }

   !exists( $$OUT_PWD/OmniRig2.* ) {
      OMNIRIG2 = $$system($$[QT_INSTALL_BINS]/dumpcpp {959819FF-B57B-468A-9F30-BBA6BE319987} -n OmniRigV2 -o $$OUT_PWD/OmniRig2)
      isEmpty(OMNIRIG2){
          error("Omnirig v2 is not found - required")
      }
   }

   INCLUDEPATH += \
        /usr/local/include \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/

   SOURCES += \
        rig/drivers/OmnirigRigDrv.cpp \
        rig/drivers/Omnirigv2RigDrv.cpp \
        OmniRig2.cpp \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.c


   HEADERS += \
        rig/drivers/OmnirigRigDrv.h \
        rig/drivers/Omnirigv2RigDrv.h \
        OmniRig2.h \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.h

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
    i18n/dbstrings.tri \
    res/data/sat_modes
