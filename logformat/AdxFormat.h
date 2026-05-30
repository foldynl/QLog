#ifndef QLOG_LOGFORMAT_ADXFORMAT_H
#define QLOG_LOGFORMAT_ADXFORMAT_H

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "AdiFormat.h"

class AdxFormat : public AdiFormat
{
public:
    explicit AdxFormat(QTextStream& stream);

    virtual void importStart() override;
    virtual void importEnd() override;

    virtual void exportContact(const QSqlRecord& record,
                               QMap<QString, QString> *applTags = nullptr) override;
    virtual void exportStart() override;
    virtual void exportEnd() override;

protected:
    virtual void writeField(const QString &name,
                            bool presenceCondition,
                            const QString &value,
                            const QString &type="") override;
    virtual void writeSQLRecord(const QSqlRecord& record,
                                QMap<QString, QString> *applTags) override;
    virtual bool readContact(QVariantMap& ) override;

private:
    static QString applicationFieldKey(const QString &programId,
                                       const QString &fieldName);
    static QString attributeValue(const QXmlStreamAttributes &attributes,
                                  const QString &name);
    static bool splitApplicationFieldName(const QString &name,
                                          QString &programId,
                                          QString &fieldName);
    bool writeApplicationField(const QString &name,
                               bool presenceCondition,
                               const QString &value,
                               const QString &type);

    QXmlStreamWriter *writer;
    QXmlStreamReader *reader;
};

#endif // QLOG_LOGFORMAT_ADXFORMAT_H
