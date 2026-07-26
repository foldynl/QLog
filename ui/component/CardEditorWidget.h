#ifndef QLOG_UI_COMPONENT_CARDEDITORWIDGET_H
#define QLOG_UI_COMPONENT_CARDEDITORWIDGET_H

#include <QWidget>
#include <QPixmap>

#include "service/emailqsl/EmailQSLService.h"

// Interactive QSL card editor widget.
// Supports two overlay types:
//   TEXT — displays a {FIELD} placeholder; drag to reposition.
//   BOX  — filled rounded rectangle; drag to move, drag bottom-right handle to resize.
class CardEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CardEditorWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pm);
    const QPixmap &image() const { return m_image; }

    void setOverlays(const QList<EmailQSLFieldOverlay> &overlays);
    const QList<EmailQSLFieldOverlay> &overlays() const { return m_overlays; }

    void updateOverlay(int index, const EmailQSLFieldOverlay &ov);
    void setSelectedIndex(int index);
    int  selectedIndex() const { return m_selectedIndex; }

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override;

signals:
    void overlayPositionChanged(int index, int newX, int newY);
    void overlaySizeChanged(int index, int newW, int newH);   // BOX resize
    void overlaySelected(int index);

protected:
    void paintEvent(QPaintEvent *)    override;
    void mousePressEvent(QMouseEvent *)   override;
    void mouseMoveEvent(QMouseEvent *)    override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    static constexpr int RESIZE_HANDLE_PX = 10; // half-size of resize grip in widget px

    QRectF  imageRect()                           const;
    double  displayScale()                        const;
    QPointF imageToWidget(const QPointF &imgPt)   const;
    QPointF widgetToImage(const QPointF &wPt)     const;
    QRectF  overlayWidgetRect(int index)          const; // widget-coords bounding rect
    QRectF  resizeHandleRect(int index)           const; // widget-coords resize grip
    int     overlayAt(const QPointF &wPt)         const; // -1 = none
    bool    isOnResizeHandle(int index, const QPointF &wPt) const;

    QPixmap                     m_image;
    QList<EmailQSLFieldOverlay> m_overlays;
    int     m_selectedIndex     = -1;

    // Drag state
    int     m_dragIndex         = -1;
    bool    m_resizing          = false;
    QPointF m_dragStartImg;         // image-coord of press point
    QPointF m_dragAnchorImg;        // image-coord anchor (overlay x,y at press)
    QSizeF  m_dragSizeAtPress;      // overlay w,h at press (resize only)
};

#endif // QLOG_UI_COMPONENT_CARDEDITORWIDGET_H
