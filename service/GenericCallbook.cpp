#include <QTextDocument>

#include "GenericCallbook.h"
#include "core/LogParam.h"

const QString GenericCallbook::SECURE_STORAGE_KEY = "Callbook";
const QString GenericCallbook::CALLBOOK_NAME = "none";

GenericCallbook::GenericCallbook(QObject *parent) :
    QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &GenericCallbook::onNetworkReply);
}

const QString GenericCallbook::getWebLookupURL(const QString &callsign,
                                               const QString &URL,
                                               bool replaceMacro )
{
    QString url = ( URL.isEmpty() ) ? LogParam::getCallbookWebLookupURL("https://www.qrz.com/lookup/<DXCALL>")
                                    : URL;

    if ( replaceMacro ) url.replace("<DXCALL>", callsign);
    return url;
}

QString GenericCallbook::decodeHtmlEntities(const QString &text)
{
    QTextDocument doc;
    doc.setHtml(text);
    return doc.toRawText().trimmed();
}

void GenericCallbook::onNetworkReply(QNetworkReply *reply)
{
    processReply(reply);
}


