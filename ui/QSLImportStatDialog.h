#ifndef QLOG_UI_QSLIMPORTSTATDIALOG_H
#define QLOG_UI_QSLIMPORTSTATDIALOG_H

#include <QDialog>
#include <logformat/LogFormat.h>

namespace Ui {
class QSLImportStatDialog;
}

class QSLImportStatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QSLImportStatDialog(const QSLMergeStat &stats,
                                 QWidget *parent = nullptr);
    explicit QSLImportStatDialog(const QHash<QString, QSLMergeStat> &stats,
                                 QWidget *parent = nullptr);
    ~QSLImportStatDialog();

private:
    Ui::QSLImportStatDialog *ui;
    void showStat(const quint64 updated,
                  const quint64 downloaded,
                  const quint64 unmatched,
                  const quint64 errors,
                  const QString  &newQSLText,
                  const QString  &updatedQSLText,
                  const QString  &unmatchedQSLText);
};

#endif // QLOG_UI_QSLIMPORTSTATDIALOG_H
