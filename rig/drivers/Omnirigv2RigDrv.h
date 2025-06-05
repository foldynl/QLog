#ifndef OMNIRIGV2RIGRIGDRV_H
#define OMNIRIGV2RIGRIGDRV_H

#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

// OmniRig.h is generated automatically by dumpcpp
// omnirig must be installed before compilation
// Currently supported is OmniRig v2
#include "Omnirig2.h"

//don't inherit Omnirigv1, it won't do any good because it's a different namespace
class OmnirigV2RigDrv : public GenericRigDrv
{
    Q_OBJECT

public:

    static QList<QPair<int, QString>> getModelList();
    static RigCaps getCaps(int);

    explicit OmnirigV2RigDrv(const RigProfile &profile,
                          QObject *parent = nullptr);

    virtual ~OmnirigV2RigDrv();

    virtual bool open() override;
    virtual bool isMorseOverCatSupported() override;
    virtual QStringList getAvailableModes() override;

    virtual void setFrequency(double) override;
    virtual void setRawMode(const QString &) override;
    virtual void setMode(const QString &, const QString &, bool digiVariant) override;
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
    OmniRigV2::IRigX *getRigPtr();

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

    OmniRigV2::OmniRigX *omniRigInterface;
    OmniRigV2::RigX *rig;
    int readableParams;
    int writableParams;
    QMutex drvLock;
    const QMap<OmniRigV2::RigParamX, QString> modeMap = {
                                      {OmniRigV2::PM_CW_U, "CWR"},
                                      {OmniRigV2::PM_CW_L, "CW"},
                                      {OmniRigV2::PM_SSB_U, "USB"},
                                      {OmniRigV2::PM_SSB_L, "LSB"},
                                      {OmniRigV2::PM_DIG_U, "DIG_U"},
                                      {OmniRigV2::PM_DIG_L, "DIG_L"},
                                      {OmniRigV2::PM_AM, "AM"},
                                      {OmniRigV2::PM_FM, "FM"}
                                     };
};
#endif // OMNIRIGV2RIGRIGDRV_H
