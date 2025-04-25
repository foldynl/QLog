#include <QSettings>
#include "GenericCallbook.h"

const QString GenericCallbook::SECURE_STORAGE_KEY = "Callbook";
const QString GenericCallbook::CONFIG_USERNAME_KEY = "genericcallbook/username";
const QString GenericCallbook::CONFIG_PRIMARY_CALLBOOK_KEY = "callbook/primary";
const QString GenericCallbook::CONFIG_SECONDARY_CALLBOOK_KEY = "callbook/secondary";
const QString GenericCallbook::CONFIG_WEB_LOOKUP_URL = "callbook/weblookupurl";
const QString GenericCallbook::CALLBOOK_NAME = "none";

GenericCallbook::GenericCallbook(QObject *parent) :
    QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &GenericCallbook::onNetworkReply);
}

const QString GenericCallbook::getWebLookupURL(const QString &callsign,
                                               const QString &URL,
                                               const bool replaceMacro )
{
    QSettings setting;
    QString url;

    if ( URL.isEmpty() )
    {
        url = setting.value(GenericCallbook::CONFIG_WEB_LOOKUP_URL, "https://www.qrz.com/lookup/<DXCALL>").toString();
    }
    else
    {
        url = URL;
    }

    if ( replaceMacro )
        url.replace("<DXCALL>", callsign);
    return url;
}

void GenericCallbook::onNetworkReply(QNetworkReply *reply)
{
    processReply(reply);
}


