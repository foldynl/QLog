#ifndef UPLOADQSODIALOG_H
#define UPLOADQSODIALOG_H

#include <QDialog>
#include <QAbstractButton>
#include <QQueue>
#include <QCheckBox>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QLabel>

#include "core/LogLocale.h"
#include "service/GenericQSOUploader.h"
#include "service/cloudlog/Cloudlog.h"

namespace Ui {
class UploadQSODialog;
}

class UploadQSODialog : public QDialog
{
    Q_OBJECT

public:
    explicit UploadQSODialog(QWidget *parent = nullptr);
    ~UploadQSODialog();

private slots:
    void showQSODetails();
    void setEQSLSettingVisible(bool visible);
    void setClublogSettingVisible(bool visible);
    void setWavelogSettingVisible(bool visible);
    void startUploadQueue();
    void executeQuery();
    void handleCallsignChange(const QString &);
    void updateWavelogStationLabel();

private:

    // The order is important here, because some services
    // send fields from other services.
    enum ServiceID
    {
        UNKNOWN = 0,
        LOTWID = 1,     // independent
        EQSLID = 2,     // independent
        CLUBLOGID = 3,  // depends on the LoTW Receive field
        HRDLOGID = 4,   // depends on EQSL and LoTW fields
        QRZCOMID = 5,   // all fields are sent
        WAVELOGID = 6,    // all fiedls are sent
    };

    class UploadTask
    {
    public:
        UploadTask() : serviceID(UNKNOWN), uploader(nullptr), controElement(nullptr),
                       qsoNumberLabel(nullptr) {};

        UploadTask(ServiceID id,
                   const QString &serviceName,
                   GenericQSOUploader *uploader,
                   const QString &dbUploadStatusFieldName,
                   const QString &dbUploadDateFieldName,
                   QCheckBox *enableWidget,
                   QLabel *qsoCountLabel,
                   bool isActivable):
            serviceID(id), serviceName(serviceName),
            uploader(uploader), controElement(enableWidget),
            dbUploadStatusFieldName(dbUploadStatusFieldName), dbUploadDateFieldName(dbUploadDateFieldName),
            qsoNumberLabel(qsoCountLabel)
        {
            enableWidget->setEnabled(isActivable);
            enableWidget->setToolTip((isActivable) ? ""
                                                   : tr("Service is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
        };

        ~UploadTask() {};

        ServiceID getServiceID() const {return serviceID;};
        void addQSO(QSharedPointer<QSqlRecord> record) {qsoRefs.append(record); qsoIds.append(record->value("id").toULongLong());};
        void clearEnqueuedQSOs() {qsoRefs.clear(); qsoIds.clear();};
        const QList<QSqlRecord> getQSOList() const
        {
            QList<QSqlRecord> retList;
            const QList<QWeakPointer<QSqlRecord>> localRefs = qsoRefs;
            for ( const QWeakPointer<QSqlRecord>& weak : localRefs )
            {
                QSharedPointer<QSqlRecord> shared = weak.toStrongRef();
                if (shared) retList.append(*shared);
            }
            return retList;
        };
        const QList<qulonglong>& getQSOIDs() const { return qsoIds; };
        bool isChecked() {return (controElement) ? controElement->isChecked() : false;};
        const QString& getDBUploadStatusFieldName() const {return dbUploadStatusFieldName;};
        const QString& getDBUploadDateFieldName() const {return dbUploadDateFieldName;};
        const QString& getServiceName() const {return serviceName;};
        GenericQSOUploader *getUploader() const {return uploader;};
        void updateQSONumberLabel() {qsoNumberLabel->setText((isChecked()) ? "(" + QString::number(qsoRefs.size()) + ")"
                                                                         : "");};
        bool isQSOListEmpty() const {return qsoRefs.isEmpty(); };
        void updateAllDBFieldValue(const QString &column1, const QVariant &value1,
                                   const QString &column2, const QVariant &value2) const
        {
            const QList<QWeakPointer<QSqlRecord>> localRefs = qsoRefs;
            for ( const QWeakPointer<QSqlRecord>& weak : localRefs )
            {
                QSharedPointer<QSqlRecord> shared = weak.toStrongRef();
                if ( shared )
                {
                    shared->setValue(column1, value1);
                    shared->setValue(column2, value2);
                }
            }
        };

    private:
        ServiceID serviceID;
        QString serviceName;
        GenericQSOUploader *uploader;
        QList<QWeakPointer<QSqlRecord>> qsoRefs;
        QList<qulonglong> qsoIds;
        QCheckBox *controElement;
        QString dbUploadStatusFieldName;
        QString dbUploadDateFieldName;
        QLabel *qsoNumberLabel;
    };

    QMap<ServiceID, UploadTask> onlineServices;
    QHash<qulonglong, QSharedPointer<QSqlRecord>> allSelectedQSOs;
    Ui::UploadQSODialog *ui;
    LogLocale locale;
    QList<UploadTask> uploadTaskQueue;
    QStandardItemModel *detailQSOsModel;
    UploadTask currentTask;
    QMap<uint, CloudlogUploader::StationProfile> availableWavelogStationIDs;
    bool executeQueryEnabled;
    void setQSODetailVisible(bool visible);
    void loadDialogState();
    void saveDialogState();
    void processNextUploader();
    void uploadFinished();
    void updateQSONumbers();
    void getWavelogStationID();

    const uint WAVELOG_MAX_STATIONID = 99999;
};

#endif // UPLOADQSODIALOG_H
