#include "QSLImportStatDialog.h"
#include "ui_QSLImportStatDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.QSLImportStatDialog");

QSLImportStatDialog::QSLImportStatDialog(const QSLMergeStat &stats,
                                         QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLImportStatDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    showStat(stats.updatedQSOs.size() + stats.newQSLs.size(),
             stats.qsosDownloaded,
             stats.unmatchedQSLs.size(),
             stats.errorQSLs.size(),
             stats.newQSLs.join("\n"),
             stats.updatedQSOs.join("\n"),
             stats.unmatchedQSLs.join("\n"));
}

QSLImportStatDialog::QSLImportStatDialog(const QHash<QString, QSLMergeStat> &stats,
                                         QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLImportStatDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    quint64 updated = 0;
    quint64 downloaded = 0;
    quint64 unmatched = 0;
    quint64 errors = 0;
    QString newQSLText;
    QString updatedQSLText;
    QString unmatchedQSLText;

    for (auto i = stats.begin(), end = stats.end(); i != end; ++i)
    {
        updated    += (i->updatedQSOs.size() + i->newQSLs.size());
        downloaded += i->qsosDownloaded;
        unmatched  += i->unmatchedQSLs.size();
        errors     += i->errorQSLs.size();
        if ( !i.value().newQSLs.isEmpty() )
            newQSLText.append("* " + i.key() + "\n" + i.value().newQSLs.join("\n") + "\n\n");
        if ( !i.value().updatedQSOs.empty() )
            updatedQSLText.append("* " + i.key() + "\n" + i.value().updatedQSOs.join("\n") + "\n\n");
        if ( !i.value().unmatchedQSLs.isEmpty() )
            unmatchedQSLText.append("* " + i.key() + "\n" + i.value().unmatchedQSLs.join("\n") + "\n\n");
    }

    showStat(updated, downloaded, unmatched, errors,
             newQSLText, updatedQSLText, unmatchedQSLText);
}

void QSLImportStatDialog::showStat(const quint64 updated,
                                   const quint64 downloaded,
                                   const quint64 unmatched,
                                   const quint64 errors,
                                   const QString  &newQSLText,
                                   const QString  &updatedQSLText,
                                   const QString  &unmatchedQSLText)
{
    FCT_IDENTIFICATION;

    ui->updatedNumber->setText(QString::number(updated));
    ui->downloadedNumber->setText(QString::number(downloaded));
    ui->unmatchedNumber->setText(QString::number(unmatched));
    ui->errorsNumber->setText(QString::number(errors));

    if ( !newQSLText.isEmpty() )
    {
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText("*** " + tr("New QSLs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText(newQSLText);
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->moveCursor(QTextCursor::End);
    }

    if ( !updatedQSLText.isEmpty() )
    {
        ui->detailsText->insertPlainText("*** " + tr("Updated QSOs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText(updatedQSLText);
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->moveCursor(QTextCursor::End);
    }

    if ( !unmatchedQSLText.isEmpty() )
    {
        ui->detailsText->insertPlainText("*** " + tr("Unmatched QSLs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText (unmatchedQSLText);
        ui->detailsText->moveCursor(QTextCursor::End);
    }
}

QSLImportStatDialog::~QSLImportStatDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
