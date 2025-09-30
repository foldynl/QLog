#ifndef QLOG_LOGFORMAT_ADIFORMAT_H
#define QLOG_LOGFORMAT_ADIFORMAT_H

#include "LogFormat.h"

class AdiFormat : public LogFormat
{
public:
    explicit AdiFormat(QTextStream& stream);

    virtual bool importNext(QSqlRecord& ) override;

    virtual void exportContact(const QSqlRecord&,
                               QMap<QString, QString> *applTags = nullptr) override;
    virtual void exportStart() override;

    static QMap<QString, QString> fieldname2INTLNameMapping;

    template<typename T>
    static void preprocessINTLFields(T &contact)
    {
        {
            const QStringList &fieldMappingList = fieldname2INTLNameMapping.keys();
            for ( const QString& fieldName :  fieldMappingList )
                preprocessINTLField(fieldName, fieldname2INTLNameMapping.value(fieldName), contact);
        }
    }

protected:
    virtual void writeField(const QString &name,
                            bool presenceCondition,
                            const QString &value,
                            const QString &type="");
    virtual void writeSQLRecord(const QSqlRecord& record,
                                QMap<QString, QString> *applTags);
    virtual bool readContact(QVariantMap &);
    void mapContact2SQLRecord(QMap<QString, QVariant> &contact,
                              QSqlRecord &record);
    void contactFields2SQLRecord(QMap<QString, QVariant> &contact,
                              QSqlRecord &record);

    enum OutputFieldFormatter
    {
        TOSTRING,
        TOLOWER,
        TOUPPER,
        TODATE,
        TOTIME,
        REMOVEDEFAULTVALUEN
    };

    const QString formatOuput(OutputFieldFormatter formatter, const QVariant &in);

    virtual const QString toString(const QVariant &);
    virtual const QString toLower(const QVariant &);
    virtual const QString toUpper(const QVariant &);
    virtual const QString toDate(const QVariant &);
    virtual const QString toTime(const QVariant &);
    virtual const QString removeDefaulValueN(const QVariant &);

    class ExportParams
    {
    public:
        ExportParams() :
            ADIFName(QString()),
            outputType(QString()),
            formatter(TOSTRING),
            isValid(false) {};
        ExportParams(const QString &inADIFName,
                     const OutputFieldFormatter formatter = OutputFieldFormatter::TOSTRING,
                     const QString &inType = QString()) :
            ADIFName(inADIFName),
            outputType(inType),
            formatter(formatter),
            isValid(true) {};
        QString ADIFName;
        QString outputType;
        OutputFieldFormatter formatter;
        bool isValid;
    };

    static QHash<QString, AdiFormat::ExportParams> DB2ADIFExportParams;

    const QString ADIF_VERSION_STRING = "3.1.6";
    const QString PROGRAMID_STRING = "QLog";

private:

    void readField(QString& field,
                   QString& value);
    QDate parseDate(const QString &date);
    QTime parseTime(const QString &time);
    QString parseQslRcvd(const QString &value);
    QString parseQslSent(const QString &value);
    QString parseUploadStatus(const QString &value);
    QString parseDownloadStatus(const QString &value);
    QString parseMorseKeyType(const QString &value);
    QString parseEqslAg(const QString &value);

    enum ParserState {
        START,
        FIELD,
        KEY,
        SIZE,
        DATA_TYPE,
        VALUE
    };

    static void preprocessINTLField(const QString &fieldName,
                                    const QString &fieldIntlName,
                                    QMap<QString, QVariant> &contact);
    static void preprocessINTLField(const QString &fieldName,
                                    const QString &fieldIntlName,
                                    QSqlRecord &contact);

    ParserState state = START;
    bool inHeader = false;
};

#endif // QLOG_LOGFORMAT_ADIFORMAT_H
