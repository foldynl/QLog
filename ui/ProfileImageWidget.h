#ifndef QLOG_UI_PROFILEIMAGEWIDGET_H
#define QLOG_UI_PROFILEIMAGEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QtNetwork>
#include <QPainter>
#include <QPaintEvent>

namespace Ui {
class ProfileImageWidget;
}

class AspectRatioLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AspectRatioLabel(QWidget* parent = nullptr,
                              Qt::WindowFlags f = Qt::WindowFlags());
    ~AspectRatioLabel();

public slots:
    void setPixmap(const QPixmap& pm);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateMargins();

    int pixmapWidth = 0;
    int pixmapHeight = 0;
};

class ProfileImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileImageWidget(QWidget *parent = nullptr);
    ~ProfileImageWidget();

public slots:
    void loadImageFromUrl(const QString &url);

private slots:
    void processReply(QNetworkReply* reply);

private:
    Ui::ProfileImageWidget *ui;
    AspectRatioLabel *imageLabel;
    QNetworkAccessManager *nam;
    QNetworkReply *currentReply;
    QMimeDatabase mimeDatabase;
    QMimeType mimeType;
};

#endif // QLOG_UI_PROFILEIMAGEWIDGET_H
