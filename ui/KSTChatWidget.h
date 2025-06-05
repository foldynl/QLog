#ifndef QLOG_UI_KSTCHATWIDGET_H
#define QLOG_UI_KSTCHATWIDGET_H

#include <QWidget>
#include <QPointer>

#include <QPainterPath>
#include <QAbstractTextDocumentLayout>

#include <QListView>
#include <QMainWindow>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWidget>
#include <QMargins>
#include <QPainter>

#include "service/kstchat/KSTChat.h"
#include "ui/NewContactWidget.h"

namespace Ui {
class KSTChatWidget;
}

class ChatMessageModel : public QAbstractListModel
{
public:
    enum MessageDirection
    {
        OUTGOING = 0,
        INCOMING = 1,
        INCOMING_TOYOU = 2,
        INCOMING_HIGHLIGHT = 3
    };

public:

    explicit ChatMessageModel(QObject* parent = nullptr) :
        QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    void addMessage(MessageDirection direction,
                    const KSTChatMsg &msg);
    void clear();
    KSTChatMsg getMessage(const QModelIndex &index) const;

private:
    QList<QPair<int, KSTChatMsg>> messages;
};

class HTMLDelegate : public QStyledItemDelegate
{
public:
    explicit HTMLDelegate(QObject* parent = nullptr) :
        QStyledItemDelegate(parent){};

protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & inOption, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & inOption, const QModelIndex & index ) const;
};

class MessageDelegate : public QStyledItemDelegate
{

public:

    explicit MessageDelegate(QObject* parent = nullptr) :
        QStyledItemDelegate(parent),
        d_radius(10),
        d_toppadding(0),
        d_bottompadding(0),
        d_leftpadding(2),
        d_rightpadding(2),
        d_verticalmargin(2),
        d_horizontalmargin(2),
        d_pointerwidth(5),
        d_pointerheight(5),
        d_widthfraction(0.95) {}

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    int d_radius;
    int d_toppadding;
    int d_bottompadding;
    int d_leftpadding;
    int d_rightpadding;
    int d_verticalmargin;
    int d_horizontalmargin;
    int d_pointerwidth;
    int d_pointerheight;
    float d_widthfraction;
};

class UserListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    UserListModel(QObject* parent = 0) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    void updateList(const QList<KSTUsersInfo> &userList);
    void clear();

    KSTUsersInfo getUserInfo(const QModelIndex &index) const;

private:
    QList<KSTUsersInfo> userData;
};

class KSTChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KSTChatWidget(int chatRoomIndex,
                           const QString& username,
                           const QString& password,
                           const NewContactWidget *,
                           QWidget *parent = nullptr);
    ~KSTChatWidget();

    QList<KSTUsersInfo> getUserList();

signals:
    void chatClosed();
    void chatUpdated(QWidget *);
    void valuableMessageUpdated(QWidget *);
    void prepareQSOInfo(QString, QString);
    void userListUpdated(QWidget *);
    void beamingRequested(double);

public slots:
    void addChatMessage(KSTChatMsg);
    void sendMessage();
    void updateUserList();
    void setPrivateChatCallsign(QString);
    void reloadStationProfile();
    void setBeamActionVisible(bool);
    void resetDupe();
    void recalculateDupe();
    void recalculateDxccStatus();
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsStatusWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities);

private slots:
    void showChatError(const QString &);
    void closeChat();
    void displayedColumns();
    void userDoubleClicked(QModelIndex);
    void messageDoubleClicked(QModelIndex);
    void prefillQSOAction();
    void highlightPressed();
    bool isHighlightCandidate(KSTChatMsg &);
    void editHighlightRules();
    void resetPressed();
    void beamingRequest();
    void clearValuableMessages();
private:
    Ui::KSTChatWidget *ui;
    QPointer<ChatMessageModel> messageModel;
    QPointer<ChatMessageModel> valuableMessageModel;
    QPointer<KSTChat> chat;
    UserListModel* userListModel;
    QSortFilterProxyModel * proxyModel;
    QPointer<chatHighlightEvaluator> highlightEvaluator;
    QString userName;

    void setSelectedCallsignInUserList(const QString &);
};

#endif // QLOG_UI_KSTCHATWIDGET_H
