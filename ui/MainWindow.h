#ifndef QLOG_UI_MAINWINDOW_H
#define QLOG_UI_MAINWINDOW_H

#include <QMainWindow>
#include "ui/StatisticsWidget.h"
#include "ui/SwitchButton.h"
#include "core/NetworkNotification.h"
#include "core/AlertEvaluator.h"
#include "core/PropConditions.h"
#include "core/ClubLog.h"

namespace Ui {
class MainWindow;
}

class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    void closeEvent(QCloseEvent* event);
    void keyReleaseEvent(QKeyEvent *event);
    QList<QAction *> getUserDefinedShortcutActionList();
    QStringList getBuiltInStaticShortcutList() const;

signals:
    void settingsChanged();
    void themeChanged(int);
    void altBackslash(bool active);
    void manualMode(bool);
    void contestStopped();
    void dupeTypeChanged();

public slots:
    void rigErrorHandler(const QString &error, const QString &errorDetail);
    void rotErrorHandler(const QString &error, const QString &errorDetail);
    void cwKeyerErrorHandler(const QString &error, const QString &errorDetail);
    void stationProfileChanged();
    void setLayoutGeometry();
    void setSimplyLayoutGeometry();
    void aboutToQuit();
    void checkBandmapWidgets();

private slots:
    void rigConnect();
    void rotConnect();
    void cwKeyerConnect();
    void cwKeyerConnectProfile(QString);
    void cwKeyerDisconnectProfile(QString);
    void showSettings();
    void showStatistics();
    void importLog();
    void exportLog();
    void showLotw();
    void showeQSL();
    void showClublog();
    void showHRDLog();
    void showQRZ();
    void showAwards();
    void showAbout();
    void showWikiHelp();
    void showMailingList();
    void showReportBug();
    void showAlerts();
    void clearAlerts();
    void conditionsUpdated();
    void QSOFilterSetting();
    void alertRuleSetting();
    void darkModeToggle(int);
    void processSpotAlert(SpotAlert alert);
    void clearAlertEvent();
    void beepSettingAlerts();
    void shortcutALTBackslash();
    void setManualContact(bool);
    void showEditLayout();

    void saveProfileLayoutGeometry();
    void setEquipmentKeepOptions(bool);

    void saveContestMenuSeqnoType(QAction *action);
    void saveContestMenuDupeType(QAction *action);
    void saveContestMenuLinkExchangeType(QAction *action);
    void startContest(const QString contestID, const QDateTime);
    void stopContest();
    void setContestMode(const QString &contestID);

    void handleActivityChange(const QString name);

    void openNonVfoBandmap();

    void visibilityNonVfoBandmap(bool, QDockWidget *);

private:
    Ui::MainWindow* ui;
    QLabel* conditionsLabel;
    QLabel* profileLabel;
    QLabel* callsignLabel;
    QLabel* locatorLabel;
    QLabel* contestLabel;
    QPushButton* alertButton;
    QPushButton* alertTextButton;
    SwitchButton* darkLightModeSwith;
    QLabel* darkIconLabel;
    StatisticsWidget* stats;
    NetworkNotification networknotification;
    AlertEvaluator alertEvaluator;
    PropConditions *conditions;
    QSettings settings;
    bool isFusionStyle;
    ClubLog* clublogRT;
    Wsjtx* wsjtx;
    QActionGroup *seqGroup;
    QActionGroup *dupeGroup;
    QActionGroup *linkExchangeGroup;
    QPushButton *activityButton;
    bool ignoreBandmapVisibility;

    void setDarkMode();
    void setLightMode();

    void setupActivitiesMenu();
    void saveEquipmentConnOptions();
    void restoreConnectionStates();
    void restoreEquipmentConnOptions();

    void restoreUserDefinedShortcuts();
    void saveUserDefinedShortcuts();

    void restoreContestMenuSeqnoType();
    void restoreContestMenuDupeType();
    void restoreContestMenuLinkExchange();
    void restoreNonVfoBandmaps();

    QString stationCallsignStatus(const StationProfile &profile) const;

    void initializeNonVfoBandmap(const QString dockId);
};

#endif // QLOG_UI_MAINWINDOW_H
