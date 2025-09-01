// RecomputeDxccDialog.h
#pragma once
#include <QDialog>
#include <QSqlDatabase>
#include <QDateTime>
#include <QCheckBox>

class QProgressBar;
class QLabel;
class QTextEdit;
class QPushButton;
class QThread;

class RecomputeDxccDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RecomputeDxccDialog(QWidget *parent = nullptr);
    ~RecomputeDxccDialog();

private slots:
    void onCancel();
    void onStart();
    void runJob();

private:
    void logLine(const QString &line);

    QProgressBar *m_bar;
    QLabel *m_stats;
    QTextEdit *m_log;
    QPushButton *m_cancelBtn;
    QPushButton *m_startBtn;
    QCheckBox* onlyMissingCheck_ = nullptr;
    bool stopRequested;
    std::atomic<bool> m_cancel{false};
};
