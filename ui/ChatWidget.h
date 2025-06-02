#ifndef QLOG_UI_CHATWIDGET_H
#define QLOG_UI_CHATWIDGET_H

#include <QWidget>
#include "service/kstchat/KSTChat.h"
#include "ui/NewContactWidget.h"

namespace Ui {
class ChatWidget;
}

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    void registerContactWidget(const NewContactWidget *contactWidget);

public slots:
    void setChatCallsign(QString);
    void reloadStationProfile();
    void resetDupe();
    void recalculateDupe();
    void recalculateDxccStatus();
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsDupeWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities);

private slots:
    void connectChat();
    void closeTab(int);
    void tabActive(QWidget *);
    void valuableMessageActive(QWidget *);
    void chatTabClicked(int);
    void processQSOInfo(const QString&, const QString&);
    void userListUpdate(QWidget *w);
    void beamRequest(double);

signals:
    void prepareQSOInfo(QString, QString);
    void userListUpdated(QList<KSTUsersInfo>);
    void beamingRequested(double);

private:
    Ui::ChatWidget *ui;
    const NewContactWidget *contact;

    int findTabWidgetIndex(QWidget *);
    QString generateTabName(QWidget *);
    QString generateTabUnread(QWidget *);
    void setTabUnreadInfo(int, QWidget *);
};

#endif // QLOG_UI_CHATWIDGET_H
