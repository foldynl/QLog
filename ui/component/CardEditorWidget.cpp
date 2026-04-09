#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QCursor>

#include "CardEditorWidget.h"

CardEditorWidget::CardEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(300, 200);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void CardEditorWidget::setImage(const QPixmap &pm)
{
    m_image = pm;
    update();
}

void CardEditorWidget::setOverlays(const QList<EmailQSLFieldOverlay> &overlays)
{
    m_overlays = overlays;
    if (m_selectedIndex >= m_overlays.size())
        m_selectedIndex = -1;
    update();
}

void CardEditorWidget::updateOverlay(int index, const EmailQSLFieldOverlay &ov)
{
    if (index < 0 || index >= m_overlays.size())
        return;
    m_overlays[index] = ov;
    update();
}

void CardEditorWidget::setSelectedIndex(int index)
{
    if (m_selectedIndex == index)
        return;
    m_selectedIndex = index;
    update();
}

QSize CardEditorWidget::sizeHint() const
{
    if (m_image.isNull())
        return QSize(500, 320);
    return m_image.size().scaled(640, 420, Qt::KeepAspectRatio);
}

QSize CardEditorWidget::minimumSizeHint() const { return QSize(300, 200); }

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

QRectF CardEditorWidget::imageRect() const
{
    if (m_image.isNull())
        return QRectF(rect());
    QSizeF sz = m_image.size();
    sz.scale(size(), Qt::KeepAspectRatio);
    return QRectF(QPointF((width()  - sz.width())  / 2.0,
                          (height() - sz.height()) / 2.0), sz);
}

double CardEditorWidget::displayScale() const
{
    if (m_image.isNull() || m_image.width() == 0) return 1.0;
    return imageRect().width() / m_image.width();
}

QPointF CardEditorWidget::imageToWidget(const QPointF &ip) const
{
    const QRectF r = imageRect();
    const double sx = r.width()  / (m_image.isNull() ? 1 : m_image.width());
    const double sy = r.height() / (m_image.isNull() ? 1 : m_image.height());
    return QPointF(r.left() + ip.x() * sx, r.top() + ip.y() * sy);
}

QPointF CardEditorWidget::widgetToImage(const QPointF &wp) const
{
    const QRectF r = imageRect();
    if (r.width() == 0 || r.height() == 0) return wp;
    const double sx = (m_image.isNull() ? 1 : m_image.width())  / r.width();
    const double sy = (m_image.isNull() ? 1 : m_image.height()) / r.height();
    return QPointF((wp.x() - r.left()) * sx, (wp.y() - r.top()) * sy);
}

QRectF CardEditorWidget::overlayWidgetRect(int idx) const
{
    if (idx < 0 || idx >= m_overlays.size()) return {};
    const EmailQSLFieldOverlay &ov = m_overlays.at(idx);
    const QPointF tl = imageToWidget(QPointF(ov.x, ov.y));
    const double  sc = displayScale();

    if (ov.type == QLatin1String("BOX"))
    {
        return QRectF(tl, QSizeF(ov.width * sc, ov.height * sc));
    }

    // TEXT / LABEL: bounding box of the displayed string
    QFont f(ov.fontFamily, qMax(8, qRound(ov.fontSize * sc)));
    f.setBold(ov.bold); f.setItalic(ov.italic);
    const QFontMetrics fm(f);
    const QString lbl = (ov.type == QLatin1String("LABEL"))
                        ? ov.fieldName
                        : (QLatin1Char('{') + ov.fieldName + QLatin1Char('}'));
    return QRectF(tl.x(), tl.y() - fm.ascent(), fm.horizontalAdvance(lbl), fm.height());
}

QRectF CardEditorWidget::resizeHandleRect(int idx) const
{
    const QRectF r = overlayWidgetRect(idx);
    if (r.isNull()) return {};
    const double h = RESIZE_HANDLE_PX;
    return QRectF(r.right() - h, r.bottom() - h, h * 2, h * 2);
}

bool CardEditorWidget::isOnResizeHandle(int idx, const QPointF &wPt) const
{
    if (idx < 0 || idx >= m_overlays.size()) return false;
    if (m_overlays.at(idx).type != QLatin1String("BOX")) return false;
    return resizeHandleRect(idx).contains(wPt);
}

int CardEditorWidget::overlayAt(const QPointF &wPt) const
{
    // Iterate in reverse so top-drawn items (higher index) are hit first
    for (int i = m_overlays.size() - 1; i >= 0; --i)
    {
        const QRectF r = overlayWidgetRect(i).adjusted(-4, -4, 4, 4);
        if (r.contains(wPt)) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void CardEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Background
    p.fillRect(rect(), palette().window());

    if (m_image.isNull())
    {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No card image selected.\nClick \"Browse…\" to choose a QSL card image."));
        p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1, Qt::DashLine));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }

    // Card image
    p.drawPixmap(imageRect().toRect(), m_image);

    const double sc = displayScale();

    for (int i = 0; i < m_overlays.size(); ++i)
    {
        const EmailQSLFieldOverlay &ov = m_overlays.at(i);
        const bool sel = (i == m_selectedIndex);
        const QRectF wr = overlayWidgetRect(i);

        if (ov.type == QLatin1String("BOX"))
        {
            // --- Fill ---
            QColor fill(ov.fillColor);
            fill.setAlphaF(ov.opacity / 100.0);
            const double cr = ov.cornerRadius * sc;

            p.setPen(QPen(QColor(ov.color), sel ? 2.0 : 1.5));
            p.setBrush(fill);
            p.drawRoundedRect(wr, cr, cr);

            // --- Caption above box ---
            if (!ov.fieldName.isEmpty())
            {
                const int dispSz = qMax(8, qRound((ov.fontSize > 0 ? ov.fontSize : 11) * sc));
                QFont f(ov.fontFamily, dispSz);
                f.setBold(ov.bold);
                p.setFont(f);
                p.setPen(QColor(ov.color));
                p.setBrush(Qt::NoBrush);
                QFontMetrics fm(f);
                p.drawText(QPointF(wr.left(), wr.top() - fm.descent() - 2), ov.fieldName);
            }

            // --- Selection decoration ---
            if (sel)
            {
                p.save();
                p.setPen(QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(wr.adjusted(-3, -3, 3, 3), cr + 3, cr + 3);

                // Resize handle (bottom-right corner)
                const QRectF rh = resizeHandleRect(i);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 120, 215));
                p.drawEllipse(rh.center(), RESIZE_HANDLE_PX * 0.5, RESIZE_HANDLE_PX * 0.5);

                // Size info label
                p.setPen(QColor(0, 80, 180));
                p.setFont(QFont(QStringLiteral("Arial"), 8));
                p.drawText(QPointF(wr.left() + 3, wr.bottom() - 3),
                           QString("%1×%2").arg(ov.width).arg(ov.height));
                p.restore();
            }
        }
        else // TEXT or LABEL
        {
            const int dispSz = qMax(8, qRound(ov.fontSize * sc));
            QFont f(ov.fontFamily, dispSz);
            f.setBold(ov.bold); f.setItalic(ov.italic);
            p.setFont(f);

            // LABEL shows literal text; TEXT shows {FIELDNAME} placeholder
            const QString lbl = (ov.type == QLatin1String("LABEL"))
                                 ? ov.fieldName
                                 : (QLatin1Char('{') + ov.fieldName + QLatin1Char('}'));
            const QFontMetrics fm(f);
            const QPointF anchor(wr.left(), wr.top() + fm.ascent());

            // Selection box
            if (sel)
            {
                p.save();
                p.setPen(QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
                p.setBrush(QColor(0, 120, 215, 25));
                p.drawRect(wr.adjusted(-3, -3, 3, 3));
                // Drag-handle dot
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 120, 215));
                p.drawEllipse(QPointF(wr.left() - 7, wr.center().y()), 4, 4);
                p.restore();
            }
            else
            {
                p.save();
                p.setPen(QPen(QColor(0, 0, 0, 35), 1, Qt::DotLine));
                p.setBrush(Qt::NoBrush);
                p.drawRect(wr.adjusted(-2, -2, 2, 2));
                p.restore();
            }

            // Shadow
            {
                QColor shadow = QColor(ov.color).lightness() > 128
                                ? QColor(0, 0, 0, 90) : QColor(255, 255, 255, 90);
                p.save();
                p.setPen(shadow);
                p.drawText(anchor + QPointF(1, 1), lbl);
                p.restore();
            }

            p.setPen(QColor(ov.color));
            p.drawText(anchor, lbl);
        }
    }

    // Border
    p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void CardEditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }

    const QPointF wPos = event->pos();
    const int idx = overlayAt(wPos);

    m_selectedIndex = idx;
    m_dragIndex     = -1;
    m_resizing      = false;

    if (idx >= 0)
    {
        m_dragIndex     = idx;
        m_dragStartImg  = widgetToImage(wPos);
        m_dragAnchorImg = QPointF(m_overlays.at(idx).x, m_overlays.at(idx).y);

        if (isOnResizeHandle(idx, wPos))
        {
            m_resizing = true;
            m_dragSizeAtPress = QSizeF(m_overlays.at(idx).width, m_overlays.at(idx).height);
            setCursor(Qt::SizeFDiagCursor);
        }
        else
        {
            setCursor(Qt::ClosedHandCursor);
        }
    }

    emit overlaySelected(m_selectedIndex);
    update();
}

void CardEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF wPos = event->pos();

    if (m_dragIndex >= 0 && (event->buttons() & Qt::LeftButton))
    {
        const QPointF curImg = widgetToImage(wPos);
        const QPointF delta  = curImg - m_dragStartImg;

        EmailQSLFieldOverlay &ov = m_overlays[m_dragIndex];
        const int imgW = m_image.isNull() ? 99999 : m_image.width();
        const int imgH = m_image.isNull() ? 99999 : m_image.height();

        if (m_resizing)
        {
            const int newW = qMax(20, qRound(m_dragSizeAtPress.width()  + delta.x()));
            const int newH = qMax(10, qRound(m_dragSizeAtPress.height() + delta.y()));
            ov.width  = qMin(newW, imgW - ov.x);
            ov.height = qMin(newH, imgH - ov.y);
        }
        else
        {
            ov.x = qBound(0, qRound(m_dragAnchorImg.x() + delta.x()), imgW - 1);
            ov.y = qBound(0, qRound(m_dragAnchorImg.y() + delta.y()), imgH - 1);
        }
        update();
    }
    else
    {
        // Cursor feedback
        const int hov = overlayAt(wPos);
        if (hov >= 0)
        {
            if (isOnResizeHandle(hov, wPos))
                setCursor(Qt::SizeFDiagCursor);
            else
                setCursor(Qt::OpenHandCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void CardEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragIndex >= 0)
    {
        const EmailQSLFieldOverlay &ov = m_overlays.at(m_dragIndex);
        if (m_resizing)
            emit overlaySizeChanged(m_dragIndex, ov.width, ov.height);
        else
            emit overlayPositionChanged(m_dragIndex, ov.x, ov.y);

        m_dragIndex = -1;
        m_resizing  = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
}
