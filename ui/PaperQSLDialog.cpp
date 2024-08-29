#include <QLabel>
#include <QDesktopServices>
#include <QMessageBox>
#include <QFileDialog>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSettings>
#include <QTemporaryDir>
#include "PaperQSLDialog.h"
#include "ui_PaperQSLDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.paperqsldialog");

extern QTemporaryDir tempDir;

PaperQSLDialog::PaperQSLDialog(const QSqlRecord &qso, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PaperQSLDialog),
    qsl(new QSLStorage(this)),
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
                                                    "",
#elif defined(Q_OS_MACOS)
                                                    "",
#else
                                                    "",
#endif
                                                    nullptr,
#if defined(Q_OS_LINUX)
                                                    // Do not use the Native Dialog under Linux because the dialog is case-sensitive.
                                                    // QT variant looks different but it is case-insensitive.
                                                    // More information:
                                                    // https://stackoverflow.com/questions/34858220/qt-how-to-set-a-case-insensitive-filter-on-qfiledialog
                                                    // https://bugreports.qt.io/browse/QTBUG-51712
                                                    QFileDialog::DontUseNativeDialog
#else
                                                    QFileDialog::Options()
#endif
                                                   );
    if ( !filename.isEmpty() )
    {
        qCDebug(runtime) << "selected file " << filename;

        QFile file(filename);

        if ( !file.open(QIODevice::ReadOnly) )
            return;

        settings.setValue("paperqslimport/last_path", QFileInfo(filename).path());

        if ( qsl->add(QSLObject(dialogQSORecord, QSLObject::QSLFILE,
                                QFileInfo(file).fileName(), file.readAll(),
                                QSLObject::RAWBYTES)) )
        {
           addFileToDialog(filename);
        }
    }
}

void PaperQSLDialog::showAvailableFiles()
{
    FCT_IDENTIFICATION;

    QStringList files = qsl->getAvailableQSLNames(dialogQSORecord, QSLObject::QSLFILE);

    for ( auto &file : qAsConst(files) )
    {
        addFileToDialog(file);
    }
}

void PaperQSLDialog::addFileToDialog(const QString &inFile)
{
    FCT_IDENTIFICATION;

    QFileInfo file(inFile);

    qCDebug(function_parameters) << file.fileName();

    // if file already exists, do not add it to dialog.
    if ( filenameList.contains(file.fileName() ) )
         return;

    filenameList << file.fileName();

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
        reply = QMessageBox::question(this, tr("Delete"), tr("Delete QSL?"),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply != QMessageBox::Yes) return;

        if ( qsl->remove(dialogQSORecord, QSLObject::QSLFILE, file.fileName()) )
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

    connect(openButton, &QPushButton::clicked, this, [this, file]()
    {
        if ( !tempDir.isValid() )
        {
            qCDebug(runtime) << "Temp directory" << tempDir.path()<< "for QSL is not valid";
            return;
        }

        QFile f(tempDir.path() + QDir::separator() + file.fileName());
        qCDebug(runtime) << "Using temp file" << f.fileName();

        if ( f.open(QFile::WriteOnly) )
        {
            f.write(qsl->getQSL(dialogQSORecord,
                                QSLObject::QSLFILE,
                                file.fileName()).getBLOB());
            f.flush();
            f.close();
        }
        else
        {
            qWarning() << "Cannot open file for QSL";
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(f.fileName()));
    });

    /***************/
    /* File label  */
    /***************/

    QLabel *fileLabel = new QLabel(file.fileName().left(80), this);
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
       // QByteArray->QString in arg is due to QT5.12
       fileLabel->setToolTip(QString("<img src='data:%0;base64, %1'>").arg(mimeType.name(), QString(qsl->getQSL(dialogQSORecord,
                                                                                                                QSLObject::QSLFILE,
                                                                                                                file.fileName()).getBLOB(QSLObject::BASE64FORM))));
    }

    fileLayout->addWidget(fileLabel);

    ui->filesListLayout->addLayout(fileLayout);

    fileCount++;
}
