#ifndef RIGWIDGET_H
#define RIGWIDGET_H

#include <QWidget>
#include "core/Rig.h"

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
    void updateMode(VFOID, QString, QString, QString);
    void updatePWR(VFOID, double);
    void updateVFO(VFOID, QString);
    void updateXIT(VFOID, double);
    void updateRIT(VFOID, double);
    void updatePTT(VFOID, bool);
    void bandComboChanged(QString);
    void modeComboChanged(QString);
    void rigProfileComboChanged(QString);
    void refreshRigProfileCombo();
    void refreshBandCombo();
    void refreshModeCombo();
    void reloadSettings();
    void rigConnected();
    void rigDisconnected();

private:

    void resetRigInfo();
    void saveLastSeenFreq();
    double lastSeenFreq;
    bool rigOnline;

    Ui::RigWidget *ui;
};

#endif // RIGWIDGET_H
