#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

#include "RigctldAdvancedDialog.h"
#include "ui_RigctldAdvancedDialog.h"
#include "rig/RigctldManager.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.rigctldadvanceddialog");

RigctldAdvancedDialog::RigctldAdvancedDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RigctldAdvancedDialog),
    m_versionUpdateTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
#ifdef QLOG_FLATPAK
    ui->pathEdit->setEnabled(false);
    ui->browseButton->setEnabled(false);
    ui->autoButton->setEnabled(false);
    ui->pathEdit->setPlaceholderText(tr("Cannot be changed"));
#endif

    m_versionUpdateTimer->setSingleShot(true);
    m_versionUpdateTimer->setInterval(500);
    connect(m_versionUpdateTimer, &QTimer::timeout, this, &RigctldAdvancedDialog::updateVersionLabel);
    connect(ui->pathEdit, &QLineEdit::textChanged, this, [this]() {
        m_versionUpdateTimer->start();
    });

    updateVersionLabel();
}

RigctldAdvancedDialog::~RigctldAdvancedDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void RigctldAdvancedDialog::setPath(const QString &path)
{
    ui->pathEdit->blockSignals(true);
    ui->pathEdit->setText(path);
    ui->pathEdit->blockSignals(false);
    updateVersionLabel();
}

QString RigctldAdvancedDialog::getPath() const
{
    return ui->pathEdit->text().trimmed();
}

void RigctldAdvancedDialog::setArgs(const QString &args)
{
    ui->argsEdit->setText(args);
}

QString RigctldAdvancedDialog::getArgs() const
{
    return ui->argsEdit->text().trimmed();
}

void RigctldAdvancedDialog::setQLogArgs(const QStringList &args)
{
    ui->qlogArgsValueLabel->setText(args.join(QLatin1Char(' ')));
    ui->qlogArgsValueLabel->updateGeometry();
    layout()->activate();
    adjustSize();
}

void RigctldAdvancedDialog::autoDetectPath()
{
    FCT_IDENTIFICATION;

    const QString detectedPath = RigctldManager::findRigctldPath();

    if ( detectedPath.isEmpty() )
    {
        updateVersionLabel();
        QMessageBox::warning(this, tr("Auto Detect"),
                             tr("rigctld was not found on this system.\n"
                                "Please install Hamlib or specify the path manually."));
    }
    else
        setPath(detectedPath);
}

void RigctldAdvancedDialog::browsePath()
{
    FCT_IDENTIFICATION;

#ifdef Q_OS_WIN
    const QString filter = tr("Executable (*.exe);;All files (*.*)");
#else
    const QString filter = tr("All files (*)");
#endif
    const QString folder = (ui->pathEdit->text().isEmpty() ) ? QDir::rootPath()
                                                             : ui->pathEdit->text();

    const QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select rigctld executable"),
                                                folder,
                                                filter);
    if ( !path.isEmpty() )
        setPath(path);
}

void RigctldAdvancedDialog::updateVersionLabel()
{
    FCT_IDENTIFICATION;

    const QString path = ui->pathEdit->text().trimmed();
    const bool isAutoDetect = path.isEmpty();

    // When autodetect, resolve the path to show it to the user
    const QString resolvedPath = isAutoDetect ? RigctldManager::findRigctldPath() : path;
    const QString NOTFOUND = QString("<span style=\"color: red;\">✗ %1</span>").arg(tr("Not found"));

    if ( resolvedPath.isEmpty() )
    {
        ui->versionValueLabel->setText(NOTFOUND);
        return;
    }

    const RigctldVersion version = RigctldManager::getVersion(resolvedPath);

    if ( !version.isValid() )
    {
        ui->versionValueLabel->setText(NOTFOUND);
        return;
    }

    const QString versionStr = QString("<span style=\"color: green;\">✓ %1.%2.%3</span>")
                                                   .arg(version.major)
                                                   .arg(version.minor)
                                                   .arg(version.patch);

    if ( isAutoDetect )
        ui->versionValueLabel->setText(QString("%1 (%2)").arg(versionStr, resolvedPath));
    else
        ui->versionValueLabel->setText(versionStr);
}
