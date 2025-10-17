#include <QCache>
#include "CallbookManager.h"
#include "core/debug.h"
#include "service/hamqth/HamQTH.h"
#include "service/qrzcom/QRZ.h"
#include "data/Callsign.h"
#include "LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.callbookmanager");

CallbookManager::CallbookManager(QObject *parent)
    : QObject{parent},
    primaryCallbookAuthSuccess(false),
    secondaryCallbookAuthSuccess(false)
{
    FCT_IDENTIFICATION;

    initCallbooks();
}

void CallbookManager::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( queryCache.contains(callsign) )
    {
        emit callsignResult(CallbookResponseData(*queryCache.object(callsign)));
        return;
    }

    // create an empty object in cache
    // if there is the second query for the same call immediately after
    // the first query, then it returns a result of empty object
    queryCache.insert(callsign, new CallbookResponseData);

    if ( !primaryCallbook.isNull() )
    {
        currentQueryCallsign = callsign;
        primaryCallbook->queryCallsign(currentQueryCallsign);
    }
    else
    {
        queryCache.remove(callsign);
        emit callsignNotFound(callsign);
    }
}

bool CallbookManager::isActive()
{
    FCT_IDENTIFICATION;

    bool ret = primaryCallbookAuthSuccess || secondaryCallbookAuthSuccess;
    qCDebug(runtime) << ret;
    return ret;
}

GenericCallbook *CallbookManager::createCallbook(const QString &callbookID)
{
    FCT_IDENTIFICATION;

    GenericCallbook *ret = nullptr;

    if (callbookID == HamQTHCallbook::CALLBOOK_NAME )
    {
        ret = new HamQTHCallbook(this);
    }
    else if ( callbookID == QRZCallbook::CALLBOOK_NAME )
    {
        ret = new QRZCallbook(this);
    }

    if ( ret )
    {
        connect(ret, &GenericCallbook::callsignResult, this, &CallbookManager::processCallsignResult);
    }

    return ret;
}

void CallbookManager::initCallbooks()
{
    primaryCallbook.clear();
    secondaryCallbook.clear();
    primaryCallbookAuthSuccess = false;
    secondaryCallbookAuthSuccess = false;

    queryCache.clear();
    currentQueryCallsign = QString();

    primaryCallbook = createCallbook(LogParam::getPrimaryCallbook(GenericCallbook::CALLBOOK_NAME));
    secondaryCallbook = createCallbook(LogParam::getSecondaryCallbook(GenericCallbook::CALLBOOK_NAME));

    if ( !primaryCallbook.isNull() )
    {
        connect(primaryCallbook, &GenericCallbook::callsignNotFound, this, &CallbookManager::primaryCallbookCallsignNotFound);
        connect(primaryCallbook, &GenericCallbook::loginFailed, this, [this]()
        {
            primaryCallbookAuthSuccess = false;
            emit loginFailed(primaryCallbook->getDisplayName());
        });
        connect(primaryCallbook, &GenericCallbook::lookupError, this, [this](const QString &error)
        {
            emit lookupError(primaryCallbook->getDisplayName()
                             + " - " + error
                             + ((!secondaryCallbook.isNull()) ? tr("<p>The secondary callbook will be used</p>") : ""));
            primaryCallbookCallsignNotFound(currentQueryCallsign);
        });
        primaryCallbookAuthSuccess = true;
    }

    if ( !secondaryCallbook.isNull() )
    {
        connect(secondaryCallbook, &GenericCallbook::callsignNotFound, this, &CallbookManager::secondaryCallbookCallsignNotFound);
        connect(secondaryCallbook, &GenericCallbook::loginFailed, this, [this]()
        {
            secondaryCallbookAuthSuccess = false;
            emit loginFailed(secondaryCallbook->getDisplayName());
        });
        connect(secondaryCallbook, &GenericCallbook::lookupError, this, [this](const QString &error)
        {
            emit lookupError(secondaryCallbook->getDisplayName() + " - " + error);
            secondaryCallbookCallsignNotFound(currentQueryCallsign);
        });
        secondaryCallbookAuthSuccess = true;
    }
}

void CallbookManager::abortQuery()
{
    FCT_IDENTIFICATION;

    queryCache.remove(currentQueryCallsign);
    if ( ! primaryCallbook.isNull() ) primaryCallbook->abortQuery();
    if ( ! secondaryCallbook.isNull() ) secondaryCallbook->abortQuery();
}

void CallbookManager::primaryCallbookCallsignNotFound(const QString &notFoundCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << notFoundCallsign;

    queryCache.remove(notFoundCallsign);

    if ( notFoundCallsign != currentQueryCallsign ) return ;

    if ( secondaryCallbook.isNull() )
    {
        emit callsignNotFound(notFoundCallsign);
        return;
    }

    qCDebug(runtime) << "Callsign not found - primary - trying the secondary callbook";

    secondaryCallbook->queryCallsign(notFoundCallsign);
}

void CallbookManager::secondaryCallbookCallsignNotFound(const QString &notFoundCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << notFoundCallsign;

    queryCache.remove(notFoundCallsign);

    if ( notFoundCallsign != currentQueryCallsign ) return ;

    qCDebug(runtime) << "Callsign not found - secondary ";

    emit callsignNotFound(notFoundCallsign);
}

void CallbookManager::processCallsignResult(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    CallbookResponseData *cacheData = new CallbookResponseData;
    *cacheData = data;
    queryCache.insert(data.call, cacheData);

    // Callbook returned queried callsign
    if ( data.call == currentQueryCallsign )
    {
        emit callsignResult(data);
        return;
    }

    Callsign queryCall(currentQueryCallsign);

    if ( ! queryCall.isValid() )
    {
        qCDebug(runtime) << "Query callsign is not valid " << currentQueryCallsign;
        return;
    }

    // If not exists full match record for example for SP/OK1xxx in a callbook
    // then callbooks return a partial result (usually base callsign) OK1xxx. In this case, QLog
    // takes only selected fields from the callbook response.

    if ( queryCall.getBase() == data.call )
    {
        qCDebug(runtime) << "Partial match for result - forwarding limited set of information";

        CallbookResponseData newData;

        newData.call = currentQueryCallsign;
        newData.fname = data.fname;
        newData.lname = data.lname;
        newData.lic_year = data.lic_year;
        newData.qsl_via = data.qsl_via;
        newData.email = data.email;
        newData.born = data.born;
        newData.url = data.url;
        newData.name_fmt = data.name_fmt;
        newData.nick = data.nick;
        newData.image_url = data.image_url;

        CallbookResponseData *cdata = new CallbookResponseData;
        *cdata = newData;

        queryCache.insert(currentQueryCallsign, cdata);
        emit callsignResult(newData);
    }
}

QCache<QString, CallbookResponseData> CallbookManager::queryCache(100);
