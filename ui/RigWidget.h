#ifndef QLOG_UI_RIGWIDGET_H
#define QLOG_UI_RIGWIDGET_H

#include <QWidget>
#include "rig/Rig.h"
#include "service/hrdlog/HRDLog.h"

namespace Ui {
class RigWidget;
}

class RigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RigWidget(QWidget *parent = nullptr);
    ~RigWidget();

signals:
    void rigProfileChanged();

public slots:
    void updateFrequency(VFOID, double, double, double);
    void updateMode(VFOID, const QString&, const QString&,
                    const QString&, qint32);
    void updatePWR(VFOID, double);
    void updateVFO(VFOID, const QString&);
    void updateXIT(VFOID, double);
    void updateRIT(VFOID, double);
    void updatePTT(VFOID, bool);
    void bandComboChanged(const QString&);
    void modeComboChanged(const QString&);
    void rigProfileComboChanged(const QString&);
    void refreshRigProfileCombo();
    void refreshBandCombo();
    void refreshModeCombo();
    void reloadSettings();
    void rigConnected();
    void rigDisconnected();
    void bandUp();
    void bandDown();
    void setBand(const QString &band);

private slots:
    void sendOnAirState();

private:

    void resetRigInfo();
    void saveLastSeenFreq();
    double lastSeenFreq;
    QString lastSeenMode;
    bool rigOnline;

    Ui::RigWidget *ui;
    HRDLogUploader *hrdlog;
};

#endif // QLOG_UI_RIGWIDGET_H
