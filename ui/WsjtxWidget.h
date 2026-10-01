#ifndef QLOG_UI_WSJTXWIDGET_H
#define QLOG_UI_WSJTXWIDGET_H

#include <QWidget>
#include <QAbstractTableModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include "data/WsjtxEntry.h"
#include "models/WsjtxTableModel.h"
#include "rig/Rig.h"
#include "component/ShutdownAwareWidget.h"

#include "core/LogLocale.h"
#include "data/WsjtxStatus.h"
#include "data/DxccLogStatusCondition.h"

namespace Ui {
class WsjtxWidget;
}

class WsjtxWidget : public QWidget, public ShutdownAwareWidget
{
    Q_OBJECT

public:
    explicit WsjtxWidget(QWidget *parent = nullptr);
    ~WsjtxWidget();
    virtual void finalizeBeforeAppExit() override;

public slots:
    void decodeReceived(WsjtxDecode);
    void statusReceived(WsjtxStatus);
    void clearSpots(const QString &id);
    void clientClosed(const QString &id);
    void tableViewDoubleClicked(QModelIndex);
    void callsignClicked(QString id, QString callsign);
    void tableViewClicked(QModelIndex);
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsDxccStatus(const QSet<uint> &entities);
    void refreshStatusColors();
    void recalculateDxccStatus();

private slots:
    void displayedColumns();
    void actionFilter();

signals:
    void callsignSelected(QString callsign, QString grid, QString id);
    void reply(WsjtxEntry);
    void CQSpot(WsjtxEntry);
    void filteredCQSpot(WsjtxEntry);
    void updatedCQSpot(WsjtxEntry);
    void spotsCleared(QString id);
    void frequencyChanged(VFOID, double, double, double);
    void modeChanged(VFOID, QString, QString, QString, qint32);

private:
    struct ClientState
    {
        ClientState() : frequency(0.0), spotPeriod(120.0f), initialized(false) {}

        WsjtxStatus status;
        QString band;
        double frequency;
        float spotPeriod;
        bool initialized;
    };

    QString contFilterRegExp() const;
    int getDistanceFilterValue() const;
    int getSNRFilterValue() const;
    QStringList dxMemberList() const;
    void reloadSetting();
    void clearTable();
    void redrawMapSpots();
    void emitOwnerState(const ClientState &client,
                        bool radioChanged,
                        bool targetChanged);
    void requestReply(const WsjtxEntry &entry);

    WsjtxTableModel* wsjtxTableModel;
    QHash<QString, ClientState> clients;
    QString newQSOOwnerId;
    QString pendingReplyId;
    QString pendingReplyCallsign;
    bool ownerClaimRequired = false;
    Ui::WsjtxWidget *ui;
    QSortFilterProxyModel *proxyModel;
    QRegularExpression contregexp;
    QRegularExpression cqRE;
    int distanceFilter;
    int snrFilter;
    DxccLogStatusCondition logStatusFilter;
    QSet<QString> dxMemberFilter;
    LogLocale locale;

    void saveTableHeaderState();
    void restoreTableHeaderState();
    bool isFilterEnabled();
};

#endif // QLOG_UI_WSJTXWIDGET_H
