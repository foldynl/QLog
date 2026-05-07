#ifndef RIG_DRIVERS_OMNIRIGRIGDRV_H
#define RIG_DRIVERS_OMNIRIGRIGDRV_H

#include <QMutex>
#include <QTimer>
#include <QMap>

#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

// Forward declaration from OmnirigV1 library (getting via #import in .cpp)
namespace OmnirigV1
{
    struct IOmniRigX;
    struct IRigX;
    struct IOmniRigXEvents;
    enum RigParamX;
    enum RigStatusX;
}

struct IConnectionPoint;
template <typename Owner, typename EventInterface> class OmniRigEventSink;

class OmnirigRigDrv : public GenericRigDrv
{
    Q_OBJECT
    friend class OmniRigEventSink<OmnirigRigDrv, OmnirigV1::IOmniRigXEvents>;

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
    virtual void setFrequency(VFOID, double) override;
    virtual void setSplit(bool) override;
    virtual void setRawMode(const QString &) override;
    virtual void setMode(const QString &, const QString &, bool) override;
    virtual void setPTT(bool newPTTState) override;
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
    void rigParamsChange(int rigID, int params);

private:
    void __rigTypeChange(int);
    void commandSleep();
    const QString getModeNormalizedText(const QString& rawMode, QString &submode);

    void checkChanges(int, bool force = false);
    bool checkFreqChange(int, bool);
    bool checkModeChange(int, bool);
    void checkPTTChange(int, bool);
    void checkVFOChange(int, bool);
    void checkRITChange(int, bool);
    void checkSplitChange(int, bool);

    double getRITFreq();
    void setRITFreq(double);
    double getXITFreq();
    void setXITFreq(double);

    void emitDisconnect();

private:

    unsigned int currFreq;
    unsigned int currTxFreq;
    QString currModeID;
    QString currVFO;
    unsigned int currRIT;
    unsigned int currXIT;
    bool currPTT;
    bool currSplitEnabled;
    bool futureSplit;

    // COM Objects
    OmnirigV1::IOmniRigX *omniInterface;   // the main OmniRigX COM object
    OmnirigV1::IRigX     *rig;

    // Event sink (IDispatch) defined in OmniRigEventSink.h
    OmniRigEventSink<OmnirigRigDrv, OmnirigV1::IOmniRigXEvents> *eventSink;
    IConnectionPoint *connPoint;
    unsigned long     connCookie;

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

#endif // RIG_DRIVERS_OMNIRIGRIGDRV_H
