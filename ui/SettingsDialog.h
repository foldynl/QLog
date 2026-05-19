#ifndef QLOG_UI_SETTINGSDIALOG_H
#define QLOG_UI_SETTINGSDIALOG_H

#include <QAbstractItemView>
#include <QDialog>
#include <functional>
#include <QModelIndex>
#include <QSqlTableModel>
#include <QCompleter>
#include <QTimer>
#include <hamlib/rig.h>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>

#include "data/StationProfile.h"
#include "data/RigProfile.h"
#include "data/RotProfile.h"
#include "data/AntProfile.h"
#include "data/CWKeyProfile.h"
#include "data/CWShortcutProfile.h"
#include "data/RotUsrButtonsProfile.h"
#include "core/LogLocale.h"
#include "ui/MainWindow.h"
#include "ui/component/MultiselectCompleter.h"
#include "rig/RigCaps.h"

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
    void cwModeChanged(int);
    void rigStackWidgetChanged(int);
    void rotStackWidgetChanged(int);
    void cwKeyStackWidgetChanged(int);
    void tqslPathBrowse();
    void tqslAutoDetect();
    void updateTQSLVersionLabel();
    void stationCallsignChanged();
    void adjustLocatorTextColor();
    void adjustVUCCLocatorTextColor();
    void adjustRotCOMPortTextColor();
    void adjustRigCOMPortTextColor();
    void adjustCWKeyCOMPortTextColor();
    void adjustOperatorTextColor();
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
    void updateDateFormatResult();
    void rigFlowControlChanged(int);
    void showRigctldAdvanced();
    void rigShareChanged(int);

    void qrzAddCallsignAPIKey();
    void qrzDelCallsignAPIKey();

    void onDeleteAllPasswords();
    void onDeleteAllQSOs();

private:
    void readSettings();
    void writeSettings();
    void setUIBasedOnRigCaps(const RigCaps&);
    void refreshRigAssignedCWKeyCombo();
    void updateRigShareEnabled();
    void setValidationResultColor(QLineEdit *);
    void generateMembershipCheckboxes();
    static void refreshProfileView(QAbstractItemView *view, const QStringList &names);
    static void disableCapCheckbox(QCheckBox *checkbox);
    static void initProfileListView(QAbstractItemView *view);
    static void populateFlowControlCombo(QComboBox *combo);
    static void populateParityCombo(QComboBox *combo);
    static void populateSignalCombo(QComboBox *combo);
    static void setComboByData(QComboBox *combo, const QVariant &data, int fallback = 0);
    static void updateOffsetSpinBox(QCheckBox *checkbox, QDoubleSpinBox *spinBox);
    static void deleteSelectedProfiles(QAbstractItemView *view,
                                       const std::function<void(const QString &)> &removeProfile);
    void generateQRZAPICallsignTable();
    void saveQRZAPICallsignTable();
    void updateCountyCompleter(int dxcc);

    static constexpr int STACKED_WIDGET_SERIAL_SETTING          = 0;
    static constexpr int STACKED_WIDGET_NETWORK_SETTING         = 1;
    static constexpr int STACKED_WIDGET_SPECIAL_OMNIRIG_SETTING = 2;

    static constexpr int RIGPORT_SERIAL_INDEX          = 0;
    static constexpr int RIGPORT_NETWORK_INDEX         = 1;
    static constexpr int RIGPORT_SPECIAL_OMNIRIG_INDEX = 2;

    static constexpr int ROTPORT_SERIAL_INDEX  = 0;
    static constexpr int ROTPORT_NETWORK_INDEX = 1;

    static constexpr int RIG_NET_DEFAULT_PORT   = 4532;
    static constexpr int ROT_NET_DEFAULT_PORT   = 4533;
    static constexpr int ROT_NET_DEFAULT_PSTROT = 12000;
    static constexpr int CW_NET_CWDAEMON_PORT   = 6789;
    static constexpr int CW_NET_FLDIGI_PORT     = 7362;
    static constexpr int CW_DEFAULT_KEY_SPEED   = 20;
    static constexpr int CW_KEY_SPEED_DISABLED  = 0;

    static constexpr int PTT_TYPE_NONE_INDEX    = 0;
    static constexpr int PTT_TYPE_CAT_INDEX     = 1;
    static constexpr int CIVADDR_DISABLED_VALUE = -1;

    static constexpr const char* EMPTY_CWKEY_PROFILE = " ";

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
    QCompleter *countyCompleter;
    QList<QCheckBox*> memberListCheckBoxes;
    Ui::SettingsDialog *ui;
    LogLocale locale;
    bool sotaFallback;
    bool potaFallback;
    bool wwffFallback;
    QString rigctldPath;
    QString rigctldArgs;
    QTimer *tqslVersionTimer;
};

#endif // QLOG_UI_SETTINGSDIALOG_H
