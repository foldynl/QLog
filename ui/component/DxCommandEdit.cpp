#include "DxCommandEdit.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <climits>
#include "core/LogParam.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.dxcommandedit");

DxCommandEdit::DxCommandEdit(QWidget *parent) :
    SuggestionEdit(parent)
{
    FCT_IDENTIFICATION;

    setPopupToolTip(tr("Show DX Cluster Commands"));
    setGroupLabels(tr("Common commands"), tr("My commands"));
    setSuggestWhileTyping(true);
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

    hideSuggestions();
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

    refreshSuggestions();
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

void DxCommandEdit::populateSuggestions(const QString &query)
{
    FCT_IDENTIFICATION;

    QList<Item> predefined;
    QList<Item> user;
    QString details;
    const QString name = commandName(query);
    const auto currentCommand = std::find_if(commands.cbegin(), commands.cend(), [&name](const Command &command)
    {
        return command.name == name;
    });

    if ( currentCommand != commands.cend() )
    {
        details = tr("%1\nSyntax: %2").arg(currentCommand->description, currentCommand->name);
        if ( !currentCommand->parameters.isEmpty() )
        {
            const QString syntax = currentCommand->name
                                   + (currentCommand->parameterSuffix ? QString() : QStringLiteral(" "))
                                   + currentCommand->parameters;
            details = tr("%1\nSyntax: %2\nExample: %3")
                      .arg(currentCommand->description, syntax, currentCommand->example);
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
        user.append({entry.command, entry.command});
        displayed.append(entry.command);
        if ( displayed.size() == MAX_MY_COMMANDS )
            break;
    }

    if ( currentCommand == commands.cend() )
    {
        for ( const Command &command : commands )
        {
            if ( !command.name.contains(name, Qt::CaseInsensitive)
                 && !command.description.contains(query, Qt::CaseInsensitive) )
                continue;
            if ( displayed.contains(command.name, Qt::CaseInsensitive) )
                continue;
            predefined.append({tr("%1 — %2").arg(command.name, command.description),
                               command.name + (command.parameters.isEmpty() || command.parameterSuffix
                                               ? QString() : QStringLiteral(" "))});
        }
    }

    setPopupItems(predefined, user, details);
}
