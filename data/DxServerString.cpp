#include <QRegularExpression>
#include "DxServerString.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.dxserverstring");

DxServerString::DxServerString(const QString &connectString,
                               const QString &defaultUsername) :
    port(7300),
    valid(false)
{
    FCT_IDENTIFICATION;

    if ( !isValidServerString(connectString) )
        return;

    // serverSelect format is:
    //   [username@]hostname:port
    // username is not mandatory
    QStringList serverElements = connectString.split(":");

    username = defaultUsername;

    if ( serverElements[0].contains(QStringLiteral("@")) )
    {
        QStringList hostNameElements = serverElements[0].split(QStringLiteral("@"));
        username = hostNameElements[0];
        hostname = hostNameElements[1];
    }
    else
    {
        hostname = serverElements[0];
    }

    port = serverElements[1].toInt();  // servername is verified, therefore it is not needed to check
    // whether the variable "server" contains hostname and port
    valid = true;
}

const QRegularExpression DxServerString::serverStringRegEx()
{
    FCT_IDENTIFICATION;
    return QRegularExpression(QStringLiteral("^([a-z0-9\\-._~%!$&'()*+,;=]+@)?(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?):([0-9]{1,5})$|^([a-z0-9\\-._~%!$&'()*+,;=]+@)?(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\\-]*[A-Za-z0-9]):([0-9]{1,5})$"),
                              QRegularExpression::CaseInsensitiveOption);

}

bool DxServerString::isValidServerString(const QString &connectString)
{
    FCT_IDENTIFICATION;

    QRegularExpressionMatch stringMatch = serverStringRegEx().match(connectString);

    bool ret = stringMatch.hasMatch();
    qCDebug(runtime) << ret;
    return ret;
}

const QString DxServerString::getPasswordStorageKey() const
{
    FCT_IDENTIFICATION;

    if ( !isValid() )
        return QString();

    return getHostname() + ":" + QString::number(getPort());
}
