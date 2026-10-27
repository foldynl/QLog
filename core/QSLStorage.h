#ifndef QLOG_CORE_QSLSTORAGE_H
#define QLOG_CORE_QSLSTORAGE_H

#include <QObject>
#include <QSqlRecord>
#include <QVariant>
#include <QList>
#include <QMap>
#include <QDateTime>

class QSLObject
{
public:
    enum SourceType
    {
        QSLFILE = 0,
        EQSL = 1,
    };

    enum BLOBFormat
    {
        BASE64FORM,
        RAWBYTES
    };

    explicit QSLObject( const qulonglong &qsoID,
                        const SourceType source,
                        const QString &qslName,
                        const QByteArray &inBlob,
                        const BLOBFormat format) :
        qsoID(qsoID),
        source(source),
        qslName(qslName),
        blob((format == RAWBYTES) ? inBlob : QByteArray::fromBase64(inBlob))
        {};

    explicit QSLObject( const QSqlRecord &qso,
                        const SourceType source,
                        const QString &qslName,
                        const QByteArray &inBlob,
                        const BLOBFormat format) :
        QSLObject(qso.value("id").toULongLong(),
                  source, qslName, inBlob, format)
    {}

    qulonglong getQSOID() const {return qsoID;};
    QSLObject::SourceType getSource() const {return source;};
    QString getQSLName() const {return qslName;};
    QByteArray getBLOB(BLOBFormat format = RAWBYTES) const {return (format == BASE64FORM) ? blob.toBase64() : blob;};

private:
    qulonglong qsoID;
    SourceType source;
    QString qslName;
    QByteArray blob;
};

struct QSLGalleryItem
{
    qulonglong contactId;
    QSLObject::SourceType source;
    QString name;
    QString callsign;
    QDateTime startTime;
    QString country;
    bool favorite = false;
};

class QSLStorage : public QObject
{
    Q_OBJECT

public:
    explicit QSLStorage(QObject *parent = nullptr);

    bool add(const QSLObject &);
    bool remove(const QSqlRecord &qso,
                const QSLObject::SourceType source,
                const QString &qslName);
    QStringList getAvailableQSLNames(const QSqlRecord &qso,
                                     const QSLObject::SourceType sourceFilter) const;
    QSLObject getQSL(const QSqlRecord &qso,
                     const QSLObject::SourceType source,
                     const QString &qslName) const;

    struct FilterValues
    {
        QMap<QString, int> countries; // localizedName -> dxcc_id
        QMap<QString, QStringList> yearMonths; // year -> sorted list of months ("01".."12")
        QStringList bands;
        QStringList modes;
        QStringList continents;
    };

    FilterValues getDistinctFilterValues() const;

    QList<QSLGalleryItem> getGalleryItems() const;
    QList<QSLGalleryItem> getGalleryItemsByDxcc(int dxcc) const;
    QList<QSLGalleryItem> getGalleryItemsByYear(const QString &year) const;
    QList<QSLGalleryItem> getGalleryItemsByYearMonth(const QString &year, const QString &month) const;
    QList<QSLGalleryItem> getGalleryItemsFavorite() const;
    QList<QSLGalleryItem> getGalleryItemsByBand(const QString &band) const;
    QList<QSLGalleryItem> getGalleryItemsByMode(const QString &mode) const;
    QList<QSLGalleryItem> getGalleryItemsByContinent(const QString &continent) const;

    QByteArray getQSLData(qulonglong contactId, int source, const QString &name, QString *error = nullptr) const;

    bool setFavorite(qulonglong contactId, QSLObject::SourceType source, const QString &name, bool favorite);
    bool isFavorite(qulonglong contactId, QSLObject::SourceType source, const QString &name) const;

private:
    const QString galleryBaseSQL=
        "SELECT q.contactid, q.source, q.name, c.callsign, c.start_time, translate_to_locale(c.country), q.favorite "
        "FROM contacts_qsl_cards q "
        "JOIN contacts c ON q.contactid = c.id ";

};

#endif // QLOG_CORE_QSLSTORAGE_H
