#ifndef QLOG_UI_DXWIDGET_H
#define QLOG_UI_DXWIDGET_H

#include <QWidget>
#include <QtNetwork>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QSqlRecord>
#include <QLabel>

#include "data/DxSpot.h"
#include "data/WCYSpot.h"
#include "data/WWVSpot.h"
#include "data/ToAllSpot.h"
#include "core/LogLocale.h"
#include "data/DxServerString.h"
#include "models/SearchFilterProxyModel.h"
#include "rig/Rig.h"
#include "component/ShutdownAwareWidget.h"

// in sec
#define DEDUPLICATION_TIME 3

// in kHz
#define DEDUPLICATION_FREQ_TOLERANCE 5

namespace Ui {
class DxWidget;
}

class DxTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    DxTableModel(QObject* parent = 0) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool addEntry(const DxSpot &entry,
                  bool deduplicate = false,
                  qint16 dedup_interval = DEDUPLICATION_TIME,
                  double dedup_freq_tolerance = DEDUPLICATION_FREQ_TOLERANCE);
    const DxSpot getSpot(const QModelIndex& index) const {return dxData.at(index.row());};
    void clear();

private:
    QList<DxSpot> dxData;
    LogLocale locale;
};

class WCYTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    WCYTableModel(QObject* parent = 0) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addEntry(const WCYSpot &entry);
    void clear();

private:
    QList<WCYSpot> wcyData;
    LogLocale locale;
};

class WWVTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    WWVTableModel(QObject* parent = 0) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addEntry(const WWVSpot &entry);
    void clear();

private:
    QList<WWVSpot> wwvData;
    LogLocale locale;
};

class ToAllTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    ToAllTableModel(QObject* parent = 0) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addEntry(const ToAllSpot &entry);
    void clear();

private:
    QList<ToAllSpot> toAllData;
    LogLocale locale;
};

class DeleteHighlightedDXServerWhenDelPressedEventFilter : public QObject
{
     Q_OBJECT
signals:
    void deleteServerItem();

public:
    DeleteHighlightedDXServerWhenDelPressedEventFilter(QObject *parent) :
        QObject(parent) {};

protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

class DxWidget : public QWidget, public ShutdownAwareWidget
{
    Q_OBJECT

public:
    explicit DxWidget(QWidget *parent = 0);
    ~DxWidget();
    virtual void finalizeBeforeAppExit() override;

public slots:
    void toggleConnect();
    void receive();
    void send();
    void connected();
    void socketError(QAbstractSocket::SocketError);
    void viewModeChanged(int);
    void entryDoubleClicked(QModelIndex);
    void actionFilter();
    void adjusteServerSelectSize(QString);
    void serverSelectChanged(int);
    void setLastQSO(QSqlRecord);
    void reloadSetting();
    void prepareQSOSpot(QSqlRecord);
    void setSearch(const QString &);
    void setSearchStatus(bool);
    void setSearchVisible();
    void setSearchClosed();
    void setDxTrend(QHash<QString, QHash<QString, QHash<QString, int>>>);
    void recalculateTrend();
    void setTunedFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq);

private slots:
    void actionCommandSpotQSO();
    void actionCommandShowHFStats();
    void actionCommandShowVHFStats();
    void actionCommandShowWCY();
    void actionCommandShowWWV();
    void actionConnectOnStartup();
    void actionDeleteServer();
    void actionForgetPassword();
    void actionKeepSpots();
    void actionClear();

    void displayedColumns();
    void trendDoubleClicked(int row, int column);

signals:
    void tuneDx(DxSpot);
    void tuneBand(QString);
    void newSpot(DxSpot);
    void newWCYSpot(WCYSpot);
    void newWWVSpot(WWVSpot);
    void newToAllSpot(ToAllSpot);
    void newFilteredSpot(DxSpot);

private:
    enum DXCConnectionState
    {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        LOGIN_SENT = 3,
        PASSWORD_SENT = 4,
        OPERATION = 5
    };

    DxTableModel* dxTableModel;
    WCYTableModel* wcyTableModel;
    WWVTableModel* wwvTableModel;
    ToAllTableModel* toAllTableModel;
    SearchFilterProxyModel* dxTableProxyModel;
    QTcpSocket* socket;
    Ui::DxWidget *ui;
    QRegularExpression moderegexp;
    QRegularExpression contregexp;
    QRegularExpression spottercontregexp;
    QRegularExpression bandregexp;
    uint dxccStatusFilter;
    bool deduplicateSpots;
    int deduplicatetime;
    int deduplicatefreq;

    QSet<QString> dxMemberFilter;
    QSqlRecord lastQSO;
    quint8 reconnectAttempts;
    QTimer reconnectTimer;
    DXCConnectionState connectionState;
    DxServerString *connectedServerString;
    QHash<QString, QHash<QString, QHash<QString, int>>> receivedTrendData;
    QHash<QString, QHash<QString, int>> prevTrendDataForMyCont;
    QHash<QString, QHash<QString, int>> trendDataForMyCont;
    QStringList trendBandList;
    QLabel *trendTableCornerLabel;

    void connectCluster();
    void disconnectCluster(bool tryReconnect = false);
    void saveDXCServers();
    QString modeFilterRegExp();
    QString contFilterRegExp();
    QString spotterContFilterRegExp();
    QString bandFilterRegExp();
    uint dxccStatusFilterValue();
    bool spotDedupValue();
    int getDedupTimeValue();
    int getDedupFreqValue();
    QStringList dxMemberList();
    bool getAutoconnectServer();
    void saveAutoconnectServer(bool);
    bool getKeepQSOs();
    void saveKeepQSOs(bool);
    void sendCommand(const QString&,
                     bool switchToConsole = false);
    void saveWidgetSetting();
    void restoreWidgetSetting();

    QStringList getDXCServerList(void);
    void serverComboSetup();
    void clearAllPasswordIcons();
    void activateCurrPasswordIcon();

    void processDxSpot(const QString &spotter,
                       const QString &freq,
                       const QString &call,
                       const QString &comment,
                       const QDateTime &dateTime = QDateTime());

    QVector<int> dxcListHiddenCols() const;
    BandPlan::BandPlanMode modeGroupFromComment(const QString &comment) const;
    QString refFromComment(const QString &comment, bool &flag,
                           const QRegularExpression &regEx,
                           const QString &refType, int justified) const;
    void wwffRefFromComment(DxSpot &spot) const;
    void potaRefFromComment(DxSpot &spot) const;
    void sotaRefFromComment(DxSpot &spot) const;
    void iotaRefFromComment(DxSpot &spot) const;

    QColor getHeatmapColor(int value, int maxValue);

    bool isFilterEnabled() const;
};

#endif // QLOG_UI_DXWIDGET_H
