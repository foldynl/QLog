#include "Callsign.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.callsign");

Callsign::Callsign(const QString &callsign,
                   QObject *parent)
    : QObject{parent},
      fullCallsign(callsign.toUpper()),
      valid(false)
{
    FCT_IDENTIFICATION;

    QRegularExpression callsignRE = callsignRegEx();
    QRegularExpressionMatch match = callsignRE.match(fullCallsign);

    if ( match.hasMatch() )
    {
        //it is a valid callsign
        valid = true;
        hostPrefixWithDelimiter = match.captured(1);
        hostPrefix              = match.captured(2);
        base                    = match.captured(3);
        basePrefix              = match.captured(4);
        basePrefixNumber        = match.captured(5);
        suffixWithDelimiter     = match.captured(7);
        suffix                  = match.captured(8);

        qCDebug(runtime) << hostPrefix << base << suffix;
    }
    else
    {
        //it is an invalid callsign
        fullCallsign = QString();
    }
}

const QRegularExpression Callsign::callsignRegEx()
{
    FCT_IDENTIFICATION;
    return QRegularExpression(callsignRegExString(), QRegularExpression::CaseInsensitiveOption);
}

const QString Callsign::callsignRegExString()
{
    FCT_IDENTIFICATION;
    return QString("^(([A-Z0-9]+)[\\/])?(([A-Z][0-9]|[A-Z]{1,2}|[0-9][A-Z])([0-9]|[0-9]+)([A-Z]+))([\\/]([A-Z0-9]+))?");
}

const QString Callsign::getCallsign() const
{
    FCT_IDENTIFICATION;

    return fullCallsign;
}

const QString Callsign::getHostPrefix() const
{
    FCT_IDENTIFICATION;

    return hostPrefix;
}

const QString Callsign::getHostPrefixWithDelimiter() const
{
    FCT_IDENTIFICATION;

    return hostPrefixWithDelimiter;
}

const QString Callsign::getBase() const
{
    FCT_IDENTIFICATION;

    return base;
}

const QString Callsign::getBasePrefix() const
{
    FCT_IDENTIFICATION;

    return basePrefix;
}

const QString Callsign::getBasePrefixNumber() const
{
    FCT_IDENTIFICATION;

    return basePrefixNumber;
}

const QString Callsign::getSuffix() const
{
    FCT_IDENTIFICATION;

    return suffix;
}

const QString Callsign::getSuffixWithDelimiter() const
{
    FCT_IDENTIFICATION;

    return suffixWithDelimiter;
}

const QString Callsign::getWPXPrefix() const
{
    FCT_IDENTIFICATION;

    if ( !isValid() )
        return QString();

    // defined here
    // https://www.cqwpx.com/rules.htm

    // inspired here
    // https://git.fkurz.net/dj1yfk/yfklog/src/branch/develop/yfksubs.pl#L605


    /*********************
     * ONLY BASE CALLSIGN
     *********************/
    if ( getBase() != QString()
         && getHostPrefix() == QString()
         && getSuffix() == QString() )
    {
        // only callsign
        // return callsign prefix + prefix number
        // OL80ABC -> OL80
        // OK1ABC -> OK1
        return getBasePrefix() + getBasePrefixNumber();
    }

    /*********************
     * HOST PREFIX PRESENT
     *********************/
    if ( getHostPrefix() != QString() )
    {
        // callsign has a Host prefix SP/OK1XXX
        // we do not look at the suffix and assign automatically HostPrefix + '0'

        if ( getHostPrefix().back().isDigit() )
            return getHostPrefix();

        return getHostPrefix() + QString("0");
    }

    /****************
     * SUFFIX PRESENT
     ****************/
    if ( getSuffix().length() == 1) // some countries add single numbers as suffix to designate a call area, e.g. /4
    {
        bool isNumber = false;
        (void)suffix.toInt(&isNumber);
        if ( isNumber )
        {
            // callsign suffix is a number
            // VE7ABC/2 -> VE2
            return getBasePrefix() + getSuffix();
        }

        // callsign suffix is not a number
        // OK1ABC/P -> OK1
        return getBasePrefix() + getBasePrefixNumber();
    }

    /***************************
     * SUFFIX PRESENT LENGTH > 1
     ***************************/
    if ( secondarySpecialSuffixes.contains(getSuffix()) )
    {
        // QRP, MM etc.
        // OK1ABC/AM -> OK1
        return getBasePrefix() + getBasePrefixNumber();
    }

    // valid prefix should contain a number in the last position - check it
    // and prefix is not just a number
    // N8ABC/KH9 -> KH9

    bool isNumber = false;
    (void)getSuffix().toInt(&isNumber);

    if ( isNumber )
    {
        // suffix contains 2 and more numbers - ignore it
        return getBasePrefix() + getBasePrefixNumber();
    }

    // suffix is combination letters and digits and last position is a number
    if ( getSuffix().back().isDigit() )
        return getSuffix();

    // prefix does not contain a number - add "0"
    return getSuffix() + QString("0");
}

bool Callsign::isValid() const
{
    FCT_IDENTIFICATION;

    return valid;
}

// Based on wiki information
// https://en.wikipedia.org/wiki/Amateur_radio_call_signs
const QStringList Callsign::secondarySpecialSuffixes =
{
    "A",   // operator at a secondary location registered with the licensing authorities
    "AM",  // aeronautical mobile
    "M",   // mobile operation
    "MM",  // marine mobile
    "P",   // portable operation
    "QRP",  // QRP - unofficial
    "R",    // repeaters
    "B",    // beacon
    "LGT"   // 'LIGHTHOUSE' or 'LIGHTSHIP'  - unofficial
};
