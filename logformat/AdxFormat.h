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
    bool hasExportError() const;

protected:
    QString importErrorString() const override;
    virtual void writeField(const QString &name,
                            bool presenceCondition,
                            const QString &value,
                            const QString &type="") override;
    virtual void writeSQLRecord(const QSqlRecord& record,
                                QMap<QString, QString> *applTags) override;
    virtual bool readContact(QVariantMap& ) override;

private:
    // Keep the per-field filter inline without function-entry logging overhead.
    static QString xmlSafeText(const QString &value)
    {
        QString result(value);
        const QChar *source = value.constData();
        QChar *destination = nullptr;
        QString::size_type length = 0;

        for ( QString::size_type index = 0; index < value.size(); ++index )
        {
            const ushort code = source[index].unicode();
            // Removing controls must not join originally isolated surrogates into a pair.
            if ( (code < 0x20 && code != 0x09 && code != 0x0a && code != 0x0d)
                 || code == 0xfffe || code == 0xffff
                 || (QChar::isHighSurrogate(code)
                     && (index + 1 == value.size() || !source[index + 1].isLowSurrogate()))
                 || (QChar::isLowSurrogate(code)
                     && (index == 0 || !source[index - 1].isHighSurrogate())) )
            {
                if ( !destination )
                    destination = result.data();
            }
            else
            {
                if ( destination )
                    destination[length] = source[index];
                ++length;
            }
        }

        if ( destination )
            result.truncate(length);

        return result;
    }

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
