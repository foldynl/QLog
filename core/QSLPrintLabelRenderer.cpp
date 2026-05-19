#include <QPainter>
#include <QPrinter>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPageSize>

#include "QSLPrintLabelRenderer.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.qslprintlabelrenderer");

QSLPrintLabelRenderer::QSLPrintLabelRenderer()
{
    FCT_IDENTIFICATION;
}

void QSLPrintLabelRenderer::setTemplate(const LabelTemplate &tmpl)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << tmpl.name;

    labelTemplate = tmpl;
}

void QSLPrintLabelRenderer::setLabels(const QList<QSLLabelData> &inLabels)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << labels.size();

    labels = inLabels;
}

void QSLPrintLabelRenderer::setFooterLeft(const QString &text)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << text;

    footerLeft = text;
}

void QSLPrintLabelRenderer::setFooterRight(const QString &text)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << text;

    footerRight = text;
}

void QSLPrintLabelRenderer::setSkipLabels(int count)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << count;

    skipLabels = qMax(0, count);
}

void QSLPrintLabelRenderer::setPrintBorders(bool enabled)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << enabled;

    printBorders = enabled;
}

void QSLPrintLabelRenderer::setStyleOptions(const LabelStyleOptions &opts)
{
    FCT_IDENTIFICATION;

    styleOptions = opts;
}

int QSLPrintLabelRenderer::labelsPerPage() const
{
    FCT_IDENTIFICATION;

    return labelTemplate.cols * labelTemplate.rows;
}

int QSLPrintLabelRenderer::labelCount() const
{
    FCT_IDENTIFICATION;

    return labels.size();
}

int QSLPrintLabelRenderer::pageCount() const
{
    FCT_IDENTIFICATION;

    int totalSlots = labels.size() + skipLabels;
    int perPage = labelsPerPage();

    return ( perPage > 0 ) ? (totalSlots + perPage - 1) / perPage : 0;
}

qreal QSLPrintLabelRenderer::mmToUnits(const qreal mm,
                                       const QPaintDevice *device,
                                       bool yAxis) const
{
    FCT_IDENTIFICATION;

    return mm * (yAxis ? device->logicalDpiY() : device->logicalDpiX()) / 25.4; // to inch - DPI (px/inch)
}

void QSLPrintLabelRenderer::drawLabel(QPainter *painter,
                                      const QRectF &labelRect,
                                      const QSLLabelData &label)
{
    FCT_IDENTIFICATION;

    if ( !painter )
        return;

    const QPaintDevice *device = painter->device();

    if ( !device )
        return;

    if ( printBorders )
    {
        painter->save();
        painter->setPen(QPen(Qt::black, 0.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(labelRect);
        painter->restore();
    }

    qreal padH = mmToUnits(2.0, device);
    qreal padV = mmToUnits(1.5, device, true);

    QRectF contentRect = labelRect.adjusted(padH, padV, -padH, -padV);

    if ( contentRect.width() <= 0 || contentRect.height() <= 0 )
        return;

    // Fonts from style options
    QFont fontToRadio(styleOptions.sansFontFamily);
    fontToRadio.setPointSizeF(styleOptions.toRadioFontSize);

    QFont fontCallsign(styleOptions.sansFontFamily);
    fontCallsign.setPointSizeF(styleOptions.callsignFontSize);
    fontCallsign.setBold(true);

    QFont fontHeader(styleOptions.sansFontFamily);
    fontHeader.setPointSizeF(styleOptions.headerFontSize);
    fontHeader.setBold(true);

    QFont fontData(styleOptions.monoFontFamily.isEmpty()
                   ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                   : QFont(styleOptions.monoFontFamily));
    fontData.setPointSizeF(styleOptions.dataFontSize);

    QFont fontFooter(styleOptions.sansFontFamily);
    fontFooter.setPointSizeF(styleOptions.headerFontSize);

    // Metrics
    QFontMetricsF fmToRadio(fontToRadio, device);
    QFontMetricsF fmCallsign(fontCallsign, device);
    QFontMetricsF fmHeader(fontHeader, device);
    QFontMetricsF fmData(fontData, device);
    QFontMetricsF fmFooter(fontFooter, device);

    // Vertical layout calculation
    const qreal lineToRadio = fmToRadio.height();
    const qreal lineCallsign = fmCallsign.height();
    const qreal lineHeader = fmHeader.height();
    const qreal lineData = fmData.height();
    const qreal lineFooter = fmFooter.height();

    // Line 1: "To Radio" + callsign share the same vertical line
    // "To Radio" left-aligned, callsign centered
    const qreal line1Height = qMax(lineToRadio, lineCallsign);
    qreal currentY = contentRect.top();

    // --- Line 1: "To Radio" + Callsign ---
    painter->setFont(fontToRadio);
    painter->setPen(Qt::black);
    const QRectF toRadioRect(contentRect.left(), currentY,
                             contentRect.width(), line1Height);
    const QString toRadioText = styleOptions.toRadioText.isEmpty() ? "To Radio" : styleOptions.toRadioText;
    painter->drawText(toRadioRect, Qt::AlignLeft | Qt::AlignVCenter, toRadioText);

    painter->setFont(fontCallsign);
    painter->drawText(toRadioRect, Qt::AlignHCenter | Qt::AlignVCenter, label.callsign);

    currentY += line1Height;

    // --- Compute dynamic column widths ---
    // Column headers (from style options, with non-empty guard)
    const QString hdrDate = styleOptions.hdrDate.isEmpty() ? "Date" : styleOptions.hdrDate;
    const QString hdrTime = styleOptions.hdrTime.isEmpty() ? "Time" : styleOptions.hdrTime;
    const QString hdrBand = styleOptions.hdrBand.isEmpty() ? "Band" : styleOptions.hdrBand;
    const QString hdrMode = styleOptions.hdrMode.isEmpty() ? "Mode" : styleOptions.hdrMode;
    const QString hdrQsl  = styleOptions.hdrQsl.isEmpty()  ? "QSL"  : styleOptions.hdrQsl;

    // Measure column widths using worst-case representative strings
    // to ensure consistent layout across all labels
    qreal colWidthDate = qMax(fmHeader.horizontalAdvance(hdrDate),
                              fmData.horizontalAdvance("2025-07-26"));
    qreal colWidthTime = qMax(fmHeader.horizontalAdvance(hdrTime),
                              fmData.horizontalAdvance("00:00"));
    qreal colWidthBand = qMax(fmHeader.horizontalAdvance(hdrBand),
                              fmData.horizontalAdvance("160M"));
    qreal colWidthMode = qMax(fmHeader.horizontalAdvance(hdrMode),
                              fmData.horizontalAdvance("SSB"));
    qreal colWidthQsl = qMax(fmHeader.horizontalAdvance(hdrQsl),
                             fmData.horizontalAdvance("TNX"));

    const bool hasExtra = !styleOptions.extraColumnHeader.isEmpty();
    qreal colWidthExtra = 0.0;
    if ( hasExtra )
        colWidthExtra = qMax(fmHeader.horizontalAdvance(styleOptions.extraColumnHeader),
                             fmData.horizontalAdvance("XXXXXXXXXX"));

    // Add inter-column spacing (1.5mm fixed gap)
    qreal colGap = mmToUnits(1.5, device);
    const qreal totalWidth = colWidthDate + colWidthTime + colWidthBand
                             + colWidthMode + colWidthQsl
                             + (hasExtra ? colWidthExtra : 0.0)
                             + (hasExtra ? 5.0 : 4.0) * colGap;

    // Scale columns proportionally if total exceeds content width
    const qreal availWidth = contentRect.width();
    if ( totalWidth > availWidth && totalWidth > 0 )
    {
        qreal scale = availWidth / totalWidth;
        colWidthDate *= scale;
        colWidthTime *= scale;
        colWidthBand *= scale;
        colWidthMode *= scale;
        colWidthQsl *= scale;
        colWidthExtra *= scale;
        colGap *= scale;
    }

    // --- Line 2: Column headers ---
    const qreal headerRowHeight = lineHeader + mmToUnits(0.5, device, true);
    painter->setFont(fontHeader);
    qreal colX = contentRect.left();

    painter->drawText(QRectF(colX, currentY, colWidthDate, headerRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, hdrDate);
    colX += colWidthDate + colGap;

    painter->drawText(QRectF(colX, currentY, colWidthTime, headerRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, hdrTime);
    colX += colWidthTime + colGap;

    painter->drawText(QRectF(colX, currentY, colWidthBand, headerRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, hdrBand);
    colX += colWidthBand + colGap;

    painter->drawText(QRectF(colX, currentY, colWidthMode, headerRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, hdrMode);
    colX += colWidthMode + colGap;

    painter->drawText(QRectF(colX, currentY, colWidthQsl, headerRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, hdrQsl);

    if ( hasExtra )
    {
        colX += colWidthQsl + colGap;
        painter->drawText(QRectF(colX, currentY, colWidthExtra, headerRowHeight),
                          Qt::AlignLeft | Qt::AlignVCenter, styleOptions.extraColumnHeader);
    }

    currentY += headerRowHeight;

    // --- QSO data rows (up to maxQsoRows) ---
    const qreal dataRowHeight = lineData + mmToUnits(0.3, device, true);
    painter->setFont(fontData);
    const int maxRows = qMin(label.qsos.size(), styleOptions.maxQsoRows);

    for ( int i = 0; i < styleOptions.maxQsoRows; ++i )
    {
        if ( i < maxRows )
        {
            const QSLLabelData::QsoRow &row = label.qsos.at(i);
            colX = contentRect.left();

            painter->drawText(QRectF(colX, currentY, colWidthDate, dataRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter, row.date);
            colX += colWidthDate + colGap;

            painter->drawText(QRectF(colX, currentY, colWidthTime, dataRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter, row.time);
            colX += colWidthTime + colGap;

            painter->drawText(QRectF(colX, currentY, colWidthBand, dataRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter, row.band);
            colX += colWidthBand + colGap;

            painter->drawText(QRectF(colX, currentY, colWidthMode, dataRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter, row.mode);
            colX += colWidthMode + colGap;

            painter->drawText(QRectF(colX, currentY, colWidthQsl, dataRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter, row.qsl);

            if ( hasExtra )
            {
                colX += colWidthQsl + colGap;
                painter->drawText(QRectF(colX, currentY, colWidthExtra, dataRowHeight),
                                  Qt::AlignLeft | Qt::AlignVCenter, row.extra);
            }
        }
        currentY += dataRowHeight;
    }

    // --- Footer line ---
    const QRectF footerRect(contentRect.left(), contentRect.bottom() - lineFooter,
                            contentRect.width(), lineFooter);

    painter->setFont(fontFooter);

    if ( !footerLeft.isEmpty() )
        painter->drawText(footerRect, Qt::AlignLeft | Qt::AlignVCenter, footerLeft);

    if ( !footerRight.isEmpty() )
        painter->drawText(footerRect, Qt::AlignRight | Qt::AlignVCenter, footerRight);
}

void QSLPrintLabelRenderer::drawPage(QPainter *painter, int pageIndex)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << pageIndex;

    if ( !painter )
        return;

    QPaintDevice *device = painter->device();

    if ( !device )
        return;

    const int perPage = labelsPerPage();

    if ( perPage <= 0 )
        return;

    const int slotStart = pageIndex * perPage;

    for ( int row = 0; row < labelTemplate.rows; ++row )
    {
        for ( int col = 0; col < labelTemplate.cols; ++col )
        {
            const int slotIndex = slotStart + row * labelTemplate.cols + col;
            const int labelIndex = slotIndex - skipLabels;

            // Skip blank positions (for skip labels on first page)
            if ( labelIndex < 0 )
                continue;

            // Past all labels
            if ( labelIndex >= labels.size() )
                return;

            const qreal xMm = labelTemplate.leftMarginMm
                              + col * (labelTemplate.labelWidthMm + labelTemplate.hSpacingMm);
            const qreal yMm = labelTemplate.topMarginMm
                         + row * (labelTemplate.labelHeightMm + labelTemplate.vSpacingMm);

            const qreal x = mmToUnits(xMm, device);
            const qreal y = mmToUnits(yMm, device, true);
            const qreal w = mmToUnits(labelTemplate.labelWidthMm, device);
            const qreal h = mmToUnits(labelTemplate.labelHeightMm, device, true);

            const QRectF labelRect(x, y, w, h);
            drawLabel(painter, labelRect, labels.at(labelIndex));
        }
    }
}

QImage QSLPrintLabelRenderer::renderPage(int pageIndex, int dpi)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << pageIndex << dpi;

    if ( dpi <= 0 )
    {
        qCWarning(runtime) << "Invalid DPI" << dpi;
        return QImage();
    }

    if ( pageIndex < 0 || pageIndex >= pageCount() )
    {
        qCWarning(runtime) << "Invalid page index" << pageIndex;
        return QImage();
    }

    QSizeF pageSizeMm = QPageSize(labelTemplate.pageSize).size(QPageSize::Millimeter);

    if ( labelTemplate.orientation == QPageLayout::Landscape )
        pageSizeMm.transpose();

    const int widthPx = qRound(pageSizeMm.width() * dpi / 25.4);
    const int heightPx = qRound(pageSizeMm.height() * dpi / 25.4);

    QImage image(widthPx, heightPx, QImage::Format_ARGB32_Premultiplied);
    image.setDotsPerMeterX(qRound(dpi / 25.4 * 1000.0));
    image.setDotsPerMeterY(qRound(dpi / 25.4 * 1000.0));
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    drawPage(&painter, pageIndex);

    painter.end();
    return image;
}

void QSLPrintLabelRenderer::printAll(QPrinter *printer)
{
    FCT_IDENTIFICATION;

    if ( !printer )
    {
        qCWarning(runtime) << "Null printer";
        return;
    }

    const int pages = pageCount();

    if ( pages <= 0 )
    {
        qCWarning(runtime) << "No pages to print";
        return;
    }

    printer->setResolution(PRINTER_RESOLUTION);

    const QPageLayout layout(QPageSize(labelTemplate.pageSize),
                             labelTemplate.orientation,
                             QMarginsF(0, 0, 0, 0),
                             QPageLayout::Millimeter);
    printer->setPageLayout(layout);

    QPainter painter(printer);

    if ( !painter.isActive() )
    {
        qCWarning(runtime) << "Cannot begin painting on printer";
        return;
    }

    for ( int i = 0; i < pages; ++i )
    {
        if ( i > 0 )
            printer->newPage();

        drawPage(&painter, i);
    }

    painter.end();
}

QList<LabelTemplate> QSLPrintLabelRenderer::predefinedTemplates()
{
    FCT_IDENTIFICATION;

    static const QList<LabelTemplate> templates =
    {
        {"Avery 3664", QPageLayout::Portrait, QPageSize::A4,
         3, 8, 70.0, 33.8, 4.3, 0.0, 0.0, 0.0},
        {"Avery 3422", QPageLayout::Portrait, QPageSize::A4,
         3, 8, 70.0, 35.0, 8.5, 0.0, 0.0, 0.0},
        {"Avery 3474", QPageLayout::Portrait, QPageSize::A4,
         2, 12, 105.0, 24.0, 4.5, 0.0, 0.0, 0.0},
        {"Avery 5160 / 8160", QPageLayout::Portrait, QPageSize::Letter,
         3, 10, 66.7, 25.4, 12.7, 4.8, 3.2, 0.0},
        {"Avery 5163 / 8163", QPageLayout::Portrait, QPageSize::Letter,
         2, 5, 101.6, 50.8, 12.70, 4.762, 3.175, 0.0},
        {"Avery L7160", QPageLayout::Portrait, QPageSize::A4,
         3, 7, 63.5, 38.1, 15.15, 7.25, 2.54, 0.0},
        {"Avery L7161", QPageLayout::Portrait, QPageSize::A4,
         3, 5, 63.5, 46.6, 15.15, 7.25, 2.54, 0.0},
        {"Avery 5164", QPageLayout::Portrait, QPageSize::Letter,
         2, 3, 101.6, 84.67, 12.70, 4.762, 3.175, 0.0},
    };
    return templates;
}
