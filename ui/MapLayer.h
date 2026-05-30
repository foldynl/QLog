#ifndef QLOG_UI_MAPLAYER_H
#define QLOG_UI_MAPLAYER_H

#include <QFlags>

class MapLayer
{
public:
    enum Layer
    {
        Grid = 0x0001,
        Grayline = 0x0002,
        Aurora = 0x0004,
        Muf = 0x0008,
        Ibp = 0x0010,
        Beam = 0x0020,
        Chat = 0x0040,
        Wsjtx = 0x0080,
        Path = 0x0100
    };
    Q_DECLARE_FLAGS(Layers, Layer)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MapLayer::Layers)

#endif // QLOG_UI_MAPLAYER_H
