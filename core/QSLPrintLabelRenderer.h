#ifndef QLOG_CORE_QSLPRINTLABELRENDERER_H
#define QLOG_CORE_QSLPRINTLABELRENDERER_H

#include <QString>
#include <QList>
#include <QRectF>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>

class QPainter;
class QPrinter;
class QPaintDevice;

struct LabelTemplate
{
    QString name;
    QPageLayout::Orientation orientation;
    QPageSize::PageSizeId pageSize;
    int cols;
    int rows;
    double labelWidthMm;
    double labelHeightMm;
    double topMarginMm;
    double leftMarginMm;
    double hSpacingMm;
    double vSpacingMm;
};

struct LabelStyleOptions
{
    QString sansFontFamily;          // empty = system default
    QString monoFontFamily;          // empty = system fixed font
    qreal toRadioFontSize = 7.5;
    qreal callsignFontSize = 14.0;
    qreal headerFontSize = 7.0;
    qreal dataFontSize = 8.0;
    QString extraColumnHeader;       // empty = no extra column
    int maxQsoRows = 4;              // 1-4 QSO data rows per label
    QString toRadioText = "To Radio";
    QString hdrDate = "Date";
    QString hdrTime = "Time";
    QString hdrBand = "Band";
    QString hdrMode = "Mode";
    QString hdrQsl  = "QSL";
};

struct QSLLabelData
{
    QString callsign;

    struct QsoRow
    {
        QString date;   // formatted by dialog according to user date format
        QString time;   // "09:38"
        QString band;   // "20M"
        QString mode;   // "SSB"
        QString qsl;    // "PSE" or "TNX"
        QString extra;  // raw DB value for extra column, empty if unused
    };

    QList<QsoRow> qsos;
};

class QSLPrintLabelRenderer
{
public:
    QSLPrintLabelRenderer();

    void setTemplate(const LabelTemplate &tmpl);
    void setLabels(const QList<QSLLabelData> &inLabels);
    void setFooterLeft(const QString &text);
    void setFooterRight(const QString &text);
    void setSkipLabels(int count);
    void setPrintBorders(bool enabled);
    void setStyleOptions(const LabelStyleOptions &opts);

    int pageCount() const;
    int labelCount() const;
    QImage renderPage(int pageIndex, int dpi = 150);
    void printAll(QPrinter *printer);

    static QList<LabelTemplate> predefinedTemplates();

private:
    void drawLabel(QPainter *painter, const QRectF &labelRect,
                   const QSLLabelData &label);
    void drawPage(QPainter *painter, int pageIndex);
    qreal mmToUnits(const qreal mm, const QPaintDevice *device, bool yAxis = false) const;
    int labelsPerPage() const;

    LabelTemplate labelTemplate;
    QList<QSLLabelData> labels;
    QString footerLeft;
    QString footerRight;
    int skipLabels = 0;
    bool printBorders = false;
    LabelStyleOptions styleOptions;

    const int PRINTER_RESOLUTION = 300;
};

#endif // QLOG_CORE_QSLPRINTLABELRENDERER_H
