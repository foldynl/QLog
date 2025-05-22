#include <QPushButton>
#include <QLabel>
#include <QTabBar>
#include <QMessageBox>

#include "ChatWidget.h"
#include "ui_ChatWidget.h"
#include "core/debug.h"
#include "service/kstchat/KSTChat.h"
#include "ui/KSTChatWidget.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.chatwidget");

ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->chatTabWidget->tabBar()->tabButton(0, QTabBar::RightSide)->hide();
    ui->chatRoomCombo->addItems(KSTChat::chatRooms);

    QWidget *w = ui->chatTabWidget->widget(0);
    w->setProperty("chatName", tr("New"));
    ui->chatTabWidget->setTabText(0, generateTabName(w));
    ui->chatRoomCombo->setCurrentIndex(LogParam::getChatSelectedRoom());
}

ChatWidget::~ChatWidget()
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        if ( qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i)) )
            ui->chatTabWidget->widget(i)->disconnect(SIGNAL(chatClosed()));
    }
    delete ui;
}

void ChatWidget::registerContactWidget(const NewContactWidget *contactWidget)
{
    FCT_IDENTIFICATION;
    contact = contactWidget;
}

void ChatWidget::setChatCallsign(QString callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->currentWidget());
    if ( kstWidget )
    {
        kstWidget->setPrivateChatCallsign(callsign);
    }
}

void ChatWidget::reloadStationProfile()
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->reloadStationProfile();
    }
}

void ChatWidget::resetDupe()
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->resetDupe();
    }
}

void ChatWidget::recalculateDupe()
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->recalculateDupe();
    }

}

void ChatWidget::recalculateDxccStatus()
{
    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->recalculateDxccStatus();
    }
}

void ChatWidget::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->updateSpotsStatusWhenQSOAdded(record);
    }

}

void ChatWidget::updateSpotsDupeWhenQSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->updateSpotsStatusWhenQSODeleted(record);
    }
}

void ChatWidget::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->chatTabWidget->count(); i++ )
    {
        KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(i));

        if ( kstWidget )
            kstWidget->updateSpotsDxccStatusWhenQSODeleted(entities);
    }
}

void ChatWidget::connectChat()
{
    FCT_IDENTIFICATION;

    QString username = KSTChat::getUsername();
    QString password = KSTChat::getPassword();

    if ( username.isEmpty()
         || password.isEmpty() )
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("ON4KST Chat is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
        return;
    }

    KSTChatWidget *newWidget = new KSTChatWidget(ui->chatRoomCombo->currentIndex() + 1,
                                                 username,
                                                 password,
                                                 contact,
                                                 this);
    newWidget->setProperty("chatName", ui->chatRoomCombo->currentText());
    newWidget->setProperty("unreadMsg", 0);
    newWidget->setProperty("valuableMsg", 0);
    ui->chatTabWidget->addTab(newWidget,generateTabName(newWidget));
    ui->chatTabWidget->setCurrentWidget(newWidget);
    connect(newWidget, &KSTChatWidget::chatClosed,
            this, [this, newWidget]()
    {
        int i = this->findTabWidgetIndex(newWidget);
        if ( i >= 0 )
        {
            this->closeTab(i);
        }
    });

    connect(newWidget, &KSTChatWidget::chatUpdated,
            this, &ChatWidget::tabActive);

    connect(newWidget, &KSTChatWidget::valuableMessageUpdated,
            this, &ChatWidget::valuableMessageActive);

    connect(newWidget, &KSTChatWidget::prepareQSOInfo,
            this, &ChatWidget::processQSOInfo);

    connect(newWidget, &KSTChatWidget::userListUpdated,
            this, &ChatWidget::userListUpdate);

    connect(newWidget, &KSTChatWidget::beamingRequested,
            this, &ChatWidget::beamRequest);

#if 0 // see comments in closeTab
    // Disable Chat Room in the Combobox
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->chatRoomCombo->model());
    if ( model )
    {
        QStandardItem *item = model->item(ui->chatRoomCombo->currentIndex());
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
#endif
    LogParam::setChatSelectedRoom(ui->chatRoomCombo->currentIndex());
}

void ChatWidget::closeTab(int tabIndex)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << tabIndex;

    if ( tabIndex < 0 )
        return;

    KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(ui->chatTabWidget->widget(tabIndex));
    ui->chatTabWidget->removeTab(tabIndex);

#if 0 // It is not an user friednly to disable the item after connection
    // because QLog save the last selected item and this items remains selected - it is chaotic
    // Enable Chat Room in the Combobox

    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->chatRoomCombo->model());

    if ( model )
    {
        QStandardItem *item = model->item(ui->chatRoomCombo->findText(kstWidget->property("chatName").toString()));
        item->setFlags(item->flags() | Qt::ItemIsEnabled);
    }
#endif
    kstWidget->deleteLater();
    ui->chatTabWidget->setCurrentIndex(tabIndex - 1);
    //emit userListUpdated(QList<KSTUsersInfo>());
}

void ChatWidget::tabActive(QWidget *w)
{
    FCT_IDENTIFICATION;

    if ( w == ui->chatTabWidget->currentWidget() )
        return;

    int unread = w->property("unreadMsg").toInt();
    unread++;
    w->setProperty("unreadMsg", unread);

    int i = findTabWidgetIndex(w);
    if ( i >=0 )
    {
        ui->chatTabWidget->setTabText(i, generateTabName(w));
        setTabUnreadInfo(i, w);
    }
}

void ChatWidget::valuableMessageActive(QWidget *w)
{
    FCT_IDENTIFICATION;

    if ( w == ui->chatTabWidget->currentWidget() )
        return;

    int valuableMsgCounter = w->property("valuableMsg").toInt();
    valuableMsgCounter++;
    w->setProperty("valuableMsg", valuableMsgCounter);

    int i = findTabWidgetIndex(w);
    if ( i >=0 )
    {
        ui->chatTabWidget->setTabText(i, generateTabName(w));
        setTabUnreadInfo(i, w);
    }
}

void ChatWidget::chatTabClicked(int tabIndex)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << tabIndex;

    QWidget *w = ui->chatTabWidget->widget(tabIndex);
    w->setProperty("unreadMsg", 0);
    w->setProperty("valuableMsg", 0);
    ui->chatTabWidget->setTabText(tabIndex, generateTabName(w));
    setTabUnreadInfo(tabIndex, w);
    KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(w);
    if ( kstWidget )
        emit userListUpdated(kstWidget->getUserList());
    else
        emit userListUpdated(QList<KSTUsersInfo>());
}

void ChatWidget::processQSOInfo(const QString &callsign, const QString &grid)
{
    FCT_IDENTIFICATION;
    emit prepareQSOInfo(callsign, grid);
}

void ChatWidget::userListUpdate(QWidget *w)
{
    FCT_IDENTIFICATION;

    if ( w != ui->chatTabWidget->currentWidget() )
        return;

    KSTChatWidget *kstWidget = qobject_cast<KSTChatWidget*>(w);
    if ( kstWidget )
        emit userListUpdated(kstWidget->getUserList());
}

void ChatWidget::beamRequest(double az)
{
    FCT_IDENTIFICATION;

    emit beamingRequested(az);
}

int ChatWidget::findTabWidgetIndex(QWidget *w)
{
    FCT_IDENTIFICATION;

    for (int i = 0 ; i < ui->chatTabWidget->count(); i++ )
    {
        if ( ui->chatTabWidget->widget(i) == w )
            return i;
    }

    return -1;
}

QString ChatWidget::generateTabName(QWidget *w)
{
    FCT_IDENTIFICATION;

    return w->property("chatName").toString();
}

QString ChatWidget::generateTabUnread(QWidget *w)
{
    FCT_IDENTIFICATION;

    int unread = w->property("unreadMsg").toInt();
    int valuableMsgCnt = w->property("valuableMsg").toInt();

    return (( unread > 0 ) ? QString::number(valuableMsgCnt) + "/" + QString::number(unread)
                           : "");
}

void ChatWidget::setTabUnreadInfo(int tabIndex, QWidget *w)
{
    FCT_IDENTIFICATION;

    if ( !w )
        return;

    QWidget *tabButton = ui->chatTabWidget->tabBar()->tabButton(tabIndex, QTabBar::LeftSide);
    int unread = w->property("unreadMsg").toInt();
    int valuableMsgCnt = w->property("valuableMsg").toInt();

    if ( tabButton )
    {
        tabButton->deleteLater();
        ui->chatTabWidget->tabBar()->setTabButton(tabIndex, QTabBar::LeftSide, nullptr);
        tabButton = nullptr;
    }

    if ( unread == 0 && valuableMsgCnt == 0 )
    {
        return;
    }
    else
    {
        // if I reuse the label then I have an issue with a geometry. Therefore I remove it and
        // create it again.
        tabButton = new QLabel(generateTabUnread(w));
        tabButton->setObjectName((valuableMsgCnt > 0) ? "chatValuableMsg" : "chatUnread"); //based on this name, stylesheed is set from stylesheet.css
        ui->chatTabWidget->tabBar()->setTabButton(tabIndex, QTabBar::LeftSide, tabButton);
    }
}
