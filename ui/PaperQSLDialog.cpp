#include <QLabel>
#include <QDesktopServices>
#include <QMessageBox>
#include <QFileDialog>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSettings>

#include "PaperQSLDialog.h"
#include "ui_PaperQSLDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.paperqsldialog");

PaperQSLDialog::PaperQSLDialog(const QSqlRecord &qso, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PaperQSLDialog),
    qsl(new PaperQSL()),
    dialogQSORecord(qso),
    fileCount(0)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    showAvailableFiles();
}

PaperQSLDialog::~PaperQSLDialog()
{
    FCT_IDENTIFICATION;

    delete qsl;
    delete ui;
}

void PaperQSLDialog::addFileClick()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const QString &lastPath = settings.value("paperqslimport/last_path", QDir::homePath()).toString();

    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Add File"),
                                                    lastPath,
#if defined(Q_OS_WIN)
                                                    ""
#elif defined(Q_OS_MACOS)
                                                    ""
#else
                                                    ""
#endif
                                                   );
    if ( !filename.isEmpty() )
    {
        qCDebug(runtime) << "selected file " << filename;

        settings.setValue("paperqslimport/last_path", QFileInfo(filename).path());

        QString newFilename;

        if ( qsl->addQSLFile(filename, dialogQSORecord, newFilename) )
        {
           addFileToDialog(QFileInfo(newFilename));
        }
    }
}

void PaperQSLDialog::showAvailableFiles()
{
    FCT_IDENTIFICATION;

    QFileInfoList files = qsl->getQSLFileList(dialogQSORecord);

    for ( auto &file : qAsConst(files) )
    {
        addFileToDialog(file);
    }
}

void PaperQSLDialog::addFileToDialog(const QFileInfo &file)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << file;

    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->setObjectName(QString::fromUtf8("fileLayout%1").arg(fileCount));

    /*****************/
    /* Remove Button */
    /*****************/
    QPushButton* removeButton = new QPushButton(tr("Remove"), this);
    removeButton->setObjectName(QString::fromUtf8("removeButton%1").arg(fileCount));

    fileLayout->addWidget(removeButton);

    connect(removeButton, &QPushButton::clicked, this, [this, fileLayout, file]()
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Delete"), tr("Delete QSL file?"),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply != QMessageBox::Yes) return;

        QFile removedFile(file.absoluteFilePath());

        if ( removedFile.remove() )
        {
            QLayoutItem *item = NULL;
            while ((item = fileLayout->takeAt(0)) != 0)
            {
                delete item->widget();
                delete item;
            }
            fileLayout->deleteLater();
        }
    });

    /*****************/
    /* Open Button */
    /*****************/
    QPushButton* openButton = new QPushButton(tr("Open"), this);
    openButton->setObjectName(QString::fromUtf8("openButton%1").arg(fileCount));

    fileLayout->addWidget(openButton);

    connect(openButton, &QPushButton::clicked, this, [file]()
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()));
    });

    /***************/
    /* File label  */
    /***************/

    QLabel *fileLabel = new QLabel(qsl->stripBaseFileName(file.fileName()).left(80), this);
    fileLabel->setObjectName(QString::fromUtf8("fileLabel%1").arg(fileCount));
    fileLabel->setMaximumWidth(200);
    fileLabel->setMinimumWidth(200);
    QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(fileLabel->sizePolicy().hasHeightForWidth());
    fileLabel->setSizePolicy(sizePolicy1);

    QMimeDatabase mimeDatabase;
    QMimeType mimeType;

    mimeType = mimeDatabase.mimeTypeForFile(file);

    if ( mimeType.name().contains("image", Qt::CaseInsensitive))
    {
       fileLabel->setToolTip(QString("<img src='%1'>").arg(file.absoluteFilePath()));
    }

    fileLayout->addWidget(fileLabel);

    ui->filesListLayout->addLayout(fileLayout);

    fileCount++;
}
