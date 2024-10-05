#ifndef QLOG_UI_DXWIDGET_H
#define QLOG_UI_DXWIDGET_H

#include <QWidget>
#include <QtNetwork>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QSqlRecord>

#include "data/DxSpot.h"
#include "data/WCYSpot.h"
#include "data/WWVSpot.h"
#include "data/ToAllSpot.h"
#include "core/LogLocale.h"
#include "core/DxServerString.h"
#include "models/SearchFilterProxyModel.h"

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
    bool addEntry(DxSpot entry,
                  bool deduplicate = false,
                  qint16 dedup_interval = DEDUPLICATION_TIME,
                  double freq_tolerance = DEDUPLICATION_FREQ_TOLERANCE);
    QString getCallsign(const QModelIndex& index);
    double getFrequency(const QModelIndex& index);
    BandPlan::BandPlanMode getBandPlanode(const QModelIndex& index);
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
    void addEntry(WCYSpot entry);
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
    void addEntry(WWVSpot entry);
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
    void addEntry(ToAllSpot entry);
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

protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

class DxWidget : public QWidget {
    Q_OBJECT

public:
    explicit DxWidget(QWidget *parent = 0);
    ~DxWidget();

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

signals:
    void tuneDx(QString, double, BandPlan::BandPlanMode);
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
};

#endif // QLOG_UI_DXWIDGET_H
