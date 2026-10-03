#include "DxCommandEdit.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QEvent>
#include <QFocusEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QStyle>
#include <algorithm>
#include <climits>
#include "core/LogParam.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.dxcommandedit");

DxCommandEdit::DxCommandEdit(QWidget *parent) :
    QLineEdit(parent),
    commandCompleter(new QCompleter(this)),
    commandModel(new QStandardItemModel(this))
{
    FCT_IDENTIFICATION;

    QAction *commandListAction = addAction(style()->standardIcon(QStyle::SP_ArrowDown),
                                           QLineEdit::TrailingPosition);
    commandListAction->setText(tr("Show DX Cluster Commands"));
    commandListAction->setToolTip(tr("Show DX Cluster Commands"));
    connect(commandListAction, &QAction::triggered, this, &DxCommandEdit::showCommands);

    // Associate the popup without QLineEdit's automatic insertion on highlighting.
    commandCompleter->setWidget(this);
    commandCompleter->setModel(commandModel);
    commandCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    commandCompleter->setMaxVisibleItems(10);
    commandCompleter->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    commandCompleter->popup()->installEventFilter(this);

    connect(commandCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, &DxCommandEdit::insertCommand);
    connect(this, &QLineEdit::textEdited, this, [this]()
    {
        updatePopup();
    });
    connect(this, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        if ( text.isEmpty() )
            commandCompleter->popup()->hide();
    });

    setClusterType(UNKNOWN);
}

QString DxCommandEdit::commandName(const QString &text)
{
    FCT_IDENTIFICATION;

    QString name = text.simplified().section(QLatin1Char(' '), 0, 0).toLower();
    // Talk mode prefixes commands with / or //; sensitive-command checks still apply.
    while ( name.startsWith(QLatin1Char('/')) )
        name.remove(0, 1);
    if ( name.startsWith(QLatin1String("show/")) )
        name.replace(0, 5, QStringLiteral("sh/"));
    for ( const QString &base : {QStringLiteral("sh/dx"), QStringLiteral("sh/mydx"),
                                QStringLiteral("sh/wwv"), QStringLiteral("sh/wcy")} )
        if ( name.startsWith(base + QLatin1Char('/')) )
            return base;
    return name;
}

void DxCommandEdit::setServer(const QString &newServer)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << newServer;

    commandCompleter->popup()->hide();
    server = newServer;
    history.clear();
    historyEnabled = true;
    if ( server.isEmpty() )
        return;

    const QJsonArray entries = QJsonDocument::fromJson(LogParam::getDXCCommandHistory(server)).array();
    for ( const QJsonValue &value : entries )
    {
        const QJsonObject entry = value.toObject();
        const QString command = entry.value(QStringLiteral("command")).toString();
        if ( command.trimmed().isEmpty() )
            continue;

        const auto duplicate = std::find_if(history.cbegin(), history.cend(), [&command](const HistoryEntry &item)
        {
            return item.command == command;
        });
        if ( duplicate == history.cend() )
            history.append({command, qMax(1, entry.value(QStringLiteral("count")).toInt(1))});
        if ( history.size() == MAX_HISTORY )
            break;
    }
}

void DxCommandEdit::setClusterType(ClusterType type)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << type;

    commands = {
        {QStringLiteral("help"), tr("Show cluster command help"), QString(), QString(), false},
        {QStringLiteral("sh/wwv"), tr("Show solar conditions (WWV)"), QString(), QString(), false},
        {QStringLiteral("sh/wcy"), tr("Show propagation reports (WCY)"), QString(), QString(), false}
    };

    if ( type == DXSPIDER )
    {
        // DXSpider options must not be offered to other cluster dialects.
        commands[0].parameters = tr("[command]\nOptional: the command to explain.");
        commands[0].example = QStringLiteral("help sh/dx");
        commands[1].parameters = tr("[count]\nOptional: the number of reports.");
        commands[1].example = QStringLiteral("sh/wwv 5");
        commands[2].parameters = tr("[count]\nOptional: the number of reports.");
        commands[2].example = QStringLiteral("sh/wcy 5");
        commands.append({QStringLiteral("sh/dx"), tr("Show recent DX spots"),
                         tr("[count] [callsign/prefix] [on band] [by callsign] [info text] [day days]\nAll optional: limit the count, station, band, spotter, comment or age."),
                         QStringLiteral("sh/dx 20 on 20m"), false});
        commands.append({QStringLiteral("sh/dxcc"), tr("Show DX spots for a DXCC entity"),
                         tr("<prefix>\nRequired: a prefix or callsign identifying the DXCC entity."),
                         QStringLiteral("sh/dxcc OK"), false});
        commands.append({QStringLiteral("sh/dxqsl"), tr("Show QSL information from DX spots"),
                         tr("<callsign>\nRequired: the callsign to look up."),
                         QStringLiteral("sh/dxqsl OK1ABC"), false});
        commands.append({QStringLiteral("sh/qra"), tr("Show bearing and distance to a locator"),
                         tr("<locator> [second locator]\nRequired: one locator. With two locators, the first is the starting point."),
                         QStringLiteral("sh/qra JO70AA"), false});
        commands.append({QStringLiteral("set/qra"), tr("Set your station locator"),
                         tr("<locator>\nRequired: your Maidenhead locator."),
                         QStringLiteral("set/qra JO70AA"), false});
        commands.append({QStringLiteral("sh/filter"), tr("Show your cluster filters"), QString(), QString(), false});
        commands.append({QStringLiteral("sh/hfstats"), tr("Show HF activity statistics"),
                         tr("[days] [date]\nOptional: the number of days and the starting date."),
                         QStringLiteral("sh/hfstats 7"), false});
        commands.append({QStringLiteral("sh/vhfstats"), tr("Show VHF activity statistics"),
                         tr("[days] [date]\nOptional: the number of days and the starting date."),
                         QStringLiteral("sh/vhfstats 7"), false});
        commands.append({QStringLiteral("sh/mydx"), tr("Show recent DX spots passing your filters"),
                         tr("[count] [callsign/prefix] [on band]\nOptional: the same options as sh/dx, with your spot filters applied."),
                         QStringLiteral("sh/mydx 20 on 20m"), false});
        commands.append({QStringLiteral("sh/prefix"), tr("Look up a DXCC entity and its prefixes"),
                         tr("<callsign/prefix>\nRequired: a callsign or prefix."),
                         QStringLiteral("sh/prefix OK"), false});
        commands.append({QStringLiteral("sh/station"), tr("Show station details and connection status"),
                         tr("[callsign ...]\nOptional: callsigns to look up; without them, show your own station."),
                         QStringLiteral("sh/station OK1ABC"), false});
        commands.append({QStringLiteral("sh/configuration"), tr("Show cluster nodes and connected users"),
                         tr("[node/prefix/all]\nOptional: a node, prefix or all. The full list can be very long."),
                         QStringLiteral("sh/configuration OK"), false});
        commands.append({QStringLiteral("sh/bands"), tr("Show cluster band and frequency definitions"), QString(), QString(), false});
        commands.append({QStringLiteral("sh/time"), tr("Show UTC and local standard time"),
                         tr("[callsign/prefix ...]\nOptional: destinations; without them, show the node's time."),
                         QStringLiteral("sh/time JA"), false});
        commands.append({QStringLiteral("sh/sun"), tr("Show sunrise, sunset and the Sun's position"),
                         tr("[day offset] [callsign/prefix ...]\nOptional: days before or after today and destinations; defaults to your station."),
                         QStringLiteral("sh/sun +1 JA"), false});
        commands.append({QStringLiteral("sh/moon"), tr("Show moonrise, moonset and the Moon's position"),
                         tr("[day offset] [callsign/prefix ...]\nOptional: days before or after today and destinations; defaults to your station."),
                         QStringLiteral("sh/moon +1 JA"), false});
        commands.append({QStringLiteral("sh/muf"), tr("Estimate HF propagation to a destination"),
                         tr("<prefix> [hours] [long]\nRequired: destination prefix. Optional: up to 24 hours and long-path prediction."),
                         QStringLiteral("sh/muf JA 24"), false});
        commands.append({QStringLiteral("sh/hftable"), tr("Show HF spotter activity by DXCC entity"),
                         tr("[days] [date] [prefix ...]\nOptional: period, starting date and prefixes; defaults to your DXCC entity."),
                         QStringLiteral("sh/hftable 7 OK"), false});
        commands.append({QStringLiteral("sh/vhftable"), tr("Show VHF spotter activity by DXCC entity"),
                         tr("[days] [date] [prefix ...]\nOptional: period, starting date and prefixes; defaults to your DXCC entity."),
                         QStringLiteral("sh/vhftable 7 OK"), false});
        commands.append({QStringLiteral("apropos"), tr("Search cluster command help"),
                         tr("<text>\nRequired: a word to search for in the cluster help."),
                         QStringLiteral("apropos filter"), false});
        commands.append({QStringLiteral("dx"), tr("Send a DX spot"),
                         tr("<frequency> <callsign> [comment]\nRequired: frequency in kHz and spotted callsign. Optional: a comment."),
                         QStringLiteral("dx 14025 OK1ABC CW"), false});
        commands.append({QStringLiteral("talk"), tr("Send a private message to a connected station"),
                         tr("<callsign> [text]\nRequired: recipient. Include text for one message; without text, enter talk mode (leave with /ex)."),
                         QStringLiteral("talk OK1ABC Thanks for the QSO"), false});
        commands.append({QStringLiteral("announce"), tr("Send an announcement"),
                         tr("[full] <text>\nRequired: announcement text. Optional: full broadcasts beyond the local node."),
                         QStringLiteral("announce Thanks for the spots"), false});
        commands.append({QStringLiteral("chat"), tr("Send a message to a chat group"),
                         tr("<group> <text>\nRequired: group name and message text."),
                         QStringLiteral("chat FOC Hello"), false});
        commands.append({QStringLiteral("join"), tr("Join a chat group"),
                         tr("<group>\nRequired: the group name."), QStringLiteral("join FOC"), false});
        commands.append({QStringLiteral("leave"), tr("Leave a chat group"),
                         tr("<group>\nRequired: the group name."), QStringLiteral("leave FOC"), false});
        commands.append({QStringLiteral("sh/chat"), tr("Show recent chat messages"),
                         tr("[group] [lines]\nOptional: group name and number of messages."),
                         QStringLiteral("sh/chat FOC 10"), false});
        commands.append({QStringLiteral("accept/spots"), tr("Set a filter for spots to receive"),
                         tr("[0-9] <pattern>\nRequired: a filter such as on hf/cw, call OK or by_zone 14,15. Optional: filter line number."),
                         QStringLiteral("accept/spots 1 on hf/cw"), false});
        commands.append({QStringLiteral("reject/spots"), tr("Set a filter for spots to hide"),
                         tr("[0-9] <pattern>\nRequired: a filter such as on 6m, call OK or info FT8. Optional: filter line number."),
                         QStringLiteral("reject/spots 1 info FT8"), false});
        commands.append({QStringLiteral("clear/spots"), tr("Remove a spot filter"),
                         tr("[0-9|all]\nOptional: a filter line number (default 1), or all to remove every spot filter."),
                         QStringLiteral("clear/spots all"), false});
        commands.append({QStringLiteral("set/name"), tr("Set your operator name"),
                         tr("<name>\nRequired: your name."), QStringLiteral("set/name Jan"), false});
        commands.append({QStringLiteral("set/qth"), tr("Set your station location name"),
                         tr("<location>\nRequired: your town or station location name."),
                         QStringLiteral("set/qth Prague"), false});
        commands.append({QStringLiteral("set/location"), tr("Set your station latitude and longitude"),
                         tr("<latitude> <longitude>\nRequired: degrees, minutes and N/S or E/W for each coordinate."),
                         QStringLiteral("set/location 50 5 N 14 25 E"), false});
        commands.append({QStringLiteral("set/page"), tr("Set cluster output paging"),
                         tr("<lines>\nRequired: lines per page; 0 disables paging."),
                         QStringLiteral("set/page 0"), false});
        commands.append({QStringLiteral("set/dx"), tr("Enable live DX spots"), QString(), QString(), false});
        commands.append({QStringLiteral("unset/dx"), tr("Disable live DX spots"), QString(), QString(), false});
        commands.append({QStringLiteral("set/wwv"), tr("Enable live WWV reports"), QString(), QString(), false});
        commands.append({QStringLiteral("unset/wwv"), tr("Disable live WWV reports"), QString(), QString(), false});
        commands.append({QStringLiteral("set/wcy"), tr("Enable live WCY reports"), QString(), QString(), false});
        commands.append({QStringLiteral("unset/wcy"), tr("Disable live WCY reports"), QString(), QString(), false});
        commands.append({QStringLiteral("set/announce"), tr("Enable announcements"), QString(), QString(), false});
        commands.append({QStringLiteral("unset/announce"), tr("Disable announcements"), QString(), QString(), false});
        commands.append({QStringLiteral("set/talk"), tr("Enable private talk messages"), QString(), QString(), false});
        commands.append({QStringLiteral("unset/talk"), tr("Disable private talk messages"), QString(), QString(), false});
        commands.append({QStringLiteral("who"), tr("Show local connections"), QString(), QString(), false});
        commands.append({QStringLiteral("bye"), tr("Disconnect from the cluster"), QString(), QString(), false});
    }
    else if ( type == CC_CLUSTER )
    {
        commands[1].parameters = tr("[/count]\nOptional: append / and the number of reports to the command.");
        commands[1].example = QStringLiteral("sh/wwv/5");
        commands[1].parameterSuffix = true;
        commands[2].parameters = tr("[/count]\nOptional: append / and the number of reports to the command.");
        commands[2].example = QStringLiteral("sh/wcy/5");
        commands[2].parameterSuffix = true;
        commands.append({QStringLiteral("sh/dx"), tr("Show recent DX spots"),
                         tr("[callsign/band]\nOptional: a callsign or band in metres. For a spot count, use sh/dx/count."),
                         QStringLiteral("sh/dx/20"), false});
        commands.append({QStringLiteral("sh/mydx"), tr("Show recent DX spots passing your filters"),
                         tr("[callsign/band]\nOptional: a callsign or band in metres. For a spot count, use sh/mydx/count."),
                         QStringLiteral("sh/mydx/20"), false});
        commands.append({QStringLiteral("sh/head"), tr("Show bearing and distance"),
                         tr("<callsign>\nRequired: the destination callsign."),
                         QStringLiteral("sh/head OK1ABC"), false});
        commands.append({QStringLiteral("set/qra"), tr("Set your station locator"),
                         tr("<locator>\nRequired: your Maidenhead locator."),
                         QStringLiteral("set/qra JO70AA"), false});
        commands.append({QStringLiteral("sh/filter"), tr("Show your cluster filters"), QString(), QString(), false});
    }

    if ( commandCompleter->popup()->isVisible() )
        updatePopup();
}

void DxCommandEdit::rememberCommand(const QString &text)
{
    FCT_IDENTIFICATION;

    const QString command = text.trimmed();
    const QString name = commandName(command);
    if ( !historyEnabled || server.isEmpty() || command.isEmpty() )
        return;
    const QString verb = name.section(QLatin1Char('/'), 0, 0);
    const QString option = name.section(QLatin1Char('/'), 1, 1);
    // Password changes can prompt for arbitrary text; stop recording until reconnecting.
    if ( (!verb.isEmpty() && QStringLiteral("set").startsWith(verb)
          && !option.isEmpty() && QStringLiteral("password").startsWith(option))
         || name.startsWith(QLatin1String("set/pw"))
         || name == QLatin1String("password") || name == QLatin1String("passwd") )
    {
        historyEnabled = false;
        return;
    }

    HistoryEntry entry = {command, 0};
    for ( int index = 0; index < history.size(); ++index )
    {
        if ( history.at(index).command == command )
        {
            entry = history.takeAt(index);
            break;
        }
    }
    if ( entry.count < INT_MAX )
        ++entry.count;
    history.prepend(entry);
    if ( history.size() > MAX_HISTORY )
        history.removeLast();

    QJsonArray entries;
    for ( const HistoryEntry &item : history )
    {
        QJsonObject value;
        value.insert(QStringLiteral("command"), item.command);
        value.insert(QStringLiteral("count"), item.count);
        entries.append(value);
    }
    if ( !LogParam::setDXCCommandHistory(server, QJsonDocument(entries).toJson(QJsonDocument::Compact)) )
        qCWarning(runtime) << "Cannot save DX cluster command history";
}

void DxCommandEdit::addHeading(const QString &text)
{
    FCT_IDENTIFICATION;

    QStandardItem *item = new QStandardItem(text);
    item->setFlags(Qt::NoItemFlags);
    QFont headingFont = font();
    headingFont.setBold(true);
    item->setFont(headingFont);
    commandModel->appendRow(item);
}

void DxCommandEdit::updatePopup(bool showAll)
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() || !hasFocus() || (!showAll && text().trimmed().size() < 2) )
    {
        commandCompleter->popup()->hide();
        return;
    }

    commandModel->clear();
    const QString query = showAll ? QString() : text().trimmed();
    const QString name = commandName(query);
    const auto currentCommand = std::find_if(commands.cbegin(), commands.cend(), [&name](const Command &command)
    {
        return command.name == name;
    });

    if ( currentCommand != commands.cend() )
    {
        QString details = tr("%1\nSyntax: %2").arg(currentCommand->description, currentCommand->name);
        if ( !currentCommand->parameters.isEmpty() )
        {
            const QString syntax = currentCommand->name
                                   + (currentCommand->parameterSuffix ? QString() : QStringLiteral(" "))
                                   + currentCommand->parameters;
            details = tr("%1\nSyntax: %2\nExample: %3")
                      .arg(currentCommand->description, syntax, currentCommand->example);
        }
        // QCompleter sizes the popup using the first row; keep help rows single-line and above history.
        const QStringList lines = details.split(QLatin1Char('\n'));
        for ( const QString &line : lines )
        {
            QStandardItem *item = new QStandardItem(line);
            item->setFlags(Qt::NoItemFlags);
            commandModel->appendRow(item);
        }
    }

    QList<HistoryEntry> sorted = history;
    // History is stored newest first, so a stable sort preserves recency for ties.
    std::stable_sort(sorted.begin(), sorted.end(), [](const HistoryEntry &left, const HistoryEntry &right)
    {
        return left.count > right.count;
    });

    QStringList displayed;
    for ( const HistoryEntry &entry : sorted )
    {
        const QString entryName = commandName(entry.command);
        const auto definition = std::find_if(commands.cbegin(), commands.cend(), [&entryName](const Command &command)
        {
            return command.name == entryName;
        });
        if ( !entry.command.contains(query, Qt::CaseInsensitive)
             && (definition == commands.cend() || !definition->description.contains(query, Qt::CaseInsensitive)) )
            continue;
        if ( displayed.isEmpty() )
            addHeading(tr("My commands"));
        QStandardItem *item = new QStandardItem(entry.command);
        item->setData(entry.command, Qt::UserRole);
        item->setEditable(false);
        commandModel->appendRow(item);
        displayed.append(entry.command);
        if ( displayed.size() == MAX_MY_COMMANDS )
            break;
    }

    if ( currentCommand == commands.cend() )
    {
        bool headingAdded = false;
        for ( const Command &command : commands )
        {
            if ( !command.name.contains(name, Qt::CaseInsensitive)
                 && !command.description.contains(query, Qt::CaseInsensitive) )
                continue;
            if ( displayed.contains(command.name, Qt::CaseInsensitive) )
                continue;
            if ( !headingAdded )
            {
                addHeading(tr("Common commands"));
                headingAdded = true;
            }
            QStandardItem *item = new QStandardItem(tr("%1 — %2").arg(command.name, command.description));
            item->setData(command.name + (command.parameters.isEmpty() || command.parameterSuffix
                                         ? QString() : QStringLiteral(" ")),
                          Qt::UserRole);
            item->setEditable(false);
            commandModel->appendRow(item);
        }
    }

    if ( commandModel->rowCount() == 0 )
    {
        commandCompleter->popup()->hide();
        return;
    }

    QRect popupRect = rect();
    popupRect.setWidth(qMax(width(), commandCompleter->popup()->sizeHintForColumn(0)
                                  + style()->pixelMetric(QStyle::PM_ScrollBarExtent)));
    commandCompleter->complete(popupRect);
    // Enter must keep sending the typed command until the operator explicitly selects a row.
    commandCompleter->popup()->setCurrentIndex(QModelIndex());
}

void DxCommandEdit::insertCommand(const QModelIndex &index)
{
    FCT_IDENTIFICATION;

    const QString command = index.data(Qt::UserRole).toString();
    if ( command.isEmpty() )
        return;
    commandCompleter->popup()->hide();
    setText(command);
    setFocus();
    updatePopup();
}

void DxCommandEdit::showCommands()
{
    FCT_IDENTIFICATION;

    setFocus();
    updatePopup(true);
}

bool DxCommandEdit::event(QEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->type() == QEvent::Hide || (event->type() == QEvent::EnabledChange && !isEnabled()) )
        commandCompleter->popup()->hide();
    if ( event->type() == QEvent::ShortcutOverride && commandCompleter->popup()->isVisible() )
    {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if ( key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter
             || key == Qt::Key_Up || key == Qt::Key_Down )
        {
            event->accept();
            return true;
        }
    }
    return QLineEdit::event(event);
}

bool DxCommandEdit::eventFilter(QObject *object, QEvent *event)
{
    FCT_IDENTIFICATION;

    if ( object == commandCompleter->popup() && event->type() == QEvent::KeyPress )
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();
        if ( key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter
             || key == Qt::Key_Up || key == Qt::Key_Down )
        {
            keyPressEvent(keyEvent);
            return true;
        }
    }
    return QLineEdit::eventFilter(object, event);
}

void DxCommandEdit::keyPressEvent(QKeyEvent *event)
{
    FCT_IDENTIFICATION;

    QAbstractItemView *popup = commandCompleter->popup();
    const int key = event->key();
    if ( key == Qt::Key_Up || key == Qt::Key_Down )
    {
        if ( !popup->isVisible() )
            showCommands();
        const int step = key == Qt::Key_Down ? 1 : -1;
        int row = popup->currentIndex().isValid() ? popup->currentIndex().row()
                                                : (step > 0 ? -1 : commandModel->rowCount());
        while ( (row += step) >= 0 && row < commandModel->rowCount() )
        {
            const QModelIndex index = commandCompleter->completionModel()->index(row, 0);
            if ( index.flags() & Qt::ItemIsSelectable )
            {
                popup->setCurrentIndex(index);
                popup->scrollTo(index);
                break;
            }
        }
        event->accept();
        return;
    }
    if ( popup->isVisible() )
    {
        if ( key == Qt::Key_Escape )
        {
            popup->hide();
            event->accept();
            return;
        }
        if ( key == Qt::Key_Return || key == Qt::Key_Enter )
        {
            const QModelIndex index = popup->currentIndex();
            popup->hide();
            if ( index.flags() & Qt::ItemIsSelectable )
            {
                insertCommand(index);
                event->accept();
                return;
            }
        }
    }
    QLineEdit::keyPressEvent(event);
}

void DxCommandEdit::focusOutEvent(QFocusEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->reason() != Qt::PopupFocusReason )
        commandCompleter->popup()->hide();
    QLineEdit::focusOutEvent(event);
}
