#ifndef QLOG_UI_EMAILQSLDIALOG_H
#define QLOG_UI_EMAILQSLDIALOG_H

#include <QDialog>
#include <QSqlRecord>

#include "service/emailqsl/EmailQSLService.h"

namespace Ui {
class EmailQSLDialog;
}

// Shown before sending an Email QSL. Displays contact details, the rendered
// card thumbnail, warnings about duplicate sends, and lets the user confirm.
class EmailQSLDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmailQSLDialog(const QSqlRecord &record, QWidget *parent = nullptr);
    ~EmailQSLDialog();

private slots:
    void sendClicked();
    void onSendFinished(bool success, const QString &message);
    void previewAndPrintCard();

private:
    void populateInfo();
    void buildWarnings();

    Ui::EmailQSLDialog *ui;
    QSqlRecord          m_record;
    EmailQSLService    *m_service;
};

#endif // QLOG_UI_EMAILQSLDIALOG_H
