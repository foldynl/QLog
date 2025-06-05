#ifndef QLOG_LOGFORMAT_POTALOGFORMAT_H
#define QLOG_LOGFORMAT_POTALOGFORMAT_H
#include "AdiFormat.h"

/*
 * A specialized case of ADI export, where each activated park gets T'd into its
 * own file with some denormalization and values set to satisfy the pota.app
 * upload processes.
 */
class PotaAdiFormat : public AdiFormat
{
public:
    explicit PotaAdiFormat(QTextStream &stream);

    virtual void exportContact(const QSqlRecord &,
                               QMap<QString, QString> *applTags = nullptr) override;
    virtual void exportEnd() override;

    virtual bool importNext(QSqlRecord &) override { return false; }

    void setExportDirectory(const QString &dir);
    ~PotaAdiFormat();

private:
    QString exportDir;
    QDateTime currentDate;

    struct ParkFormatter
    {
        AdiFormat *formatter = nullptr;
        QFile *file = nullptr;
        QTextStream *stream = nullptr;

        ~ParkFormatter()
        {
            if ( formatter ) delete formatter;
            if ( stream ) delete stream;
            if ( file ) delete file;
        }
    };

    QHash<QString, ParkFormatter *> parkFormatters;

    AdiFormat *getActivatorParkFormatter(const QSqlRecord &record);
    void moveFieldValue(QSqlRecord &record,
                        const QString &fromFieldName,
                        const QString &toFieldName);
    bool isValidPotaRecord(const QSqlRecord &record) const;
    void preparePotaField(QSqlRecord &record, const QString &fromField,
                          const QString &toField, const QString &toFieldSig);
    void expandParkRecord(QList<QSqlRecord> &inputList, const QString &columnName);
};

#endif // QLOG_LOGFORMAT_POTALOGFORMAT_H
