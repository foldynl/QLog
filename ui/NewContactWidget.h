#ifndef NEWCONTACTWIDGET_H
#define NEWCONTACTWIDGET_H

#include <QWidget>
#include <QSqlRecord>
#include <QCompleter>
#include <QComboBox>
#include "data/Data.h"
#include "core/Gridsquare.h"
#include "data/DxSpot.h"
#include "core/Rig.h"
#include "core/CallbookManager.h"
#include "data/StationProfile.h"
#include "core/PropConditions.h"

namespace Ui {
class NewContactWidget;
}

enum CoordPrecision {
    COORD_NONE = 0,
    COORD_DXCC = 1,
    COORD_GRID = 2,
    COORD_FULL = 3
};

class NewContactWidget : public QWidget {
    Q_OBJECT

public:
    explicit NewContactWidget(QWidget *parent = nullptr);
    ~NewContactWidget();

    void assignPropConditions(PropConditions *);
    QString getCallsign() const;
    QString getName() const;
    QString getRST() const;
    QString getGreeting() const;
    QString getMyCallsign() const;
    QString getMyName() const;
    QString getMyQTH() const;
    QString getMyLocator() const;
    QString getMySIG() const;
    QString getMySIGInfo() const;
    QString getMyIOTA() const;
    QString getMySOTA() const;
    QString getMyPOTA() const;
    QString getMyWWFT() const;
    QString getMyVUCC() const;
    QString getMyPWR() const;
    double getQSOBearing() const;


signals:
    void contactAdded(QSqlRecord record);
    void newTarget(double lat, double lon);
    void filterCallsign(QString call);
    void userFrequencyChanged(VFOID, double, double, double);
    void markQSO(DxSpot spot);

    void stationProfileChanged();
    void rigProfileChanged();
    void antProfileChanged();

public slots:
    void refreshRigProfileCombo();
    void saveExternalContact(QSqlRecord record);
    void readGlobalSettings();
    void tuneDx(QString callsign, double frequency);
    void showDx(QString callsign, QString grid);
    void resetContact();
    void saveContact();

    // to receive RIG instructions
    void changeFrequency(VFOID, double, double, double);
    void changeMode(VFOID, QString rawMode, QString mode, QString subMode, qint32 width);
    void changePower(VFOID, double power);
    void rigConnected();
    void rigDisconnected();
    void nearestSpot(const DxSpot &);
    void setNearestSpotColor(const QString &call);
    void setManualMode(bool);
    void exitManualMode();

private slots:
    void callsignChanged();
    void frequencyTXChanged();
    void frequencyRXChanged();
    void bandChanged();
    void modeChanged();
    void subModeChanged();
    void gridChanged();
    void updateTime();
    void updateTimeOff();
    void startContactTimer();
    void stopContactTimer();
    void markContact();
    void editCallsignFinished();
    void callsignResult(const QMap<QString, QString>& data);
    void propModeChanged(const QString&);
    void sotaChanged(QString);
    void sotaEditFinished();
    void potaChanged(QString);
    void potaEditFinished();
    void wwffEditFinished();
    void wwffChanged(QString);
    void formFieldChangedString(const QString&);
    void formFieldChanged();
    void useNearestCallsign();
    void timeOnChanged();

    void stationProfileComboChanged(QString);
    void rigProfileComboChanged(QString);
    void antProfileComboChanged(QString);
    void qrz();

private:
    void fillFieldsFromLastQSO(QString callsign);
    void queryDxcc(QString callsign);
    void clearCallbookQueryFields();
    void readWidgetSettings();
    void writeWidgetSetting();
    void __modeChanged(qint32);
    void refreshStationProfileCombo();
    void updateTXBand(double freq);
    void updateRXBand(double freq);
    void updateCoordinates(double lat, double lon, CoordPrecision prec);
    void updateDxccStatus();
    void updatePartnerLocTime();
    void setDefaultReport();
    void refreshAntProfileCombo();
    void addAddlFields(QSqlRecord &record, const StationProfile &profile);
    bool eventFilter(QObject *object, QEvent *event);
    bool isQSOTimeStarted();
    void QSYContactWiping(double);
    void connectFieldChanged();
    void changeCallsignManually(const QString &);
    void changeCallsignManually(const QString &, double);
    void __changeFrequency(VFOID, double vfoFreq, double ritFreq, double xitFreq);
    void showRXTXFreqs(bool);
    void setComboBaseData(QComboBox *, const QString &);

private:
    Rig* rig;
    double realRigFreq;
    double realFreqForManualExit;
    QString callsign;
    DxccEntity dxccEntity;
    QString defaultReport;
    CallbookManager callbookManager;
    QTimer* contactTimer;
    Ui::NewContactWidget *ui;
    CoordPrecision coordPrec;
    PropConditions *prop_cond;
    QCompleter *iotaCompleter;
    QCompleter *satCompleter;
    QCompleter *sotaCompleter;
    QCompleter *potaCompleter;
    QCompleter *wwffCompleter;
    QTimeZone partnerTimeZone;
    double QSOFreq;
    qint32 bandwidthFilter;
    bool rigOnline;
    QMap<QString, QString> lastCallbookQueryData;
    SOTAEntity lastSOTA;
    POTAEntity lastPOTA;
    WWFFEntity lastWWFF;
    bool isManualEnterMode;
};

#endif // NEWCONTACTWIDGET_H
