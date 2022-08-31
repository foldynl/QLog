#include "CWKeyer.h"
#include "CWKey.h"
#include "CWDummyKey.h"
#include "CWWinKey.h"
#include "core/debug.h"
#include "data/CWKeyProfile.h"

MODULE_IDENTIFICATION("qlog.core.cwkeyer");

CWKeyer *CWKeyer::instance()
{
    FCT_IDENTIFICATION;

    static CWKeyer instance;
    return &instance;
}

void CWKeyer::start()
{
    FCT_IDENTIFICATION;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000);

}

void CWKeyer::stopTimer()
{
    FCT_IDENTIFICATION;
    bool check = QMetaObject::invokeMethod(CWKeyer::instance(), &CWKeyer::stopTimerImplt, Qt::QueuedConnection);
    Q_ASSERT( check );
}

void CWKeyer::update()
{
    FCT_IDENTIFICATION;

    // at this moment, nothin
}

void CWKeyer::open()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &CWKeyer::openImpl, Qt::QueuedConnection);
}

void CWKeyer::openImpl()
{
    FCT_IDENTIFICATION;

    cwKeyLock.lock();
    __openCWKey();
    cwKeyLock.unlock();
}

void CWKeyer::__openCWKey()
{
    FCT_IDENTIFICATION;

    // if rig is active then close it
    __closeCWKey();

    CWKeyProfile newProfile = CWKeyProfilesManager::instance()->getCurProfile1();

    qCDebug(runtime) << "Opening profile name: " << newProfile.profileName;

    switch ( newProfile.model )
    {
    case CWKey::DUMMY_KEYER:
        cwKey = new CWWinKey2("/dev/ttyUSB0",
                              1200,
                              this);
        break;
    case CWKey::WINKEY2_KEYER:
        cwKey = new CWWinKey2(newProfile.portPath,
                              newProfile.baudrate,
                              this);
        break;
    default:
        cwKey = nullptr;
        qInfo() << "Unsupported Key Model " << newProfile.model;
    }

    if (!cwKey)
    {
        // initialization failed
        emit cwKeyerError(tr("Initialization Error"),
                             QString());
        return;
    }

    if ( ! cwKey->open() )
    {
        emit cwKeyerError(tr("Open Connection Error"),
                          cwKey->lastError());
        __closeCWKey();
        return;
    }

    //cwKey->sendText(QString("TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING TESTING")); //TODO, must be removed !!!!!!!!!!!!
    connectedRigProfile = newProfile;

    emit cwKeyConnected();
}

void CWKeyer::close()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &CWKeyer::closeImpl, Qt::QueuedConnection);
}

void CWKeyer::closeImpl()
{
    FCT_IDENTIFICATION;

    cwKeyLock.lock();
    __closeCWKey();
    cwKeyLock.unlock();
}

void CWKeyer::__closeCWKey()
{
    FCT_IDENTIFICATION;

    connectedRigProfile = CWKeyProfile();

    if ( cwKey )
    {
        cwKey->close();
        delete cwKey;
        cwKey = nullptr;
    }

    emit cwKeyDisconnected();
}

void CWKeyer::setMode(const CWKey::CWKeyModeID mode)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setModeImpl",
                              Qt::QueuedConnection,
                              Q_ARG(CWKey::CWKeyModeID, mode));
}

void CWKeyer::setModeImpl(const CWKey::CWKeyModeID mode)
{
    FCT_IDENTIFICATION;

    if ( !cwKey ) return;

    cwKeyLock.lock();

    cwKey->setMode(mode);

    cwKeyLock.unlock();
}

void CWKeyer::setSpeed(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setModeImpl",
                              Qt::QueuedConnection,
                              Q_ARG(qint16, wpm));
}

void CWKeyer::setSpeedImpl(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    if ( !cwKey ) return;

    cwKeyLock.lock();

    cwKey->setWPM(wpm);

    cwKeyLock.unlock();
}

void CWKeyer::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendTextImpl",
                              Qt::QueuedConnection,
                              Q_ARG(QString, text));
}

void CWKeyer::sendTextImpl(const QString &text)
{
    FCT_IDENTIFICATION;

    cwKeyLock.lock();

    cwKey->sendText(text);

    cwKeyLock.unlock();
}

void CWKeyer::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    if ( timer )
    {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
}

CWKeyer::CWKeyer(QObject *parent ) :
    QObject(parent),
    cwKey(nullptr),
    timer(nullptr)
{
    FCT_IDENTIFICATION;
}

CWKeyer::~CWKeyer()
{
    FCT_IDENTIFICATION;
}
