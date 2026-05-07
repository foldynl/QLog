#-------------------------------------------------
#
# Project created by QtCreator 2019-06-10T09:13:09
#
#-------------------------------------------------

QT       += core gui sql network xml charts webenginewidgets serialport dbus quickwidgets webchannel websockets printsupport

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET = qlog
TEMPLATE = app
VERSION = 0.50.0

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

# Define paths to zlib - Leave empty if system libraries should be used
#ZLIBINCLUDEPATH =
#ZLIBLIBPATH =

# Define paths to OpenSSL - Leave empty if system libraries should be used
#OPENSSLINCLUDEPATH =
#OPENSSLLIBPATH =

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
#CONFIG += sanitizer sanitize_address
CONFIG *= link_pkgconfig

SOURCES += \
        awards/AwardDefinition.cpp \
        awards/AwardDXCC.cpp \
        awards/AwardGridsquare.cpp \
        awards/AwardIOTA.cpp \
        awards/AwardITU.cpp \
        awards/AwardJapan.cpp \
        awards/AwardNZ.cpp \
        awards/AwardRDA.cpp \
        awards/AwardSpanishDME.cpp \
        awards/AwardUKD.cpp \
        awards/AwardUSCounty.cpp \
        awards/AwardPOTAActivator.cpp \
        awards/AwardPOTAHunter.cpp \
        awards/AwardSOTA.cpp \
        awards/SecondarySubdivisionAward.cpp \
        awards/AwardWAC.cpp \
        awards/AwardWAS.cpp \
        awards/AwardWAZ.cpp \
        awards/AwardWPX.cpp \
        awards/AwardWWFF.cpp \
        awards/BandTableAward.cpp \
        core/AlertEvaluator.cpp \
        core/AppGuard.cpp \
        core/CallbookManager.cpp \
        core/CredentialStore.cpp \
        core/FileCompressor.cpp \
        core/FldigiTCPServer.cpp \
        core/LOVDownloader.cpp \
        core/LogDatabase.cpp \
        core/LogLocale.cpp \
        core/LogParam.cpp \
        core/MembershipQE.cpp \
        core/Migration.cpp \
        core/NetworkNotification.cpp \
        core/PasswordCipher.cpp \
        core/PlatformParameterManager.cpp \
        core/PotaQE.cpp \
        core/PropConditions.cpp \
        core/QSLPrintLabelRenderer.cpp \
        core/QSLStorage.cpp \
        core/QSOFilterManager.cpp \
        core/WsjtxUDPReceiver.cpp \
        core/debug.cpp \
        core/EmergencyFrequency.cpp \
        core/main.cpp \
        core/zonedetect.c \
        cwkey/CWKeyer.cpp \
        cwkey/drivers/CWCatKey.cpp \
        cwkey/drivers/CWDaemonKey.cpp \
        cwkey/drivers/CWDummyKey.cpp \
        cwkey/drivers/CWFldigiKey.cpp \
        cwkey/drivers/CWKey.cpp \
        cwkey/drivers/CWWinKey.cpp \
        data/ActivityProfile.cpp \
        data/AntProfile.cpp \
        data/BandPlan.cpp \
        data/Accents.cpp \
        data/CWKeyProfile.cpp \
        data/CWShortcutProfile.cpp \
        data/Callsign.cpp \
        data/Data.cpp \
        data/DxServerString.cpp \
        data/Gridsquare.cpp \
        data/HostsPortString.cpp \
        data/MainLayoutProfile.cpp \
        data/RigProfile.cpp \
        data/RotProfile.cpp \
        data/RotUsrButtonsProfile.cpp \
        data/SerialPort.cpp \
        data/StationProfile.cpp \
        data/UpdatableSQLRecord.cpp \
        logformat/AdiFormat.cpp \
        logformat/AdxFormat.cpp \
        logformat/CabrilloFormat.cpp \
        logformat/CSVFormat.cpp \
        logformat/JsonFormat.cpp \
        logformat/LogFormat.cpp \
        logformat/PotaAdiFormat.cpp \
        models/AlertTableModel.cpp \
        models/AwardsTableModel.cpp \
        models/DxccTableModel.cpp \
        models/LogbookModel.cpp \
        models/RigTypeModel.cpp \
        models/RotTypeModel.cpp \
        models/SearchFilterProxyModel.cpp \
        models/ShortcutEditorModel.cpp \
        models/SqlListModel.cpp \
        models/WsjtxTableModel.cpp \
        rig/Rig.cpp \
        rig/RigCaps.cpp \
        rig/RigctldManager.cpp \
        rig/drivers/FlrigRigDrv.cpp \
        rig/drivers/GenericRigDrv.cpp \
        rig/drivers/HamlibRigDrv.cpp \
        rig/drivers/TCIRigDrv.cpp \
        rotator/RotCaps.cpp \
        rotator/Rotator.cpp \
        rotator/drivers/GenericRotDrv.cpp \
        rotator/drivers/HamlibRotDrv.cpp \
        rotator/drivers/PSTRotDrv.cpp \
        service/GenericCallbook.cpp \
        service/GenericQSLDownloader.cpp \
        service/GenericQSOUploader.cpp \
        service/cloudlog/Cloudlog.cpp \
        service/clublog/ClubLog.cpp \
        service/eqsl/Eqsl.cpp \
        service/hamqth/HamQTH.cpp \
        service/hrdlog/HRDLog.cpp \
        service/kstchat/KSTChat.cpp \
        service/lotw/Lotw.cpp \
        service/potaapp/PotaApp.cpp \
        service/qrzcom/QRZ.cpp \
        ui/ActivityEditor.cpp \
        ui/AlertRuleDetail.cpp \
        ui/AlertSettingDialog.cpp \
        ui/AlertWidget.cpp \
        ui/AwardsDialog.cpp \
        ui/DXCCSubmissionDialog.cpp \
        ui/BandmapWidget.cpp \
        ui/CWConsoleWidget.cpp \
        ui/ChatWidget.cpp \
        ui/ClockWidget.cpp \
        ui/ColumnSettingDialog.cpp \
        ui/DevToolsDialog.cpp \
        ui/DownloadQSLDialog.cpp \
        ui/DxFilterDialog.cpp \
        ui/DxWidget.cpp \
        ui/DxccTableWidget.cpp \
        ui/EditActivitiesDialog.cpp \
        ui/CabrilloExportDialog.cpp \
        ui/CabrilloTemplateDialog.cpp \
        ui/ExportDialog.cpp \
        ui/ExportPasswordDialog.cpp \
        ui/LoadDatabaseDialog.cpp \
        ui/PlatformSettingsDialog.cpp \
        ui/QSLGalleryDialog.cpp \
        ui/QSLPrintLabelDialog.cpp \
        ui/RigctldAdvancedDialog.cpp \
        ui/ImportDialog.cpp \
        ui/InputPasswordDialog.cpp \
        ui/KSTChatWidget.cpp \
        ui/KSTHighlightRuleDetail.cpp \
        ui/KSTHighlighterSettingDialog.cpp \
        ui/LogbookWidget.cpp \
        ui/MainWindow.cpp \
        ui/MapWebChannelHandler.cpp \
        ui/MapWidget.cpp \
        ui/ModeSelectionController.cpp \
        ui/NewContactWidget.cpp \
        ui/OnlineMapWidget.cpp \
        ui/PaperQSLDialog.cpp \
        ui/ProfileImageWidget.cpp \
        ui/QSLImportStatDialog.cpp \
        ui/QSODetailDialog.cpp \
        ui/QSOFilterDetail.cpp \
        ui/QSOFilterDialog.cpp \
        ui/QTableQSOView.cpp \
        ui/RigWidget.cpp \
        ui/RotatorWidget.cpp \
        ui/SettingsDialog.cpp \
        ui/ShowUploadDialog.cpp \
        ui/StatisticsWidget.cpp \
        ui/UploadQSODialog.cpp \
        ui/WebEnginePage.cpp \
        ui/WsjtxFilterDialog.cpp \
        ui/WsjtxWidget.cpp \
        ui/component/BaseDoubleSpinBox.cpp \
        ui/component/EditLine.cpp \
        ui/component/FreqQSpinBox.cpp \
        ui/component/MultiselectCompleter.cpp \
        ui/component/RepeatButton.cpp \
        ui/component/SmartSearchBox.cpp \
        ui/component/SqlHighlighter.cpp \
        ui/component/SwitchButton.cpp

HEADERS += \
        awards/AwardDefinition.h \
        awards/AwardDXCC.h \
        awards/AwardGridsquare.h \
        awards/AwardIOTA.h \
        awards/AwardITU.h \
        awards/AwardJapan.h \
        awards/AwardNZ.h \
        awards/AwardRDA.h \
        awards/AwardSpanishDME.h \
        awards/AwardUKD.h \
        awards/AwardUSCounty.h \
        awards/AwardPOTAActivator.h \
        awards/AwardPOTAHunter.h \
        awards/AwardSOTA.h \
        awards/SecondarySubdivisionAward.h \
        awards/AwardWAC.h \
        awards/AwardWAS.h \
        awards/AwardWAZ.h \
        awards/AwardWPX.h \
        awards/AwardWWFF.h \
        awards/BandTableAward.h \
        core/AlertEvaluator.h \
        core/AppGuard.h \
        core/CallbookManager.h \
        core/CredentialStore.h \
        core/FileCompressor.h \
        core/FldigiTCPServer.h \
        core/LOVDownloader.h \
        core/LogDatabase.h \
        core/LogLocale.h \
        core/LogParam.h \
        core/MembershipQE.h \
        core/Migration.h \
        core/NetworkNotification.h \
        core/PasswordCipher.h \
        core/PlatformParameterManager.h \
        core/PotaQE.h \
        core/PropConditions.h \
        core/QSLPrintLabelRenderer.h \
        core/QSLStorage.h \
        core/QSOFilterManager.h \
        core/QuadKeyCache.h \
        core/WsjtxUDPReceiver.h \
        core/csv.hpp \
        core/debug.h \
        core/EmergencyFrequency.h \
        core/zonedetect.h \
        cwkey/CWKeyer.h \
        cwkey/drivers/CWCatKey.h \
        cwkey/drivers/CWDaemonKey.h \
        cwkey/drivers/CWDummyKey.h \
        cwkey/drivers/CWFldigiKey.h \
        cwkey/drivers/CWKey.h \
        cwkey/drivers/CWWinKey.h \
        data/ActivityProfile.h \
        data/AntProfile.h \
        data/Band.h \
        data/BandPlan.h \
        data/CWKeyProfile.h \
        data/CWShortcutProfile.h \
        data/Callsign.h \
        data/Data.h \
        data/DxServerString.h \
        data/DxSpot.h \
        data/Dxcc.h \
        data/Gridsquare.h \
        data/HostsPortString.h \
        data/MainLayoutProfile.h \
        data/POTAEntity.h \
        data/POTASpot.h \
        data/ProfileManager.h \
        data/RigProfile.h \
        data/RotProfile.h \
        data/RotUsrButtonsProfile.h \
        data/SOTAEntity.h \
        data/SerialPort.h \
        data/SpotAlert.h \
        data/StationProfile.h \
        data/ToAllSpot.h \
        data/UpdatableSQLRecord.h \
        data/WCYSpot.h \
        data/WWFFEntity.h \
        data/WWVSpot.h \
        data/WsjtxDecode.h \
        data/WsjtxEntry.h \
        data/WsjtxLog.h \
        data/WsjtxLogADIF.h \
        data/WsjtxStatus.h \
        logformat/AdiFormat.h \
        logformat/AdxFormat.h \
        logformat/CabrilloFormat.h \
        logformat/CSVFormat.h \
        logformat/JsonFormat.h \
        logformat/LogFormat.h \
        logformat/PotaAdiFormat.h \
        models/AlertTableModel.h \
        models/AwardsTableModel.h \
        models/DxccTableModel.h \
        models/LogbookModel.h \
        models/RigTypeModel.h \
        models/RotTypeModel.h \
        models/SearchFilterProxyModel.h \
        models/ShortcutEditorModel.h \
        models/SqlListModel.h \
        models/WsjtxTableModel.h \
        rig/Rig.h \
        rig/RigCaps.h \
        rig/RigctldManager.h \
        rig/drivers/FlrigRigDrv.h \
        rig/drivers/GenericRigDrv.h \
        rig/drivers/HamlibRigDrv.h \
        rig/drivers/TCIRigDrv.h \
        rig/macros.h \
        rotator/RotCaps.h \
        rotator/Rotator.h \
        rotator/drivers/GenericRotDrv.h \
        rotator/drivers/HamlibRotDrv.h \
        rotator/drivers/PSTRotDrv.h \
        service/GenericCallbook.h \
        service/GenericQSLDownloader.h \
        service/GenericQSOUploader.h \
        service/cloudlog/Cloudlog.h \
        service/clublog/ClubLog.h \
        service/eqsl/Eqsl.h \
        service/hamqth/HamQTH.h \
        service/hrdlog/HRDLog.h \
        service/kstchat/KSTChat.h \
        service/lotw/Lotw.h \
        service/potaapp/PotaApp.h \
        service/qrzcom/QRZ.h \
        ui/ActivityEditor.h \
        ui/AlertRuleDetail.h \
        ui/AlertSettingDialog.h \
        ui/AlertWidget.h \
        ui/AwardsDialog.h \
        ui/DXCCSubmissionDialog.h \
        ui/BandmapWidget.h \
        ui/CWConsoleWidget.h \
        ui/ChatWidget.h \
        ui/ClockWidget.h \
        ui/ColumnSettingDialog.h \
        ui/DevToolsDialog.h \
        ui/DownloadQSLDialog.h \
        ui/DxFilterDialog.h \
        ui/DxWidget.h \
        ui/DxccTableWidget.h \
        ui/EditActivitiesDialog.h \
        ui/CabrilloExportDialog.h \
        ui/CabrilloTemplateDialog.h \
        ui/ExportDialog.h \
        ui/ExportPasswordDialog.h \
        ui/LoadDatabaseDialog.h \
        ui/PlatformSettingsDialog.h \
        ui/QSLGalleryDialog.h \
        ui/QSLPrintLabelDialog.h \
        ui/RigctldAdvancedDialog.h \
        ui/ImportDialog.h \
        ui/InputPasswordDialog.h \
        ui/KSTChatWidget.h \
        ui/KSTHighlightRuleDetail.h \
        ui/KSTHighlighterSettingDialog.h \
        ui/LogbookWidget.h \
        ui/MainWindow.h \
        ui/MapWebChannelHandler.h \
        ui/MapWidget.h \
        ui/ModeSelectionController.h \
        ui/NewContactWidget.h \
        ui/OnlineMapWidget.h \
        ui/PaperQSLDialog.h \
        ui/ProfileImageWidget.h \
        ui/QSLImportStatDialog.h \
        ui/QSODetailDialog.h \
        ui/QSOFilterDetail.h \
        ui/QSOFilterDialog.h \
        ui/QTableQSOView.h \
        ui/ShowUploadDialog.h \
        ui/SplashScreen.h \
        ui/RigWidget.h \
        ui/RotatorWidget.h \
        ui/SettingsDialog.h \
        ui/StatisticsWidget.h \
        ui/UploadQSODialog.h \
        ui/WebEnginePage.h \
        ui/WsjtxFilterDialog.h \
        ui/WsjtxWidget.h \
        i18n/dbstrings.tri \
        i18n/datastrings.tri \
        ui/component/BaseDoubleSpinBox.h \
        ui/component/ButtonStyle.h \
        ui/component/EditLine.h \
        ui/component/FreqQSpinBox.h \
        ui/component/MultiselectCompleter.h \
        ui/component/RepeatButton.h \
        ui/component/ShutdownAwareWidget.h \
        ui/component/SmartSearchBox.h \
        ui/component/SqlHighlighter.h \
        ui/component/StyleItemDelegate.h \
        ui/component/SwitchButton.h

FORMS += \
        ui/ActivityEditor.ui \
        ui/AlertRuleDetail.ui \
        ui/AlertSettingDialog.ui \
        ui/AlertWidget.ui \
        ui/AwardsDialog.ui \
        ui/DXCCSubmissionDialog.ui \
        ui/BandmapWidget.ui \
        ui/CWConsoleWidget.ui \
        ui/ChatWidget.ui \
        ui/ClockWidget.ui \
        ui/ColumnSettingDialog.ui \
        ui/ColumnSettingSimpleDialog.ui \
        ui/DevToolsDialog.ui \
        ui/DownloadQSLDialog.ui \
        ui/DxFilterDialog.ui \
        ui/DxWidget.ui \
        ui/EditActivitiesDialog.ui \
        ui/CabrilloExportDialog.ui \
        ui/CabrilloTemplateDialog.ui \
        ui/ExportDialog.ui \
        ui/ExportPasswordDialog.ui \
        ui/LoadDatabaseDialog.ui \
        ui/PlatformSettingsDialog.ui \
        ui/QSLGalleryDialog.ui \
        ui/QSLPrintLabelDialog.ui \
        ui/RigctldAdvancedDialog.ui \
        ui/ImportDialog.ui \
        ui/InputPasswordDialog.ui \
        ui/KSTChatWidget.ui \
        ui/KSTHighlightRuleDetail.ui \
        ui/KSTHighlighterSettingDialog.ui \
        ui/LogbookWidget.ui \
        ui/MainWindow.ui \
        ui/NewContactWidget.ui \
        ui/PaperQSLDialog.ui \
        ui/ProfileImageWidget.ui \
        ui/QSLImportStatDialog.ui \
        ui/QSODetailDialog.ui \
        ui/QSOFilterDetail.ui \
        ui/QSOFilterDialog.ui \
        ui/RigWidget.ui \
        ui/RotatorWidget.ui \
        ui/SettingsDialog.ui \
        ui/ShowUploadDialog.ui \
        ui/StatisticsWidget.ui \
        ui/UploadQSODialog.ui \
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
    res/qlog.desktop \
    res/qlog.1 \
    res/io.github.foldynl.QLog.metainfo.xml

TRANSLATIONS = i18n/qlog_cs.ts \
               i18n/qlog_de.ts \
               i18n/qlog_es.ts \
               i18n/qlog_fr.ts \
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

!isEmpty(ZLIBINCLUDEPATH) {
   INCLUDEPATH += $$ZLIBINCLUDEPATH
}

!isEmpty(OPENSSLINCLUDEPATH) {
   INCLUDEPATH += $$OPENSSLINCLUDEPATH
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

!isEmpty(ZLIBLIBPATH) {
   LIBS += -L$$ZLIBLIBPATH
}

!isEmpty(OPENSSLLIBPATH) {
   LIBS += -L$$OPENSSLLIBPATH
}

unix:!macx {
   isEmpty(PREFIX) {
     PREFIX = /usr/local
   }

   target.path = $$PREFIX/bin

   desktop.path = $$PREFIX/share/applications/
   desktop.files += res/$${TARGET}.desktop

   manpage.path = $$PREFIX/share/man/man1
   manpage.files += res/$${TARGET}.1

   icon.path = $$PREFIX/share/icons/hicolor/256x256/apps
   icon.files += res/$${TARGET}.png

   metainfo.path = $$PREFIX/share/metainfo/
   metainfo.files += res/io.github.foldynl.QLog.metainfo.xml

   INSTALLS += target desktop icon metainfo manpage

   INCLUDEPATH += /usr/local/include
   PKGCONFIG += openssl
   LIBS += -L/usr/local/lib -lhamlib -lsqlite3 -lz
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
}

macx: {
   # This allows the app to be shipped in a non-bundeled version
   !isEmpty(PREFIX) {
      target.path = $$PREFIX
      INSTALLS += target
   }

   INCLUDEPATH += /usr/local/include /opt/homebrew/include /opt/local/include
   LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lhamlib -lsqlite3 -lz -L/opt/local/lib -lssl -lcrypto
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain
   DISTFILES +=
}

win32: {
   INCLUDEPATH += \
        /usr/local/include \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/

   SOURCES += \
        rig/drivers/OmnirigRigDrv.cpp \
        rig/drivers/Omnirigv2RigDrv.cpp \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.c


   HEADERS += \
        rig/drivers/OmnirigRigDrv.h \
        rig/drivers/Omnirigv2RigDrv.h \
        rig/drivers/OmniRigEventSink.h \
        $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/sqlite/sqlite3.h

   TARGET = qlog
   QMAKE_TARGET_COMPANY = OK1MLG
   QMAKE_TARGET_DESCRIPTION = Hamradio logging

   LIBS += -lws2_32 -llibhamlib-4 -lzlib -llibssl -llibcrypto
   equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
   equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain

   DEFINES += WIN32_LEAN_AND_MEAN
   DEFINES += ZD_EXPORT=
}

DEFINES += HAMLIBVERSION_MAJOR=$$HAMLIBVERSION_MAJOR
DEFINES += HAMLIBVERSION_MINOR=$$HAMLIBVERSION_MINOR
DEFINES += HAMLIBVERSION_PATCH=$$HAMLIBVERSION_PATCH

DISTFILES += \
    Changelog \
    i18n/dbstrings.tri \
    res/data/sat_modes
