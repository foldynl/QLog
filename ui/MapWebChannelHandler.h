#ifndef QLOG_UI_MAPWEBCHANNELHANDLER_H
#define QLOG_UI_MAPWEBCHANNELHANDLER_H

#include <QObject>
#include <QWebEnginePage>

class MapWebChannelHandler : public QObject
{
    Q_OBJECT
public:
    explicit MapWebChannelHandler(const QString &configID,
                                    QObject *parent = nullptr);
    void restoreLayerControlStates(QWebEnginePage *page);
    QString generateMapMenuJS(bool gridLayer = true,
                              bool grayline = false,
                              bool aurora = false,
                              bool muf = false,
                              bool ibp = false,
                              bool antpath = false,
                              bool chatStations = false,
                              bool wsjtxStations = false,
                              bool paths = false);

signals:
    void chatCallsignPressed(QString);
    void wsjtxCallsignPressed(QString);
    void IBPPressed(QString, double);

public slots:
    void handleLayerSelectionChanged(const QVariant &data,
                                     const QVariant &state);
    void chatCallsignClicked(const QVariant &data);
    void wsjtxCallsignClicked(const QVariant &data);
    void IBPCallsignClicked(const QVariant &callsign,
                            const QVariant &freq);
private:
    QString configID;

    void connectWebChannel(QWebEnginePage *page);
};

#endif // QLOG_UI_MAPWEBCHANNELHANDLER_H
