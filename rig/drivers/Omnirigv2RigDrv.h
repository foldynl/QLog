#ifndef OMNIRIGV2RIGRIGDRV_H
#define OMNIRIGV2RIGRIGDRV_H

#include <QMutex>
#include <QTimer>
#include <QMap>

#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

// Forward declaration from OmnirigV2 library (getting via #import in .cpp)
namespace OmnirigV2
{
    struct IOmniRigX;
    struct IRigX;
    struct IOmniRigXEvents;
    enum RigParamX;
    enum RigStatusX;
}

struct IConnectionPoint;
template <typename Owner, typename EventInterface> class OmniRigEventSink;

class OmnirigV2RigDrv : public GenericRigDrv
{
    Q_OBJECT
    friend class OmniRigEventSink<OmnirigV2RigDrv, OmnirigV2::IOmniRigXEvents>;

public:
    static QList<QPair<int, QString>> getModelList();
    static RigCaps getCaps(int);

    explicit OmnirigV2RigDrv(const RigProfile &profile,
                             QObject *parent = nullptr);

    ~OmnirigV2RigDrv() override;

    bool        open() override;
    bool        isMorseOverCatSupported() override;
    QStringList getAvailableModes() override;

    void setFrequency(double) override;
    void setFrequency(VFOID, double) override;
    void setSplit(bool) override;
    void setRawMode(const QString &) override;
    void setMode(const QString &, const QString &, bool digiVariant) override;
    void setPTT(bool) override;
    void setKeySpeed(qint16 wpm) override;
    void syncKeySpeed(qint16 wpm) override;
    void sendMorse(const QString &) override;
    void stopMorse() override;
    void sendState() override;
    void stopTimers() override;
    void sendDXSpot(const DxSpot &spot) override;

public slots:
    void rigTypeChange(int);
    void rigStatusChange(int);
    void rigParamsChange(int rigID, int params);

private:
    void __rigTypeChange(int);
    void commandSleep();
    const QString getModeNormalizedText(const QString &rawMode, QString &submode);

    void checkChanges(int params, bool force = false);
    bool checkFreqChange(int params, bool force);
    bool checkModeChange(int params, bool force);
    void checkPTTChange(int params, bool force);
    void checkVFOChange(int params, bool force);
    void checkRITChange(int params, bool force);
    void checkSplitChange(int params, bool force);

    double getRITFreq();
    void   setRITFreq(double);
    double getXITFreq();
    void   setXITFreq(double);

    void emitDisconnect();

private:
    unsigned int currFreq;
    unsigned int currTxFreq;
    unsigned int currRIT;
    unsigned int currXIT;
    bool         currPTT;
    bool         currSplitEnabled;
    QString      currModeID;
    QString      currVFO;

    // COM Objects
    OmnirigV2::IOmniRigX *omniInterface;   // the main OmniRigX COM object
    OmnirigV2::IRigX     *rig;

    // Event sink (IDispatch) defined in OmniRigEventSink.h
    OmniRigEventSink<OmnirigV2RigDrv, OmnirigV2::IOmniRigXEvents> *eventSink;
    IConnectionPoint *connPoint;
    unsigned long     connCookie;

    // Parameters from OmniRig (bitmap)
    int readableParams;
    int writableParams;

    QMutex drvLock;
    QTimer offlineTimer;

    const int FREQMASK;
    const int VFO_A_MASK;
    const int VFO_B_MASK;
    const int VFO_SPEC_MASK;
    const int ALLVFOsMASK;
    const int SPLIT_MASK;

    static const uint OFFLINETIMER_TIME_MS = 10000;

    // Mode maps
    QMap<int, QString> modeMap;
};

#endif // OMNIRIGV2RIGRIGDRV_H
