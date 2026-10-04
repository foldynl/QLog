#include "MacroExpander.h"
#include <QRegularExpression>
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.macroexpander");

const QList<MacroExpander::Definition> &MacroExpander::definitions()
{
    FCT_IDENTIFICATION;

    static const QList<Definition> macros = {
        {"DXCALL", "callsign", UPPER_CASE, QT_TR_NOOP("DX Callsign")},
        {"NAME", "name_intl", UPPER_CASE, QT_TR_NOOP("DX Operator Name")},
        {"RST", "rst_sent", UPPER_CASE, QT_TR_NOOP("Sent Report")},
        {"RSTN", "rst_sent", REPORT_N, QT_TR_NOOP("Sent Report (9 → N)")},
        {"GREETING", "greeting", UPPER_CASE, QT_TR_NOOP("Greeting GM/GA/GE (current QSO only)")},
        {"QTH", "qth_intl", UNCHANGED, QT_TR_NOOP("DX QTH")},
        {"MYCALL", "station_callsign", UPPER_CASE, QT_TR_NOOP("My Callsign")},
        {"MYNAME", "my_name_intl", UPPER_CASE, QT_TR_NOOP("My Name")},
        {"MYQTH", "my_city_intl", UPPER_CASE, QT_TR_NOOP("My QTH")},
        {"MYLOCATOR", "my_gridsquare", UPPER_CASE, QT_TR_NOOP("My Gridsquare")},
        {"MYGRID", "my_gridsquare", UPPER_CASE, QT_TR_NOOP("My Gridsquare")},
        {"MYSIG", "my_sig_intl", UPPER_CASE, QT_TR_NOOP("My SIG")},
        {"MYSIGINFO", "my_sig_info_intl", UPPER_CASE, QT_TR_NOOP("My SIG Information")},
        {"MYIOTA", "my_iota", UPPER_CASE, QT_TR_NOOP("My IOTA")},
        {"MYSOTA", "my_sota_ref", UPPER_CASE, QT_TR_NOOP("My SOTA")},
        {"MYWWFT", "my_wwff_ref", UPPER_CASE, QT_TR_NOOP("My WWFF")},
        {"MYVUCC", "my_vucc_grids", UPPER_CASE, QT_TR_NOOP("My VUCC Gridsquares")},
        {"MYPWR", "tx_pwr", UPPER_CASE, QT_TR_NOOP("My Power in W")},
        {"EXCHSTR", "stx_string", UPPER_CASE, QT_TR_NOOP("Contest Exchange Message")},
        {"EXCHNR", "stx", SERIAL_NUMBER, QT_TR_NOOP("Contest Exchange Serial Number")},
        {"EXCHNRN", "stx", SERIAL_NUMBER_N, QT_TR_NOOP("Contest Exchange Serial Number (9 → N, 0 → T)")},
        {"DXLOCATOR", "gridsquare", UPPER_CASE, QT_TR_NOOP("DX Gridsquare")},
        {"DXGRID", "gridsquare", UPPER_CASE, QT_TR_NOOP("DX Gridsquare")},
        {"MODE", "mode", UPPER_CASE, QT_TR_NOOP("QSO Mode")},
        {"PROPMODE", "prop_mode", UPPER_CASE, QT_TR_NOOP("Propagation Mode (e.g. ES)")},
        {"PROP", "prop_mode", PROPAGATION, QT_TR_NOOP("Propagation Mode for a Spot (e.g. <ES>)")}
    };
    return macros;
}

MacroExpander::Result MacroExpander::expand(const QString &text, const Context &context)
{
    FCT_IDENTIFICATION;

    static const QRegularExpression macroRE(QStringLiteral("<([A-Z][A-Z0-9]*)>"));
    Result result;
    int position = 0;
    auto matches = macroRE.globalMatch(text);
    const QList<Definition> &macros = definitions();

    while ( matches.hasNext() )
    {
        const QRegularExpressionMatch match = matches.next();
        const QString name = match.captured(1);
        result.text += text.mid(position, match.capturedStart() - position);
        QString replacement = match.captured();

        for ( const Definition &macro : macros )
        {
            if ( name != macro.name )
                continue;

            replacement = context.value(QLatin1String(macro.field));

            if ( replacement.isEmpty() && !result.missingMacros.contains(match.captured()) )
                result.missingMacros.append(match.captured());

            switch ( macro.format )
            {
            case UPPER_CASE:
                replacement = replacement.toUpper();
                break;
            case REPORT_N:
                replacement.replace('9', 'N');
                break;
            case SERIAL_NUMBER:
                replacement = replacement.rightJustified(3, '0');
                break;
            case SERIAL_NUMBER_N:
                replacement = replacement.rightJustified(3, '0').replace('9', 'N').replace('0', 'T');
                break;
            case PROPAGATION:
                if ( !replacement.isEmpty() )
                    replacement = QStringLiteral("<%1>").arg(replacement.toUpper());
                break;
            case UNCHANGED:
                break;
            }
            break;
        }

        // Do not expand field contents again; literal spot markers and CW controls stay untouched.
        result.text += replacement;
        position = match.capturedEnd();
    }
    result.text += text.mid(position);
    return result;
}

MacroExpander::Result MacroExpander::expandSpotComment(const QString &text, const Context &context)
{
    FCT_IDENTIFICATION;

    Result result = expand(text, context);
    result.text = result.text.replace('\r', ' ').replace('\n', ' ').trimmed();
    return result;
}

MacroExpander::Context MacroExpander::fromQSO(const QSqlRecord &qso)
{
    FCT_IDENTIFICATION;

    Context context;

    for ( const Definition &macro : definitions() )
    {
        const QString field = QLatin1String(macro.field);
        if ( context.contains(field) )
            continue;

        QString value = qso.value(field).toString();

        if ( value.isEmpty() && field.endsWith(QLatin1String("_intl")) )
            value = qso.value(field.left(field.size() - 5)).toString();

        if ( field == QLatin1String("tx_pwr") && !value.isEmpty() )
        {
            const double power = qso.value(field).toDouble();
            value = QString::number(power, 'f', (power != 0.0 && power < 1.0) ? 1 : 0);
        }
        context.insert(field, value);
    }
    return context;
}

QString MacroExpander::helpText()
{
    FCT_IDENTIFICATION;

    QStringList lines;

    for ( const Definition &macro : definitions() )
        lines.append(QStringLiteral("<%1> = %2").arg(QLatin1String(macro.name), tr(macro.description)));

    return lines.join(QLatin1Char('\n'));
}
