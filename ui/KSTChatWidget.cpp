#include <QMessageBox>
#include <QScrollBar>
#include <QCommonStyle>

#include "KSTChatWidget.h"
#include "ui_KSTChatWidget.h"

#include "core/debug.h"
#include "data/Data.h"
#include "data/StationProfile.h"
#include "ui/ColumnSettingDialog.h"
#include "ui/component/StyleItemDelegate.h"
#include "KSTHighlighterSettingDialog.h"
#include "rotator/Rotator.h"

MODULE_IDENTIFICATION("qlog.ui.kstchatwidget");

KSTChatWidget::KSTChatWidget(int chatRoomIndex,
                             const QString &username,
                             const QString &password,
                             const NewContactWidget *contact,
                             QWidget *parent) :
    QWidget(parent),
    ui(new Ui::KSTChatWidget),
    messageModel(new ChatMessageModel(this)),
    valuableMessageModel(new ChatMessageModel(this)),
    chat(new KSTChat(chatRoomIndex, username, password, contact, this)),
    userListModel(new UserListModel(this)),
    highlightEvaluator(new chatHighlightEvaluator(chatRoomIndex, this)),
    userName(username)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->splitterMsgMsg->setSizes(QList<int>({1, 0}));
    ui->splitterMsgUsr->setSizes(QList<int>({3, 1}));

    ui->toLabel->setVisible(false);
    ui->resetButton->setVisible(false);
    setBeamActionVisible(Rotator::instance()->isRotConnected());

    QCommonStyle style;
    ui->resetButton->setIcon(style.standardIcon(QStyle::SP_LineEditClearButton));

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(userListModel);
    proxyModel->setSortRole(Qt::UserRole);
    ui->usersTableView->setModel(proxyModel);
    ui->usersTableView->setSortingEnabled(true);
    ui->usersTableView->sortByColumn(0, Qt::AscendingOrder);
    ui->usersTableView->setItemDelegateForColumn(2, new DistanceFormatDelegate(1, 0.1, ui->usersTableView));
    ui->usersTableView->setItemDelegateForColumn(3, new UnitFormatDelegate("°", 0, 0.1, ui->usersTableView));
    ui->usersTableView->setItemDelegateForColumn(4, new HTMLDelegate(ui->usersTableView));
    ui->usersTableView->horizontalHeader()->setSectionsMovable(true);
    ui->usersTableView->addAction(ui->actionPrefillQSO);
    ui->usersTableView->addAction(ui->actionBeam);
    ui->usersTableView->addAction(ui->actionDisplayedColumns);

    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    ui->messageListView->setItemDelegate(new MessageDelegate(this));
    ui->messageListView->setModel(messageModel);
    ui->messageListView->addAction(ui->actionShowAboutMeOnly);
    ui->messageListView->addAction(ui->actionSuppressUser2User);
    ui->messageListView->addAction(ui->actionHighlight);
    ui->messageListView->addAction(separator);
    ui->messageListView->addAction(ui->actionHighlightRules);

    ui->valuableMessageListView->setItemDelegate(new MessageDelegate(this));
    ui->valuableMessageListView->setModel(valuableMessageModel);
    ui->valuableMessageListView->addAction(ui->actionClearValuableMessages);

    connect(chat.data(), &KSTChat::chatMsg,
            this, &KSTChatWidget::addChatMessage);

    connect(chat.data(), &KSTChat::chatError,
            this, &KSTChatWidget::showChatError);

    connect(chat.data(), &KSTChat::usersListUpdated,
            this, &KSTChatWidget::updateUserList);

    connect(chat.data(), &KSTChat::chatDisconnected,
            this, &KSTChatWidget::closeChat);

    connect(Rotator::instance(), &Rotator::rotConnected,
            this, [this]()
    {
        setBeamActionVisible(true);
    });

    connect(Rotator::instance(), &Rotator::rotDisconnected,
            this, [this]()
    {
        setBeamActionVisible(false);
    });

    chat->connectChat();
}

KSTChatWidget::~KSTChatWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}

QList<KSTUsersInfo> KSTChatWidget::getUserList()
{
    FCT_IDENTIFICATION;

    return (chat.data()) ? chat->getUsersList() : QList<KSTUsersInfo>();
}

void KSTChatWidget::addChatMessage(KSTChatMsg msg)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << msg.time << msg.sender << msg.message;

    if ( msg.message.isEmpty() )
        return;

    bool isMyCallsignPresent = msg.message.contains(userName,Qt::CaseInsensitive);
    bool isSenderSelectedCallsign = ui->toLabel->isVisible()
                                    && !msg.sender.isEmpty()
                                    && msg.sender.startsWith(ui->toLabel->text());
    bool isUser2User = msg.message.startsWith(" ("); //hack - how to easy to recognize Private Message
    bool shouldHighlight = ui->actionHighlight->isChecked() && isHighlightCandidate(msg);

    qCDebug(runtime) << "AboutMe" << isMyCallsignPresent;
    qCDebug(runtime) << "isUser2User" << isUser2User;
    qCDebug(runtime) << "shouldHighlight" << shouldHighlight;
    qCDebug(runtime) << "isSenderSelectedCallsign" << isSenderSelectedCallsign;

    // Filter incoming messages
    // Empty callsign means server response, do not suppress it
    if ( !msg.sender.isEmpty() )
    {
        if ( ! shouldHighlight && ! isSenderSelectedCallsign )
        {
            if ( ui->actionShowAboutMeOnly->isChecked() && !isMyCallsignPresent )
                return;

            if ( ui->actionSuppressUser2User->isChecked() && isUser2User)
                return;
        }
    }

    ChatMessageModel::MessageDirection dir = ChatMessageModel::INCOMING;

    if ( shouldHighlight )
    {
        dir = ChatMessageModel::INCOMING_HIGHLIGHT;
    }
    else if ( isMyCallsignPresent || isSenderSelectedCallsign )
    {
        dir = ChatMessageModel::INCOMING_TOYOU;

    }

    if ( shouldHighlight
         || isMyCallsignPresent
         || isSenderSelectedCallsign )
    {
        valuableMessageModel->addMessage(dir, msg);
        if ( ui->valuableMessageListView->verticalScrollBar()->value() == ui->valuableMessageListView->verticalScrollBar()->maximum() )
            ui->valuableMessageListView->scrollToBottom();
        emit valuableMessageUpdated(this);
    }

    messageModel->addMessage(dir, msg);

    if ( ui->messageListView->verticalScrollBar()->value() == ui->messageListView->verticalScrollBar()->maximum() )
        ui->messageListView->scrollToBottom();

    emit chatUpdated(this);
}

void KSTChatWidget::sendMessage()
{
    FCT_IDENTIFICATION;

    KSTChatMsg chatMsg;
    QString command;

    chatMsg.sender = tr("You");
    chatMsg.time = QDateTime::currentDateTimeUtc().toString("hhmm");

    if ( ui->toLabel->text() != QString() )
    {
        chatMsg.message = "(" + ui->toLabel->text() + ") " + ui->msgLineEdit->text();
        command = "/cq "
                  + ui->toLabel->text()
                  + " " + ui->msgLineEdit->text();
    }
    else
    {
        chatMsg.message = ui->msgLineEdit->text();
        command = ui->msgLineEdit->text();
    }

    messageModel->addMessage(ChatMessageModel::OUTGOING,
                             chatMsg);
    valuableMessageModel->addMessage(ChatMessageModel::OUTGOING,
                                  chatMsg);
    ui->messageListView->scrollToBottom();
    ui->valuableMessageListView->scrollToBottom();
    chat->sendMessage(command);
    ui->msgLineEdit->blockSignals(true);
    ui->msgLineEdit->clear();
    ui->msgLineEdit->blockSignals(false);
}

void KSTChatWidget::updateUserList()
{
    FCT_IDENTIFICATION;

    userListModel->clear();
    QList<KSTUsersInfo> usersList = chat->getUsersList();

    // refresh user List
    userListModel->updateList(usersList);

    setSelectedCallsignInUserList(ui->toLabel->text());

    emit userListUpdated(this);
}

void KSTChatWidget::setPrivateChatCallsign(QString callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( callsign.isEmpty() )
    {
        resetPressed();
        return;
    }

    ui->toLabel->setVisible(true);
    ui->resetButton->setVisible(true);
    ui->toLabel->setText(callsign);
    setSelectedCallsignInUserList(callsign);
}

void KSTChatWidget::reloadStationProfile()
{
    FCT_IDENTIFICATION;

    chat->reloadStationProfile();
}

void KSTChatWidget::setBeamActionVisible(bool flag)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << flag;

    ui->actionBeam->setVisible(flag);
}

void KSTChatWidget::resetDupe()
{
    FCT_IDENTIFICATION;

    chat->resetDupe();
}

void KSTChatWidget::recalculateDupe()
{
    FCT_IDENTIFICATION;

    chat->recalculateDupe();
}

void KSTChatWidget::recalculateDxccStatus()
{
    FCT_IDENTIFICATION;

    chat->recalculateDxccStatus();
}

void KSTChatWidget::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    chat->updateSpotsStatusWhenQSOAdded(record);
}

void KSTChatWidget::updateSpotsStatusWhenQSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    chat->updateSpotsStatusWhenQSODeleted(record);

}

void KSTChatWidget::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    chat->updateSpotsDxccStatusWhenQSODeleted(entities);
}

void KSTChatWidget::showChatError(const QString &error)
{
    FCT_IDENTIFICATION;

    QMessageBox::warning(nullptr, QMessageBox::tr("QLog Error"),
                         QMessageBox::tr("Chat Error: ") + " " + error);
}

void KSTChatWidget::closeChat()
{
    FCT_IDENTIFICATION;

    emit chatClosed();
}

void KSTChatWidget::displayedColumns()
{
    FCT_IDENTIFICATION;

    ColumnSettingSimpleDialog dialog(ui->usersTableView);
    dialog.exec();
}

void KSTChatWidget::userDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    const QModelIndex &sourceIindex = proxyModel->mapToSource(index);
    setPrivateChatCallsign(userListModel->getUserInfo(sourceIindex).callsign);
}

void KSTChatWidget::messageDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    const QStringList messageSenderElements = messageModel->getMessage(index).sender.split(" ");
    if ( messageSenderElements.size() > 0 )
        setPrivateChatCallsign(messageSenderElements.at(0));
}

void KSTChatWidget::prefillQSOAction()
{
    FCT_IDENTIFICATION;

    const QModelIndex &sourceIndex = proxyModel->mapToSource(ui->usersTableView->currentIndex());
    const KSTUsersInfo &info = userListModel->getUserInfo(sourceIndex);
    emit prepareQSOInfo(info.callsign, info.grid.getGrid());
}

void KSTChatWidget::highlightPressed()
{
    FCT_IDENTIFICATION;

}

bool KSTChatWidget::isHighlightCandidate(KSTChatMsg &msg)
{
    FCT_IDENTIFICATION;

    return highlightEvaluator->shouldHighlight(msg, msg.matchedHighlightRules);
}

void KSTChatWidget::editHighlightRules()
{
    FCT_IDENTIFICATION;

    KSTHighlighterSettingDialog dialog(this);
    dialog.exec();
    highlightEvaluator->loadRules();
}

void KSTChatWidget::resetPressed()
{
    FCT_IDENTIFICATION;

    ui->msgLineEdit->clear();
    ui->toLabel->clear();
    ui->usersTableView->clearSelection();
    ui->toLabel->setVisible(false);
    ui->resetButton->setVisible(false);
}

void KSTChatWidget::beamingRequest()
{
    FCT_IDENTIFICATION;

    const QModelIndex &sourceIndex = proxyModel->mapToSource(ui->usersTableView->currentIndex());
    const KSTUsersInfo &info = userListModel->getUserInfo(sourceIndex);


    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    if ( !profile.locator.isEmpty() )
    {
        Gridsquare myGrid(profile.locator);

        double bearing;
        if ( myGrid.bearingTo(info.grid, bearing) )
        {
            emit beamingRequested(bearing);
        }
    }
}

void KSTChatWidget::clearValuableMessages()
{
    FCT_IDENTIFICATION;

    valuableMessageModel->clear();
}

void KSTChatWidget::setSelectedCallsignInUserList(const QString &callsign)
{
    FCT_IDENTIFICATION;

    if ( callsign.isEmpty() )
        return;

    const QModelIndexList &nextMatches = proxyModel->match(proxyModel->index(0,0), Qt::DisplayRole, callsign, 1, Qt::MatchExactly);

    if ( nextMatches.size() >= 1 )
    {
        ui->usersTableView->setCurrentIndex(nextMatches.at(0));
    }
}

int ChatMessageModel::rowCount(const QModelIndex &) const
{
    return messages.size();
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const
{
    if ( role == Qt::UserRole )
    {
        return messages.at(index.row()).first;
    }
    if ( role == Qt::DisplayRole )
    {
        const KSTChatMsg &msg = messages.at(index.row()).second;
        QString text(msg.message);
        text.replace("\n", "<br/>");

        QString htmlText;
        if ( !text.isEmpty() )
        {
            // <font size=\"2\">%1<b style=\"color: magenta\"> %2</b></font><br/>%3
            htmlText = QString("<font size=\"2\">%1 %2 %3</b></font><br/>%4").arg(msg.time,
                                                                                  msg.sender.isEmpty() ? "Server" : msg.sender,
                                                                                  !msg.matchedHighlightRules.isEmpty() ? "(" + msg.matchedHighlightRules.join(", ") + ")" : "",
                                                                                  text);

        }
        return htmlText;
    }
    return QVariant();
}

void ChatMessageModel::addMessage(MessageDirection direction,
                                  const KSTChatMsg &msg)
{
    beginInsertRows(QModelIndex(), messages.size(), messages.size()+1);
    messages.append(QPair<int, KSTChatMsg>(direction, msg));
    endInsertRows();
}

void ChatMessageModel::clear()
{
    FCT_IDENTIFICATION;
    beginResetModel();
    messages.clear();
    endResetModel();
}

KSTChatMsg ChatMessageModel::getMessage(const QModelIndex &index) const
{
    FCT_IDENTIFICATION;
    return messages.at(index.row()).second;
}

void MessageDelegate::paint(QPainter *painter,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    //https://forum.qt.io/topic/91480/implementing-chat-type-listview-with-text-bubbles/6

    QTextDocument bodydoc;
    QTextOption textOption(bodydoc.defaultTextOption());
    QString bodytext(index.data(Qt::DisplayRole).toString());

    bool outgoing = (index.data(Qt::UserRole).toInt() == ChatMessageModel::MessageDirection::OUTGOING);

    qreal contentswidth = option.rect.width() * d_widthfraction
                          - d_horizontalmargin - d_pointerwidth
                          - d_leftpadding - d_rightpadding;

    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    bodydoc.setDefaultTextOption(textOption);
    bodydoc.setHtml(bodytext);
    bodydoc.setTextWidth(contentswidth);

    qreal bodyheight = bodydoc.size().height();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // uncomment to see the area provided to paint this item
    //painter->drawRect(option.rect);

    painter->translate(option.rect.left() + d_horizontalmargin,
                       option.rect.top() + ((index.row() == 0) ? d_verticalmargin : 0));

    // background color for chat bubble
    QColor bgcolor;

    switch ( index.data(Qt::UserRole).toInt() )
    {
    case ChatMessageModel::MessageDirection::OUTGOING:
        bgcolor =
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                  QColorConstants::LightGray;
#else
        QColor(Qt::lightGray);
#endif
        break;
    case ChatMessageModel::MessageDirection::INCOMING_TOYOU:
        bgcolor =
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                  QColorConstants::Green;
#else
                  QColor(Qt::green);
#endif
        break;
    case ChatMessageModel::MessageDirection::INCOMING_HIGHLIGHT:
        bgcolor =
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                  QColorConstants::Red;
#else
                  QColor(Qt::red);
#endif
        break;
    default:
        bgcolor =
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                  QColorConstants::Cyan;
#else
                  QColor(Qt::cyan);
#endif
    }

    // create chat bubble
    QPainterPath pointie;

    // left bottom
    pointie.moveTo(0, bodyheight + d_toppadding + d_bottompadding);

    // right bottom
    pointie.lineTo(0 + contentswidth + d_pointerwidth + d_leftpadding + d_rightpadding - d_radius,
                   bodyheight + d_toppadding + d_bottompadding);
    pointie.arcTo(0 + contentswidth + d_pointerwidth + d_leftpadding + d_rightpadding - 2 * d_radius,
                  bodyheight + d_toppadding + d_bottompadding - 2 * d_radius,
                  2 * d_radius, 2 * d_radius, 270, 90);

    // right top
    pointie.lineTo(0 + contentswidth + d_pointerwidth + d_leftpadding + d_rightpadding, 0 + d_radius);
    pointie.arcTo(0 + contentswidth + d_pointerwidth + d_leftpadding + d_rightpadding - 2 * d_radius, 0,
                  2 * d_radius, 2 * d_radius, 0, 90);

    // left top
    pointie.lineTo(0 + d_pointerwidth + d_radius, 0);
    pointie.arcTo(0 + d_pointerwidth, 0, 2 * d_radius, 2 * d_radius, 90, 90);

    // left bottom almost (here is the pointie)
    pointie.lineTo(0 + d_pointerwidth, bodyheight + d_toppadding + d_bottompadding - d_pointerheight);
    pointie.closeSubpath();

    // rotate bubble for outgoing messages
    if (!outgoing)
    {
        painter->translate(option.rect.width() - pointie.boundingRect().width() - d_horizontalmargin - d_pointerwidth, 0);
        painter->translate(pointie.boundingRect().center());
        painter->rotate(180);
        painter->translate(-pointie.boundingRect().center());
    }

    painter->setPen(QPen(bgcolor));
    painter->drawPath(pointie);
    painter->fillPath(pointie, QBrush(bgcolor));

    // rotate back or painter is going to paint the text rotated
    if (!outgoing)
    {
        painter->translate(pointie.boundingRect().center());
        painter->rotate(-180);
        painter->translate(-pointie.boundingRect().center());
    }

    QAbstractTextDocumentLayout::PaintContext ctx;
    //if (outgoing)
    ctx.palette.setColor(QPalette::Text, QColor("black"));
    //else
    //ctx.palette.setColor(QPalette::Text, QColor("white"));

    painter->translate((!outgoing ? 0 : d_pointerwidth) + d_leftpadding, 0);
    bodydoc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTextDocument bodydoc;
    QTextOption textOption(bodydoc.defaultTextOption());
    QString bodytext(index.data(Qt::DisplayRole).toString());

    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    bodydoc.setDefaultTextOption(textOption);
    bodydoc.setHtml(bodytext);

    // the width of the contents are the (a fraction of the window width) minus (margins + padding + width of the bubble's tail)
    qreal contentswidth = option.rect.width() * d_widthfraction
                          - d_horizontalmargin - d_pointerwidth
                          - d_leftpadding - d_rightpadding;

    // set this available width on the text document
    bodydoc.setTextWidth(contentswidth);

    QSize size(bodydoc.idealWidth() + d_horizontalmargin + d_pointerwidth + d_leftpadding + d_rightpadding,
               bodydoc.size().height() + d_bottompadding + d_toppadding + d_verticalmargin + 1);

    if (index.row() == 0) // have extra margin at top of first item
        size += QSize(0, d_verticalmargin);

    return size;
}

int UserListModel::rowCount(const QModelIndex &) const
{
    return userData.count();
}

int UserListModel::columnCount(const QModelIndex &) const
{
    return 5;
}

QVariant UserListModel::data(const QModelIndex &index, int role) const
{
    const KSTUsersInfo &userInfo = userData.at(index.row());

    if ( role == Qt::DisplayRole )
    {
        switch ( index.column() )
        {
        case 0:
            return userInfo.callsign;
            break;
        case 1:
            if ( userInfo.grid.isValid() )
                return userInfo.grid.getGrid();
            else
                return QString();
            break;
        case 2:
        case 3:
        {
            const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

            if ( !profile.locator.isEmpty() )
            {
                Gridsquare myGrid(profile.locator);

                if ( index.column() == 2 )
                {
                    double distance;
                    if (  myGrid.distanceTo(userInfo.grid, distance) )
                    {
                        return QString::number(distance, 'f', 1);
                    }
                }

                if ( index.column() == 3 )
                {
                    double bearing;
                    if ( myGrid.bearingTo(userInfo.grid, bearing) )
                    {
                        return QString::number(bearing, 'f', 0);
                    }
                }
            }
            return QString();
        }
            break;
        case 4:
            return userInfo.stationComment;
            break;
        default:
            return QVariant();
        }
    }
    else if ( index.column() == 0 && role == Qt::BackgroundRole)
    {
        return Data::statusToColor(userInfo.status, userInfo.dupeCount, QColor(Qt::transparent));
    }
    else if (index.column() == 0 && role == Qt::ToolTipRole)
    {
        return QCoreApplication::translate("DBStrings", userInfo.dxcc.country.toUtf8().constData()) + " [" + Data::statusToText(userInfo.status) + "]";
    }
    else if ( role == Qt::UserRole )
    {
        switch ( index.column() )
        {
        case 2:
            return data(index,Qt::DisplayRole).toFloat();
            break;
        case 3:
            return data(index,Qt::DisplayRole).toInt();
            break;
        default:
            return data(index, Qt::DisplayRole);
        }
    }

    return QVariant();
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ( role != Qt::DisplayRole || orientation != Qt::Horizontal )
        return QVariant();

    switch (section)
    {
    case 0: return tr("Callsign");
    case 1: return tr("Gridsquare");
    case 2: return tr("Distance");
    case 3: return tr("Azimuth");
    case 4: return tr("Comment");
    default: return QVariant();
    }
}

void UserListModel::updateList(const QList<KSTUsersInfo> &userList)
{
    beginResetModel();
    userData = userList;
    endResetModel();
}

void UserListModel::clear()
{
    beginResetModel();
    userData.clear();
    endResetModel();
}

KSTUsersInfo UserListModel::getUserInfo(const QModelIndex &index) const
{
    return userData.at(index.row());
}

void HTMLDelegate::paint(QPainter* painter, const QStyleOptionViewItem & inOption, const QModelIndex &index) const
{
    QStyleOptionViewItem option = inOption;
    initStyleOption(&option, index);

    QStyle *style = option.widget? option.widget->style() : QApplication::style();
    QTextDocument doc;

    doc.setHtml(option.text);
    option.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    // Highlighting text if item is selected
    if (option.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text,
                             option.palette.color(QPalette::Active, QPalette::HighlightedText));

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option);
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize HTMLDelegate::sizeHint ( const QStyleOptionViewItem & inOption, const QModelIndex & index ) const
{
    QStyleOptionViewItem option = inOption;
    initStyleOption(&option, index);

    QTextDocument doc;
    doc.setHtml(option.text);
    doc.setTextWidth(option.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}
