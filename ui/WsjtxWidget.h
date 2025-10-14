#ifndef QLOG_UI_WSJTXWIDGET_H
#define QLOG_UI_WSJTXWIDGET_H

#include <QWidget>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include "data/WsjtxEntry.h"
#include "models/WsjtxTableModel.h"
#include "rig/Rig.h"
#include "component/ShutdownAwareWidget.h"

#include "core/LogLocale.h"

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
    void tableViewDoubleClicked(QModelIndex);
    void callsignClicked(QString);
    void tableViewClicked(QModelIndex);
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);

private slots:
    void displayedColumns();
    void actionFilter();

signals:
    void callsignSelected(QString callsign, QString grid);
    void reply(WsjtxDecode);
    void CQSpot(WsjtxEntry);
    void filteredCQSpot(WsjtxEntry);
    void spotsCleared();
    void frequencyChanged(VFOID, double, double, double);
    void modeChanged(VFOID, QString, QString, QString, qint32);

private:
    uint dxccStatusFilterValue() const;
    QString contFilterRegExp() const;
    int getDistanceFilterValue() const;
    int getSNRFilterValue() const;
    QStringList dxMemberList() const;
    void reloadSetting();
    void clearTable();

    WsjtxTableModel* wsjtxTableModel;
    WsjtxStatus status;
    QString currBand;
    double currFreq;
    Ui::WsjtxWidget *ui;
    QSortFilterProxyModel *proxyModel;
    QRegularExpression contregexp;
    QRegularExpression cqRE;
    int distanceFilter;
    int snrFilter;
    uint dxccStatusFilter;
    QSet<QString> dxMemberFilter;
    LogLocale locale;

    void saveTableHeaderState();
    void restoreTableHeaderState();
    bool isFilterEnabled();
};

#endif // QLOG_UI_WSJTXWIDGET_H
