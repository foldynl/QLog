#include <QSqlError>
#include <QSqlQuery>

#include "data/DxccConfirmation.h"
#include "data/BandPlan.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.dxccconfirmation");

DxccConfirmationResult DxccConfirmation::status(int dxcc,
                                                 int myDxcc,
                                                 const QString &band,
                                                 const QString &mode,
                                                 AlertDxccContext context,
                                                 DxccConfirmationSources sources)
{
    FCT_IDENTIFICATION;

    QString cacheBand = QStringLiteral("*");
    QString cacheMode = QStringLiteral("*");

    switch ( context )
    {
        case AlertDxccContext::Anywhere:
            break;
        case AlertDxccContext::Band:
            if ( band.isEmpty() ) return DxccConfirmationResult::Error;
            cacheBand = band;
            break;
        case AlertDxccContext::Mode:
            cacheMode = normalizedMode(mode);
            if ( cacheMode.isEmpty() ) return DxccConfirmationResult::Error;
            break;
        case AlertDxccContext::BandMode:
            return DxccConfirmationResult::Error;
    }

    if ( dxcc <= 0 ) return DxccConfirmationResult::Error;

    if ( !cachedSourcesSet || sources != cachedSources )
    {
        cache.clear();
        cachedSources = sources;
        cachedSourcesSet = true;
    }

    if ( DxccConfirmationResult *cached = cache.value(dxcc, myDxcc, cacheBand, cacheMode) )
        return *cached;

    if ( sources == DxccConfirmationSource::NoConfirmationSource )
    {
        cache.insert(dxcc, myDxcc, cacheBand, cacheMode,
                     new DxccConfirmationResult(DxccConfirmationResult::NotConfirmed));
        return DxccConfirmationResult::NotConfirmed;
    }

    QString sql = QStringLiteral(
        "SELECT EXISTS("
        " SELECT 1"
        " FROM contacts");

    if ( context == AlertDxccContext::Mode )
        sql += QStringLiteral(" INNER JOIN modes ON modes.name = contacts.mode");

    sql += QStringLiteral(
        " WHERE contacts.dxcc = :dxcc"
        " AND UPPER(COALESCE(contacts.prop_mode, '')) <> 'SAT'");

    if ( myDxcc != 0 )
        sql += QStringLiteral(" AND contacts.my_dxcc = :myDxcc");

    if ( context == AlertDxccContext::Band )
        sql += QStringLiteral(" AND contacts.band = :band");
    else if ( context == AlertDxccContext::Mode )
        sql += QStringLiteral(" AND modes.dxcc = :mode");

    sql += QStringLiteral(" AND (%1))")
               .arg(confirmationCondition(sources, QStringLiteral("contacts")));

    QSqlQuery query;
    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Cannot prepare DXCC confirmation statement" << query.lastError();
        return DxccConfirmationResult::Error;
    }

    query.bindValue(QStringLiteral(":dxcc"), dxcc);
    if ( myDxcc != 0 )
        query.bindValue(QStringLiteral(":myDxcc"), myDxcc);
    if ( context == AlertDxccContext::Band )
        query.bindValue(QStringLiteral(":band"), cacheBand);
    else if ( context == AlertDxccContext::Mode )
        query.bindValue(QStringLiteral(":mode"), cacheMode);

    if ( !query.exec() || !query.next() )
    {
        qCWarning(runtime) << "Cannot execute DXCC confirmation statement" << query.lastError();
        return DxccConfirmationResult::Error;
    }

    const DxccConfirmationResult result = query.value(0).toBool()
                                              ? DxccConfirmationResult::Confirmed
                                              : DxccConfirmationResult::NotConfirmed;
    cache.insert(dxcc, myDxcc, cacheBand, cacheMode, new DxccConfirmationResult(result));
    return result;
}

void DxccConfirmation::invalidate(int dxcc, int myDxcc)
{
    FCT_IDENTIFICATION;

    cache.invalidate(dxcc, myDxcc);
}

void DxccConfirmation::clear()
{
    FCT_IDENTIFICATION;

    cache.clear();
}

QString DxccConfirmation::confirmationCondition(DxccConfirmationSources sources,
                                                const QString &tableAlias)
{
    FCT_IDENTIFICATION;

    const QString prefix = tableAlias.isEmpty() ? QString() : tableAlias + QLatin1Char('.');
    QStringList conditions;

    if ( sources.testFlag(DxccConfirmationSource::LotwConfirmation) )
        conditions << prefix + QLatin1String("lotw_qsl_rcvd = 'Y'");
    if ( sources.testFlag(DxccConfirmationSource::PaperConfirmation) )
        conditions << prefix + QLatin1String("qsl_rcvd = 'Y'");
    if ( sources.testFlag(DxccConfirmationSource::EqslConfirmation) )
        conditions << prefix + QLatin1String("eqsl_qsl_rcvd = 'Y'");

    return conditions.isEmpty()
               ? QStringLiteral("0=1")
               : conditions.join(QLatin1String(" OR "));
}

QString DxccConfirmation::normalizedMode(const QString &mode)
{
    FCT_IDENTIFICATION;

    return mode == BandPlan::MODE_GROUP_STRING_FTx
               ? BandPlan::MODE_GROUP_STRING_DIGITAL
               : mode;
}
