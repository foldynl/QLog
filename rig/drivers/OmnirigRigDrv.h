#ifndef RIG_DRIVERS_OMNIRIGRIGDRV_H
#define RIG_DRIVERS_OMNIRIGRIGDRV_H

#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

// OmniRig.h is generated automatically by dumpcpp
// omnirig must be installed before compilation
// Currently supported is OmniRig v1
#include "Omnirig.h"

class OmnirigRigDrv : public GenericRigDrv
{
    Q_OBJECT

public:
    static QList<QPair<int, QString>> getModelList();
    static RigCaps getCaps(int model);
    explicit OmnirigRigDrv(const RigProfile &profile,
                        QObject *parent = nullptr);
    virtual ~OmnirigRigDrv();

    virtual bool open() override;
    virtual bool isMorseOverCatSupported() override;
    virtual QStringList getAvailableModes() override;

    virtual void setFrequency(double) override;
    virtual void setRawMode(const QString &) override;
    virtual void setMode(const QString &, const QString &, bool) override;
    virtual void setPTT(bool) override;
    virtual void setKeySpeed(qint16 wpm) override;
    virtual void syncKeySpeed(qint16 wpm) override;
    virtual void sendMorse(const QString &) override;
    virtual void stopMorse() override;
    virtual void sendState() override;
    virtual void stopTimers() override;
    virtual void sendDXSpot(const DxSpot &spot) override;

private slots:
    void rigTypeChange(int);
    void rigStatusChange(int);
    void COMException (int,  QString, QString, QString);
    void rigParamsChange(int rigID, int params);

private:
    OmniRig::IRigX* getRigPtr();

    void __rigTypeChange(int);
    void commandSleep();
    const QString getModeNormalizedText(const QString& rawMode, QString &submode);
    void checkChanges(int, bool force = false);
    bool checkFreqChange(int, bool);
    bool checkModeChange(int, bool);
    void checkPTTChange(int, bool);
    void checkVFOChange(int, bool);
    void checkRITChange(int, bool);
    //void checkXITChange(int, bool); XitOffset is not implemented in Omnirig library now

    double getRITFreq();
    void setRITFreq(double);
    double getXITFreq();
    void setXITFreq(double);

    unsigned int currFreq;
    QString currModeID;
    QString currVFO;
    unsigned int currRIT;
    unsigned int currXIT;
    bool currPTT;

    OmniRig::OmniRigX *omniRigInterface;
    OmniRig::RigX *rig;
    int readableParams;
    int writableParams;
    QMutex drvLock;
    const QMap<OmniRig::RigParamX, QString> modeMap = {
                                      {OmniRig::PM_CW_U, "CWR"},
                                      {OmniRig::PM_CW_L, "CW"},
                                      {OmniRig::PM_SSB_U, "USB"},
                                      {OmniRig::PM_SSB_L, "LSB"},
                                      {OmniRig::PM_DIG_U, "DIG_U"},
                                      {OmniRig::PM_DIG_L, "DIG_L"},
                                      {OmniRig::PM_AM, "AM"},
                                      {OmniRig::PM_FM, "FM"}
                                     };
};

#endif // RIG_DRIVERS_OMNIRIGRIGDRV_H
