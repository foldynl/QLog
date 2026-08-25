#ifndef QLOG_UI_RIGCTLDADVANCEDDIALOG_H
#define QLOG_UI_RIGCTLDADVANCEDDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui {
class RigctldAdvancedDialog;
}

class RigctldAdvancedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RigctldAdvancedDialog(QWidget *parent = nullptr);
    ~RigctldAdvancedDialog();

    void setPath(const QString &path);
    QString getPath() const;

    void setArgs(const QString &args);
    QString getArgs() const;
    void setQLogArgs(const QStringList &args);

private slots:
    void autoDetectPath();
    void browsePath();
    void updateVersionLabel();

private:
    Ui::RigctldAdvancedDialog *ui;
    QTimer *m_versionUpdateTimer;
};

#endif // QLOG_UI_RIGCTLDADVANCEDDIALOG_H
