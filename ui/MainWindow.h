#ifndef QLOG_UI_MAINWINDOW_H
#define QLOG_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QActionGroup>
#include "ui/StatisticsWidget.h"
#include "core/NetworkNotification.h"
#include "core/AlertEvaluator.h"
#include "core/PropConditions.h"
#include "service/clublog/ClubLog.h"

namespace Ui {
class MainWindow;
}

class QLabel;
class WsjtxUDPReceiver;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    void closeEvent(QCloseEvent* event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

    QList<QAction *> getUserDefinedShortcutActionList();
    QStringList getBuiltInStaticShortcutList() const;

signals:
    void settingsChanged();
    void themeChanged(int themeMode, bool isDark);
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
    void checkNewVersion();

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
    void showAwards();
    void showDXCCSubmission();
    void showAbout();
    void showWhatsNew();
    void showWikiHelp();
    void showMailingList();
    void showReportBug();
    void showAlerts();
    void clearAlerts();
    void conditionsUpdated();
    void QSOFilterSetting();
    void alertRuleSetting();
    void processSpotAlert(SpotAlert alert);
    void clearAlertEvent();
    void beepSettingAlerts();
    void shortcutALTBackslash();
    void setManualContact(bool);
    void showEditLayout();
    void showServiceUpload();
    void showServiceDownloadQSL();
    void showDumpDB();
    void showLoadDB();
    void showQSLGallery();
    void showDevTools();
    void printQslLabels();

    void saveProfileLayoutGeometry();
    void setEquipmentKeepOptions(bool);

    void saveContestMenuSeqnoType(QAction *action);
    void saveContestMenuDupeType(QAction *action);
    void saveContestMenuLinkExchangeType(QAction *action);
    void startContest(const QString contestID, const QDateTime);
    void stopContest();
    void exportCabrillo();
    void setContestMode(const QString &contestID);

    void handleActivityChange(const QString name);

    void openNonVfoBandmap(const QString &widgetID, const QString& bandName);

    void showUpdateDialog(const QString &newVersion, const QString &repoName);

private:
    Ui::MainWindow* ui;
    QLabel* conditionsLabel;
    QLabel* profileLabel;
    QLabel* callsignLabel;
    QLabel* locatorLabel;
    QLabel* contestLabel;
    QPushButton* alertButton;
    QPushButton* alertTextButton;
    QPushButton *themeButton;
    StatisticsWidget* stats;
    NetworkNotification networknotification;
    AlertEvaluator alertEvaluator;
    PropConditions *conditions;
    bool isFusionStyle;
    ClubLogUploader* clublogRT;
    WsjtxUDPReceiver* wsjtx;
    QActionGroup *seqGroup;
    QActionGroup *dupeGroup;
    QActionGroup *linkExchangeGroup;
    QPushButton *activityButton;
    QMetaObject::Connection alertTextButtonConn;
    bool firstRun = false;
    void setupActivitiesMenu();


    void restoreUserDefinedShortcuts();
    void saveUserDefinedShortcuts();

    void restoreContestMenuSeqnoType();
    void restoreContestMenuDupeType();
    void restoreContestMenuLinkExchange();

    QString stationCallsignStatus(const StationProfile &profile) const;

    void openNonVfoBandmaps(const QList<QPair<QString, QString>> &list);
    void clearNonVfoBandmaps();
    QList<QPair<QString, QString>> getNonVfoBandmapsParams() const;
    void themeInit(int mode);
    bool setNativeTheme();
    void setLightTheme();
    void setDarkTheme();

    void showEvent(QShowEvent *event) override;

    void restartApplication();
};

#endif // QLOG_UI_MAINWINDOW_H
