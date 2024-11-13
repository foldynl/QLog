#include <QFile>
#include <QSettings>

#include "MapWebChannelHandler.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.maplayercontrolhandler");

MapWebChannelHandler::MapWebChannelHandler(const QString &configID,
                                               QObject *parent)
    : QObject(parent),
      configID(configID)
{
}

void MapWebChannelHandler::connectWebChannel(QWebEnginePage *page)
{
    FCT_IDENTIFICATION;

    QFile file(":/qtwebchannel/qwebchannel.js");

    if (!file.open(QIODevice::ReadOnly))
    {
        qCInfo(runtime) << "Cannot read qwebchannel.js";
        return;
    }

    QTextStream stream(&file);
    QString js;

    js.append(stream.readAll());
    js += " var webChannel = new QWebChannel(qt.webChannelTransport, function(channel) "
          "{ window.foo = channel.objects.layerControlHandler; });"
          " map.on('overlayadd', function(e){ "
          "  switch (e.name) "
          "  { "
          "     case '" + tr("Grid") + "': "
          "        foo.handleLayerSelectionChanged('maidenheadConfWorked', 'on'); "
          "        break; "
          "     case '" + tr("Gray-Line") + "': "
          "        foo.handleLayerSelectionChanged('grayline', 'on'); "
          "        break; "
          "     case '" + tr("Beam") + "': "
          "        foo.handleLayerSelectionChanged('antPathLayer', 'on'); "
          "        break; "
          "     case '" + tr("Aurora") + "': "
          "        foo.handleLayerSelectionChanged('auroraLayer', 'on'); "
          "        break; "
          "     case '" + tr("MUF") + "': "
          "        foo.handleLayerSelectionChanged('mufLayer', 'on'); "
          "        break; "
          "     case '" + tr("IBP") + "': "
          "        foo.handleLayerSelectionChanged('IBPLayer', 'on'); "
          "        break; "
          "     case '" + tr("Chat") + "': "
          "        foo.handleLayerSelectionChanged('chatStationsLayer', 'on'); "
          "        break; "
          "     case '" + tr("WSJTX - CQ") + "': "
          "        foo.handleLayerSelectionChanged('wsjtxStationsLayer', 'on'); "
          "        break; "
          "     case '" + tr("Paths") + "': "
          "        foo.handleLayerSelectionChanged('pathLayer', 'on'); "
          "        break; "
          "  } "
          "});"
          "map.on('overlayremove', function(e){ "
          "   switch (e.name) "
          "   { "
          "      case '" + tr("Grid") + "': "
          "         foo.handleLayerSelectionChanged('maidenheadConfWorked', 'off'); "
          "         break; "
          "      case '" + tr("Gray-Line") + "': "
          "         foo.handleLayerSelectionChanged('grayline', 'off'); "
          "         break; "
          "     case '" + tr("Beam") + "': "
          "        foo.handleLayerSelectionChanged('antPathLayer', 'off'); "
          "        break; "
          "     case '" + tr("Aurora") + "': "
          "        foo.handleLayerSelectionChanged('auroraLayer', 'off'); "
          "        break; "
          "     case '" + tr("MUF") + "': "
          "        foo.handleLayerSelectionChanged('mufLayer', 'off'); "
          "        break; "
          "     case '" + tr("IBP") + "': "
          "        foo.handleLayerSelectionChanged('IBPLayer', 'off'); "
          "        break; "
          "     case '" + tr("Chat") + "': "
          "        foo.handleLayerSelectionChanged('chatStationsLayer', 'off'); "
          "        break; "
          "     case '" + tr("WSJTX - CQ") + "': "
          "        foo.handleLayerSelectionChanged('wsjtxStationsLayer', 'off'); "
          "        break; "
          "     case '" + tr("Paths") + "': "
          "        foo.handleLayerSelectionChanged('pathLayer', 'off'); "
          "        break; "
          "   } "
          "});";
    page->runJavaScript(js);
}

void MapWebChannelHandler::restoreLayerControlStates(QWebEnginePage *page)
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QString js;

    settings.beginGroup(QString("%1/layerstate").arg(configID));

    const QStringList &keys = settings.allKeys();

    for ( const QString &key : keys)
    {
        qCDebug(runtime) << "key:" << key << "value:" << settings.value(key);

        js += ( settings.value(key).toBool() ) ? QString("map.addLayer(%1);").arg(key)
                                               : QString("map.removeLayer(%1);").arg(key);
    }
    qCDebug(runtime) << js;

    page->runJavaScript(js);

    connectWebChannel(page);
}

QString MapWebChannelHandler::generateMapMenuJS(bool gridLayer,
                                                bool grayline,
                                                bool aurora,
                                                bool muf,
                                                bool ibp,
                                                bool antpath,
                                                bool chatStations,
                                                bool wsjtxStations,
                                                bool paths)
{
    FCT_IDENTIFICATION;
    QStringList options;

    if ( aurora )
        options << "\"" + tr("Aurora") + "\": auroraLayer";

    if ( antpath )
        options << "\"" + tr("Beam") + "\": antPathLayer";

    if ( chatStations )
        options << "\"" + tr("Chat") + "\": chatStationsLayer";

    if ( gridLayer )
        options << "\"" + tr("Grid") + "\": maidenheadConfWorked";

    if ( grayline )
        options << "\"" + tr("Gray-Line") + "\": grayline";

    if ( ibp )
        options << "\"" + tr("IBP") + "\": IBPLayer";

    if ( muf )
        options << "\"" + tr("MUF") + "\": mufLayer";

    if ( wsjtxStations )
        options << "\"" + tr("WSJTX - CQ") + "\": wsjtxStationsLayer";

    if ( paths )
        options << "\"" + tr("Paths") + "\": pathLayer";

    QString ret = QString("var layerControl = new L.Control.Layers(null,"
                          "{ %1 },{}).addTo(map);").arg(options.join(","));

    qCDebug(runtime) << ret;

    return ret;
}

void MapWebChannelHandler::handleLayerSelectionChanged(const QVariant &data, const QVariant &state)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data << state;

    QSettings settings;

    settings.setValue(QString("%1/layerstate/%2").arg(configID,data.toString()),
                      (state.toString().toLower() == "on") ? true : false);
}

void MapWebChannelHandler::chatCallsignClicked(const QVariant &data)
{
    FCT_IDENTIFICATION;

    emit chatCallsignPressed(data.toString());
}

void MapWebChannelHandler::wsjtxCallsignClicked(const QVariant &data)
{
    FCT_IDENTIFICATION;

    emit wsjtxCallsignPressed(data.toString());
}

void MapWebChannelHandler::IBPCallsignClicked(const QVariant &callsign, const QVariant &freq)
{
    FCT_IDENTIFICATION;

    emit IBPPressed(callsign.toString(), freq.toDouble());
}
