#include <algorithm>
#include <QDir>
#include <QSet>
#include <QSqlQuery>
#include <QSqlError>

#include "QSLStorage.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.qslstorage");

QSLStorage::QSLStorage(QObject *parent) : QObject(parent)
{
    FCT_IDENTIFICATION;
}

bool QSLStorage::add(const QSLObject &qslObject)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << qslObject.getQSOID()
                                 << qslObject.getSource()
                                 << qslObject.getQSLName();
    QSqlQuery insert;

    if ( !insert.prepare("REPLACE INTO contacts_qsl_cards (contactid, source, name, data) "
                         " VALUES (:contactid, :source, :name, :data)" ) )
    {
        qCDebug(runtime) << " Cannot prepare INSERT for PaperQSL " << insert.lastError();
        return false;
    }

    insert.bindValue(":contactid", qslObject.getQSOID());
    insert.bindValue(":source", qslObject.getSource());
    insert.bindValue(":name", qslObject.getQSLName());
    insert.bindValue(":data", qslObject.getBLOB(QSLObject::BASE64FORM));

    if ( !insert.exec() )
    {
        qCDebug(runtime) << "Cannot import QSL" << insert.lastError();
        return false;
    }
    return true;
}

bool QSLStorage::remove(const QSqlRecord &qso,
                        const QSLObject::SourceType source,
                        const QString &qslName)
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare("DELETE FROM contacts_qsl_cards "
                        "WHERE source = :source "
                        "AND contactid = :contactid "
                        "AND name = :qsl_name"))
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement";
        return false;
    }

    query.bindValue(":source", source);
    query.bindValue(":contactid", qso.value("id"));
    query.bindValue(":qsl_name", qslName);

    if ( !query.exec() )
    {
        qCDebug(runtime) << "Cannot delete QSL file" << qslName;
        return false;
    }

    return true;
}

QStringList QSLStorage::getAvailableQSLNames(const QSqlRecord &qso,
                                             const QSLObject::SourceType sourceFilter) const
{
    FCT_IDENTIFICATION;

    QStringList ret;
    QSqlQuery query;

    if ( !query.prepare("SELECT name FROM contacts_qsl_cards "
                        "WHERE source = :source "
                        "AND contactid = :contactid "
                        "ORDER BY name"))
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement";
        return ret;
    }

    query.bindValue(":source", sourceFilter);
    query.bindValue(":contactid", qso.value("id"));

    if ( query.exec() )
    {
        while(query.next())
        {
            ret << query.value(0).toString();
        }
    }
    else
    {
        qCDebug(runtime) << "Error" << query.lastError();
    }
    return ret;
}

QSLObject QSLStorage::getQSL(const QSqlRecord &qso,
                             const QSLObject::SourceType source,
                             const QString &qslName) const
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare("SELECT data FROM contacts_qsl_cards "
                        "WHERE source = :source "
                        "AND contactid = :contactid "
                        "AND name = :qsl_name "
                        "ORDER BY name LIMIT 1"))
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement";
    }
    else
    {
        query.bindValue(":source", source);
        query.bindValue(":contactid", qso.value("id"));
        query.bindValue(":qsl_name", qslName);

        if ( query.exec() && query.next() )
            return QSLObject(qso, source, qslName, query.value(0).toByteArray(), QSLObject::BASE64FORM);
    }

    return QSLObject (qso, source, qslName, QByteArray(), QSLObject::RAWBYTES);
}

QSLStorage::FilterValues QSLStorage::getDistinctFilterValues() const
{
    FCT_IDENTIFICATION;

    FilterValues ret;
    QSqlQuery query;

    if ( !query.prepare("SELECT DISTINCT translate_to_locale(c.country), strftime('%Y', c.start_time), "
                        "strftime('%m', c.start_time), c.band, c.mode, c.cont, c.dxcc "
                        "FROM contacts_qsl_cards q "
                        "JOIN contacts c ON q.contactid = c.id") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return ret;
    }

    QSet<QString> bands, modes, continents;
    QMap<QString, QSet<QString>> yearMonthSets;

    if ( query.exec() )
    {
        while ( query.next() )
        {
            const QString country = query.value(0).toString();
            const QString year = query.value(1).toString();
            const QString month = query.value(2).toString();
            const QString band = query.value(3).toString();
            const QString mode = query.value(4).toString();
            const QString cont = query.value(5).toString();
            const int dxcc = query.value(6).toInt();

            if ( !country.isEmpty() && dxcc > 0 )
                ret.countries.insert(country, dxcc);
            if ( !year.isEmpty() )    yearMonthSets[year].insert(month.isEmpty() ? "00" : month);
            if ( !band.isEmpty() )    bands.insert(band);
            if ( !mode.isEmpty() )    modes.insert(mode);
            if ( !cont.isEmpty() )    continents.insert(cont);
        }
    }
    else
    {
        qCDebug(runtime) << "Error" << query.lastError();
    }

    ret.bands = bands.values();
    ret.modes = modes.values();
    ret.continents = continents.values();

    // Build sorted yearMonths map
    for ( QMap<QString, QSet<QString>>::const_iterator it = yearMonthSets.constBegin(); it != yearMonthSets.constEnd(); ++it )
    {
        QStringList months = it.value().values();
        std::sort(months.begin(), months.end());
        ret.yearMonths.insert(it.key(), months);
    }

    std::sort(ret.bands.begin(), ret.bands.end());
    std::sort(ret.modes.begin(), ret.modes.end());
    std::sort(ret.continents.begin(), ret.continents.end());

    return ret;
}

static QList<QSLGalleryItem> executeGalleryQuery(QSqlQuery &query)
{
    QList<QSLGalleryItem> ret;

    if ( query.exec() )
    {
        while ( query.next() )
        {
            QSLGalleryItem item;
            item.contactId = query.value(0).toULongLong();
            item.source = static_cast<QSLObject::SourceType>(query.value(1).toInt());
            item.name = query.value(2).toString();
            item.callsign = query.value(3).toString();
            item.startTime = query.value(4).toDateTime();
            item.country = query.value(5).toString();
            item.favorite = query.value(6).toBool();
            ret << item;
        }
    }

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItems() const
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items found or error" << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByDxcc(int dxcc) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE c.dxcc = :dxcc ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":dxcc", dxcc);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for dxcc" << dxcc << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByYear(const QString &year) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << year;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE strftime('%Y', c.start_time) = :year ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":year", year);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for year" << year << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByYearMonth(const QString &year, const QString &month) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << year << month;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE strftime('%Y', c.start_time) = :year "
                        "AND strftime('%m', c.start_time) = :month ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":year", year);
    query.bindValue(":month", month);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for" << year << month << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByBand(const QString &band) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << band;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE c.band = :band ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":band", band);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for band" << band << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByMode(const QString &mode) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE c.mode = :mode ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":mode", mode);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for mode" << mode << query.lastError();

    return ret;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsByContinent(const QString &continent) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << continent;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE c.cont = :cont ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    query.bindValue(":cont", continent);

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No gallery items for continent" << continent << query.lastError();

    return ret;
}

QByteArray QSLStorage::getQSLData(qulonglong contactId, int source, const QString &name, QString *error) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << contactId << source << name;
    if ( error )
        error->clear();

    QSqlQuery query;

    if ( !query.prepare("SELECT data FROM contacts_qsl_cards "
                        "WHERE contactid = :contactid AND source = :source AND name = :name "
                        "LIMIT 1") )
    {
        qCWarning(runtime) << "Cannot prepare QSL read" << query.lastError();
        if ( error )
            *error = tr("Cannot look up the QSL gallery image %1:\n%2").arg(name, query.lastError().text());
        return QByteArray();
    }

    query.bindValue(":contactid", static_cast<quint64>(contactId));
    query.bindValue(":source", source);
    query.bindValue(":name", name);

    QByteArray data;
    if ( query.exec() && query.next() )
        data = QByteArray::fromBase64(query.value(0).toByteArray());

    if ( data.isEmpty() && error )
        *error = query.lastError().isValid() ? tr("Cannot load the QSL gallery image data for %1:\n%2").arg(name, query.lastError().text())
                                           : tr("Cannot export %1. The QSL image is missing or empty.").arg(name);

    if ( data.isEmpty() )
        qCDebug(runtime) << "QSL data not found" << query.lastError();
    return data;
}

QList<QSLGalleryItem> QSLStorage::getGalleryItemsFavorite() const
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare(galleryBaseSQL + "WHERE q.favorite = 1 ORDER BY c.start_time DESC") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return QList<QSLGalleryItem>();
    }

    QList<QSLGalleryItem> ret = executeGalleryQuery(query);

    if ( ret.isEmpty() )
        qCDebug(runtime) << "No favorite gallery items found or error" << query.lastError();

    return ret;
}

bool QSLStorage::setFavorite(qulonglong contactId, QSLObject::SourceType source, const QString &name, bool favorite)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << contactId << source << name << favorite;

    QSqlQuery query;

    if ( !query.prepare("UPDATE contacts_qsl_cards SET favorite = :fav "
                        "WHERE contactid = :contactid AND source = :source AND name = :name") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return false;
    }

    query.bindValue(":fav", favorite ? 1 : 0);
    query.bindValue(":contactid", static_cast<quint64>(contactId));
    query.bindValue(":source", static_cast<int>(source));
    query.bindValue(":name", name);

    if ( !query.exec() )
    {
        qCDebug(runtime) << "Cannot update favorite" << query.lastError();
        return false;
    }

    return true;
}

bool QSLStorage::isFavorite(qulonglong contactId, QSLObject::SourceType source, const QString &name) const
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare("SELECT favorite FROM contacts_qsl_cards "
                        "WHERE contactid = :contactid AND source = :source AND name = :name "
                        "LIMIT 1") )
    {
        qCDebug(runtime) << "Cannot prepare SQL Statement" << query.lastError();
        return false;
    }

    query.bindValue(":contactid", static_cast<quint64>(contactId));
    query.bindValue(":source", static_cast<int>(source));
    query.bindValue(":name", name);

    if ( query.exec() && query.next() )
        return query.value(0).toBool();

    return false;
}
