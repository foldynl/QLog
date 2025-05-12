#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QMenu>
#include <QMouseEvent>
#include "rotator/Rotator.h"
#include "rotator/Rotator.h"
#include "RotatorWidget.h"
#include "ui_RotatorWidget.h"
#include "core/debug.h"
#include "core/Gridsquare.h"
#include "data/StationProfile.h"
#include "data/RotUsrButtonsProfile.h"

MODULE_IDENTIFICATION("qlog.ui.rotatorwidget");

RotatorWidget::RotatorWidget(QWidget *parent) :
    QWidget(parent),
    antennaNeedle(nullptr),
    requestedAzimuthNeedle(nullptr),
    QSOAzimuthNeedle(nullptr),
    waitingFirstValue(true),
    compassScene(nullptr),
    ui(new Ui::RotatorWidget),
    antennaAzimuth(0.0),
    requestedAzimuth(qQNaN()),
    qsoAzimuth(qQNaN()),
    contact(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    redrawMap();

    QStringListModel* rotModel = new QStringListModel(this);
    ui->rotProfileCombo->setModel(rotModel);
    ui->rotProfileCombo->setStyleSheet("QComboBox {color: red}");
    refreshRotProfileCombo();

    QStringListModel* userButtonModel = new QStringListModel(this);
    ui->userButtonsProfileCombo->setModel(userButtonModel);
    refreshRotUserButtonProfileCombo();

    QMenu *qsoBearingMenu = new QMenu(this);
    qsoBearingMenu->addAction(ui->actionQSO_SP);
    qsoBearingMenu->addAction(ui->actionQSO_LP);

    ui->qsoBearingButton->setMenu(qsoBearingMenu);
    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_SP);

    rotDisconnected();
}

void RotatorWidget::gotoPosition()
{
    FCT_IDENTIFICATION;

    setBearing(ui->gotoDoubleSpinBox->value());
}

double RotatorWidget::getQSOBearing()
{
    FCT_IDENTIFICATION;

    double qsoBearing = (contact) ? contact->getQSOBearing()
                                  : qQNaN();

    qCDebug(runtime) << "QSO Bearing:" << qsoBearing;

    return qsoBearing;
}

void RotatorWidget::qsoBearingLP()
{
    FCT_IDENTIFICATION;

    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_LP);

    double qsoBearing = getQSOBearing();

    if ( qIsNaN(qsoBearing) )
        return;

    qsoBearing -= 180;

    if ( qsoBearing < 0 )
        qsoBearing += 360;

    setBearing(qsoBearing);
}

void RotatorWidget::qsoBearingSP()
{
    FCT_IDENTIFICATION;

    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_SP);

    setBearing(getQSOBearing());
}

void RotatorWidget::setRequestedAz(double requestedAz)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << requestedAz;

    if ( qIsNaN(requestedAz) )
        return;

    requestedAzimuth = requestedAz;
    if ( requestedAzimuthNeedle )
    {
        requestedAzimuthNeedle->show();
        requestedAzimuthNeedle->setRotation(requestedAz);
    }
}

void RotatorWidget::setBearing(double in_azimuth)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth;

    if ( qIsNaN(in_azimuth) || !ui->gotoDoubleSpinBox->isEnabled() )
        return;

    setRequestedAz(in_azimuth);
    Rotator::instance()->setPosition(in_azimuth, 0);
}

void RotatorWidget::positionChanged(double in_azimuth, double in_elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth <<in_elevation;

    antennaAzimuth = (in_azimuth < 0.0 ) ? 360.0 + in_azimuth : in_azimuth;
    if ( antennaNeedle) antennaNeedle->setRotation(antennaAzimuth);
    if ( waitingFirstValue )
    {
        waitingFirstValue = false;
        if ( requestedAzimuthNeedle) requestedAzimuthNeedle->setRotation(in_azimuth);
    }
    ui->gotoDoubleSpinBox->blockSignals(true);
    ui->gotoDoubleSpinBox->setValue(antennaAzimuth);
    ui->gotoDoubleSpinBox->blockSignals(false);
    if ( qAbs(qRound(requestedAzimuth) - qRound(in_azimuth)) <= AZIMUTH_DEAD_BAND
         && requestedAzimuthNeedle )
        requestedAzimuthNeedle->hide();
}

void RotatorWidget::setQSOBearing(double , double )
{
    FCT_IDENTIFICATION;

    if ( !QSOAzimuthNeedle )
        return;

    qsoAzimuth = getQSOBearing();

    qCDebug(runtime) << qsoAzimuth;

    if ( !qIsNaN(qsoAzimuth) )
    {
        QSOAzimuthNeedle->show();
        QSOAzimuthNeedle->setRotation(qsoAzimuth);
    }
    else
        QSOAzimuthNeedle->hide();
}


void RotatorWidget::showEvent(QShowEvent* event)
{
    FCT_IDENTIFICATION;

    ui->compassView->fitInView(compassScene->sceneRect(), Qt::KeepAspectRatio);
    QWidget::showEvent(event);
}

void RotatorWidget::resizeEvent(QResizeEvent* event)
{
    FCT_IDENTIFICATION;

    ui->compassView->fitInView(compassScene->sceneRect(), Qt::KeepAspectRatio);
    QWidget::resizeEvent(event);
}

void RotatorWidget::mousePressEvent(QMouseEvent *event)
{
    FCT_IDENTIFICATION;

    if( event->button() == Qt::LeftButton )
    {
        QPointF clickPos = ui->compassView->mapToScene(ui->compassView->mapFromGlobal(
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                                                                                      event->globalPosition().toPoint()
#else
                                                                                      event->globalPos()
#endif
                                                                                     ));

        qreal dx = clickPos.x();
        qreal dy = -1 * clickPos.y();

        if ( qSqrt(qPow(dx, 2) + qPow(dy, 2)) <= GLOBE_RADIUS ) // distance between click and center of the globe
        {
            double angle = qRadiansToDegrees(qAtan2(dx, dy));

            if ( angle < 0 )
                angle += 360;

            setBearing(std::round(angle));
        }
    }

    QWidget::mousePressEvent(event);
}

void RotatorWidget::userButton1()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[0];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton2()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[1];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton3()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[2];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton4()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[3];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::refreshRotProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->rotProfileCombo->blockSignals(true);

    QStringList currProfiles = RotProfilesManager::instance()->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->rotProfileCombo->model());

    model->setStringList(currProfiles);

    if ( RotProfilesManager::instance()->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->rotProfileCombo->setCurrentText(currProfiles.first());
        rotProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->rotProfileCombo->setCurrentText(RotProfilesManager::instance()->getCurProfile1().profileName);
    }

    ui->rotProfileCombo->blockSignals(false);
}

void RotatorWidget::refreshRotUserButtonProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->userButtonsProfileCombo->blockSignals(true);

    RotUsrButtonsProfilesManager *buttonManager =  RotUsrButtonsProfilesManager::instance();

    QStringList currProfiles = buttonManager->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->userButtonsProfileCombo->model());

    model->setStringList(currProfiles);

    if ( buttonManager->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->userButtonsProfileCombo->setCurrentText(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->userButtonsProfileCombo->setCurrentText(buttonManager->getCurProfile1().profileName);
    }

    rotUserButtonProfileComboChanged(ui->userButtonsProfileCombo->currentText());

    ui->userButtonsProfileCombo->blockSignals(false);
}

void RotatorWidget::refreshRotUserButtons()
{
    FCT_IDENTIFICATION;

    RotUsrButtonsProfile profile = RotUsrButtonsProfilesManager::instance()->getCurProfile1();

    setUserButtonDesc(ui->userButton_1,
                      profile.shortDescs[0],
                      profile.bearings[0]);
    setUserButtonDesc(ui->userButton_2,
                      profile.shortDescs[1],
                      profile.bearings[1]);
    setUserButtonDesc(ui->userButton_3,
                      profile.shortDescs[2],
                      profile.bearings[2]);
    setUserButtonDesc(ui->userButton_4,
                      profile.shortDescs[3],
                      profile.bearings[3]);
}

void RotatorWidget::setUserButtonDesc(QPushButton *button,
                                      const QString &desc,
                                      const double value)
{
    FCT_IDENTIFICATION;

    if ( value >= 0 )
    {
        button->setText(desc);
        button->setEnabled(true);
    }
    else
        button->setText("");
}

void RotatorWidget::redrawMap()
{
    FCT_IDENTIFICATION;

    if ( !ui )
        return;

    if ( compassScene )
        compassScene->deleteLater();

    compassScene = new QGraphicsScene(this);
    ui->compassView->setScene(compassScene);
    ui->compassView->setStyleSheet("background-color: transparent;");

    QImage source(":/res/map/nasabluemarble.jpg");
    QImage map(MAP_RESOLUTION, MAP_RESOLUTION, QImage::Format_ARGB32);
    const Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    double lat = myGrid.getLatitude();
    double lon = myGrid.getLongitude();

    if ( qIsNaN(lat) || qIsNaN(lon) )
        return;

    // transform image to azimuthal map
    double lambda0 = (lon / 180.0) * M_PI;
    double phi1 = - (lat / 90.0) * (0.5 * M_PI);

    for (int x = 0; x < map.width(); x++)
    {
        double x2 = 2.0 * M_PI * (static_cast<double>(x) / static_cast<double>(map.width()) - 0.5);
        for (int y = 0; y < map.height(); y++)
        {
            double y2 = 2.0 * M_PI * (static_cast<double>(y) / static_cast<double>(map.height()) - 0.5);
            double c = sqrt(x2 * x2 + y2 * y2);

            if ( c < M_PI )
            {
                double phi = asin(cos(c) * sin(phi1) + y2 * sin(c) * cos(phi1) / c);

                double lambda;
                if (c != 0)
                    lambda = lambda0 + atan2(x2 * sin(c), c * cos(phi1) * cos(c) - y2 * sin(phi1) * sin(c));
                else
                    lambda = lambda0;

                double s = (lambda / (2 * M_PI)) + 0.5;
                double t = (phi / M_PI) + 0.5;

                int x3 = static_cast<int>(s * static_cast<double>(source.width())) % source.width();
                int y3 = static_cast<int>(t * static_cast<double>(source.height())) % source.height();

                if (x3 < 0) x3 += source.width();
                if (y3 < 0) y3 += source.height();

                map.setPixelColor(x, y, source.pixelColor(x3, y3));
            }
            else
                map.setPixelColor(x, y, QColor(0, 0, 0, 0));
        }
    }

    // draw azimuthal map
    QGraphicsPixmapItem *pixMapItem = compassScene->addPixmap(QPixmap::fromImage(map));
    pixMapItem->moveBy(-MAP_RESOLUTION/2, -MAP_RESOLUTION/2);
    pixMapItem->setTransformOriginPoint(MAP_RESOLUTION/2, MAP_RESOLUTION/2);
    pixMapItem->setScale(GLOBE_RADIUS * 2/MAP_RESOLUTION);

    // circle around the globe - globe "antialiasing"
    compassScene->addEllipse(-100, -100, GLOBE_RADIUS * 2, GLOBE_RADIUS * 2,
                             QPen(QColor(100, 100, 100)),
                             QBrush(QColor(0, 0, 0),
                             Qt::NoBrush));

    // point in the middle of globe
    compassScene->addEllipse(-1, -1, 2, 2,
                             QPen(Qt::NoPen),
                             QBrush(QColor(0, 0, 0),
                             Qt::SolidPattern));

    // draw needles
    QPainterPath path;
    path.lineTo(-2, 0);
    path.lineTo(0, -90);
    path.lineTo(2, 0);
    path.closeSubpath();

    QPainterPath path2;
    path2.lineTo(-1, 0);
    path2.lineTo(0, -90);
    path2.lineTo(1, 0);
    path2.closeSubpath();

    requestedAzimuthNeedle = compassScene->addPath(QPainterPath(path2),
                                                   QPen(Qt::NoPen),
                                                   QBrush(QColor(255,255,255),
                                                          Qt::SolidPattern));
    if ( !qIsNaN(requestedAzimuth) )
        requestedAzimuthNeedle->setRotation(requestedAzimuth);
    else
        requestedAzimuthNeedle->hide();

    antennaNeedle = compassScene->addPath(path,
                                          QPen(QColor(0, 0, 0, 150)),
                                          QBrush(QColor(255, 191, 0),
                                          Qt::SolidPattern));
    antennaNeedle->setRotation(antennaAzimuth);
    if ( !ui->gotoButton->isEnabled() )
        antennaNeedle->hide();

    QSOAzimuthNeedle = compassScene->addPath(QPainterPath(path2),
                                             QPen(Qt::NoPen),
                                             QBrush(QColor(255,0,255),
                                             Qt::SolidPattern));

    setQSOBearing(qQNaN(), qQNaN()); // only call the function; input parameters are ignored
}

void RotatorWidget::rotProfileComboChanged(QString profileName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << profileName;

    RotProfilesManager::instance()->setCurProfile1(profileName);

    emit rotProfileChanged();
}

void RotatorWidget::rotUserButtonProfileComboChanged(QString profileName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << profileName;

    RotUsrButtonsProfilesManager::instance()->setCurProfile1(profileName);

    refreshRotUserButtons();

    emit rotUserButtonChanged();
}

void RotatorWidget::reloadSettings()
{
    FCT_IDENTIFICATION;

    refreshRotProfileCombo();
    refreshRotUserButtonProfileCombo();
}

void RotatorWidget::rotConnected()
{
    FCT_IDENTIFICATION;

    ui->rotProfileCombo->setStyleSheet("QComboBox {color: green}");
    ui->gotoDoubleSpinBox->setEnabled(true);
    ui->gotoButton->setEnabled(true);
    ui->qsoBearingButton->setEnabled(true);
    ui->userButtonsProfileCombo->setEnabled(true);
    refreshRotUserButtons();
    waitingFirstValue = true;
    if ( antennaNeedle ) antennaNeedle->show();
}

void RotatorWidget::rotDisconnected()
{
    FCT_IDENTIFICATION;

    ui->rotProfileCombo->setStyleSheet("QComboBox {color: red}");
    ui->gotoDoubleSpinBox->setEnabled(false);
    ui->gotoButton->setEnabled(false);
    ui->qsoBearingButton->setEnabled(false);
    ui->userButtonsProfileCombo->setEnabled(false);
    ui->userButton_1->setEnabled(false);
    ui->userButton_2->setEnabled(false);
    ui->userButton_3->setEnabled(false);
    ui->userButton_4->setEnabled(false);
    if ( antennaNeedle ) antennaNeedle->hide();
    if ( requestedAzimuthNeedle) requestedAzimuthNeedle->hide();
    requestedAzimuth = qQNaN();
}

RotatorWidget::~RotatorWidget()
{
    delete ui;
}

void RotatorWidget::registerContactWidget(const NewContactWidget *contactWidget)
{
    FCT_IDENTIFICATION;

    contact = contactWidget;
}
