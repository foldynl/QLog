#ifndef QLOG_UI_QSOFILTERDETAIL_H
#define QLOG_UI_QSOFILTERDETAIL_H

#include <QDialog>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include "core/LogLocale.h"

namespace Ui {
class QSOFilterDetail;
}

class QSOFilterDetail : public QDialog
{
    Q_OBJECT

public:
    explicit QSOFilterDetail(const QString &filterName = QString(), QWidget *parent = nullptr);
    ~QSOFilterDetail();

public slots:
    void addCondition(int fieldIdx = -1, int operatorId = -1, QString value = QString());
    void save();
    void filterNameChanged(const QString&);

private:
    Ui::QSOFilterDetail *ui;
    QString filterName;
    int condCount;
    QStringList filterNamesList;

private:
    void loadFilter(const QString &filterName);
    bool filterExists(const QString &filterName);
    bool isDateField(int index);
    bool isDateTimeField(int index);
    bool isQSLSentField(int index);
    bool isQSLSentViaField(int index);
    bool isQSLRcvdField(int index);
    bool isUploadStatusField(int index);
    bool isAntPathField(int index);
    bool isBoolField(int index);
    bool isQSOCompleteField(int index);
    bool isDownloadStatusField(int index);
    bool isMorseKeyTypeField(int index);
    bool isEqslAgTypeField(int index);
    QComboBox* createComboBox(const QMap<QString, QString>&, const QString&,
                              const int identifier, const QSizePolicy&);
    QDateEdit* createDateEdit(const QString&, const int, const QSizePolicy&);
    QDateTimeEdit* createDateTimeEdit(const QString&, const int, const QSizePolicy&);
    QLineEdit* createLineEdit(const QString&, const int, const QSizePolicy&);

    LogLocale locale;
};

#endif // QLOG_UI_QSOFILTERDETAIL_H
