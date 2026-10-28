#ifndef DOWNLOADQSLDIALOG_H
#define DOWNLOADQSLDIALOG_H

#include <QDialog>
#include <functional>
#include "core/LogLocale.h"
#include "logformat/LogFormat.h"
#include "service/GenericQSLDownloader.h"

namespace Ui {
class DownloadQSLDialog;
}

class DownloadQSLDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadQSLDialog(QWidget *parent = nullptr);
    ~DownloadQSLDialog();

private:
    void prepareDownload(GenericQSLDownloader* service,
                         const QString &serviceName,
                         const std::function<void()> &downloadSucceeded);
    void startNextDownload();
    void loadDialogState();
    void loadLotwDate();
    void loadEqslDate();
    void saveDialogState();
    QStringList selectedLotwCallsigns() const;

    Ui::DownloadQSLDialog *ui;
    LogLocale locale;
    QHash<QString, QSLMergeStat> downloadStat;
    QQueue<std::function<void()>> downloadQueue;

private slots:
    void downloadQSLs();
    void reject() override;

};

#endif // DOWNLOADQSLDIALOG_H
