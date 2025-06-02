#ifndef DOWNLOADQSLDIALOG_H
#define DOWNLOADQSLDIALOG_H

#include <QDialog>
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
                         bool qslSinceActive,
                         const QString &settingString);
    void startNextDownload();

    Ui::DownloadQSLDialog *ui;
    LogLocale locale;
    QHash<QString, QSLMergeStat> downloadStat;
    QQueue<std::function<void()>> downloadQueue;

private slots:
    void downloadQSLs();

};

#endif // DOWNLOADQSLDIALOG_H
