#ifndef QLOG_UI_SETTINGSDIALOG_H
#define QLOG_UI_SETTINGSDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QSqlTableModel>
#include <QCompleter>
#include <hamlib/rig.h>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

#include "data/StationProfile.h"
#include "data/RigProfile.h"
#include "data/RotProfile.h"
#include "data/AntProfile.h"
#include "data/CWKeyProfile.h"
#include "data/CWShortcutProfile.h"
#include "data/RotUsrButtonsProfile.h"
#include "core/LogLocale.h"
#include "rig/Rig.h"
#include "ui/MainWindow.h"
#include "core/MultiselectCompleter.h"

namespace Ui {
class SettingsDialog;
}

class QSqlTableModel;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(MainWindow *parent = nullptr);
    ~SettingsDialog();

public slots:
    void save();

    void addRigProfile();
    void delRigProfile();
    void refreshRigProfilesView();
    void doubleClickRigProfile(QModelIndex);
    void clearRigProfileForm();
    void rigRXOffsetChanged(int);
    void rigTXOffsetChanged(int);
    void rigGetFreqChanged(int);
    void rigPortTypeChanged(int);
    void rigInterfaceChanged(int);
    void rigPTTTypeChanged(int);

    void addRotProfile();
    void delRotProfile();
    void refreshRotProfilesView();
    void doubleClickRotProfile(QModelIndex);
    void clearRotProfileForm();
    void rotPortTypeChanged(int);
    void rotInterfaceChanged(int);

    void addRotUsrButtonsProfile();
    void delRotUsrButtonsProfile();
    void refreshRotUsrButtonsProfilesView();
    void doubleClickRotUsrButtonsProfile(QModelIndex);
    void clearRotUsrButtonsProfileForm();

    void addAntProfile();
    void delAntProfile();
    void refreshAntProfilesView();
    void doubleClickAntProfile(QModelIndex);
    void clearAntProfileForm();

    void addCWKeyProfile();
    void delCWKeyProfile();
    void refreshCWKeyProfilesView();
    void doubleClickCWKeyProfile(QModelIndex);
    void clearCWKeyProfileForm();

    void addCWShortcutProfile();
    void delCWShortcutProfile();
    void refreshCWShortcutProfilesView();
    void doubleClickCWShortcutProfile(QModelIndex);
    void clearCWShortcutProfileForm();

    void refreshStationProfilesView();
    void addStationProfile();
    void deleteStationProfile();
    void doubleClickStationProfile(QModelIndex);
    void clearStationProfileForm();

    void rigChanged(int);
    void rotChanged(int);
    void cwKeyChanged(int);
    void rigStackWidgetChanged(int);
    void rotStackWidgetChanged(int);
    void cwKeyStackWidgetChanged(int);
    void tqslPathBrowse();
    void stationCallsignChanged();
    void adjustLocatorTextColor();
    void adjustVUCCLocatorTextColor();
    void adjustRotCOMPortTextColor();
    void adjustRigCOMPortTextColor();
    void adjustCWKeyCOMPortTextColor();
    void cancelled();
    void sotaChanged(const QString&);
    void sotaEditFinished();
    void potaChanged(const QString&);
    void potaEditFinished();
    void wwffChanged(const QString&);
    void wwffEditFinished();
    void primaryCallbookChanged(int);
    void secondaryCallbookChanged(int);
    void assignedKeyChanged(int);
    void testWebLookupURL();
    void joinMulticastChanged(int);
    void adjustWSJTXMulticastAddrTextColor();
    void hrdlogSettingChanged();
    void clublogSettingChanged();

private:
    void readSettings();
    void writeSettings();
    void setUIBasedOnRigCaps(const RigCaps&);
    void refreshRigAssignedCWKeyCombo();
    void setValidationResultColor(QLineEdit *);
    QString getMemberListComboValue(const QComboBox *);
    void generateMembershipCheckboxes();

    QSqlTableModel* modeTableModel;
    QSqlTableModel* bandTableModel;
    StationProfilesManager *stationProfManager;
    RigProfilesManager *rigProfManager;
    RotProfilesManager *rotProfManager;
    AntProfilesManager *antProfManager;
    CWKeyProfilesManager *cwKeyProfManager;
    CWShortcutProfilesManager *cwShortcutProfManager;
    RotUsrButtonsProfilesManager *rotUsrButtonsProfManager;
    QCompleter *sotaCompleter;
    QCompleter *iotaCompleter;
    QCompleter *wwffCompleter;
    MultiselectCompleter *potaCompleter;
    QCompleter *sigCompleter;
    QList<QCheckBox*> memberListCheckBoxes;
    Ui::SettingsDialog *ui;
    LogLocale locale;
    bool sotaFallback;
    bool potaFallback;
    bool wwffFallback;
};

#endif // QLOG_UI_SETTINGSDIALOG_H
