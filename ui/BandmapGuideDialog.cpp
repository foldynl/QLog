#include "BandmapGuideDialog.h"
#include "ui_BandmapGuideDialog.h"

#include <algorithm>

#include <QColorDialog>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include "ui/component/BaseDoubleSpinBox.h"
#include "core/debug.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.bandmapguidedialog");

BandmapGuideDialog::BandmapGuideDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BandmapGuideDialog),
    currentGuideIndex(-1)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->mainHLayout->setStretch(0, 0);
    ui->mainHLayout->setStretch(1, 1);

    QHeaderView *hdr = ui->rangesTable->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::ResizeToContents);

    loadGuides();
    populateGuideList();

    const QString currentId = BandmapGuide::currentProfileId();
    int selectedRow = 0;
    for ( int i = 0; i < guides.size(); i++ )
    {
        if ( guides[i].id == currentId )
        {
            selectedRow = i;
            break;
        }
    }

    if ( ui->guideList->count() > 0 )
        ui->guideList->setCurrentRow(selectedRow);

    int tableWidth = hdr->length() + ui->rangesTable->frameWidth() * 2;
    int leftPanelWidth = ui->leftLayout->sizeHint().width();
    QMargins m = ui->mainHLayout->contentsMargins();
    int needed = m.left() + m.right() + ui->mainHLayout->spacing() + leftPanelWidth + tableWidth;

    resize(qMax(needed, width()), height());
}

BandmapGuideDialog::~BandmapGuideDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void BandmapGuideDialog::installWheelGuard(QWidget *widget)
{
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->installEventFilter(this);
}

// Prevent mouse wheel from changing combo/spin values while scrolling the table;
// wheel events are only handled when the widget has focus (after clicking on it)
bool BandmapGuideDialog::eventFilter(QObject *obj, QEvent *event)
{
    if ( event->type() == QEvent::Wheel )
    {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if ( widget && !widget->hasFocus() )
        {
            event->ignore();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void BandmapGuideDialog::loadGuides()
{
    FCT_IDENTIFICATION;

    guides = BandmapGuide::profiles();

    if ( guides.isEmpty() )
        guides.append(BandmapGuide::exampleProfile());
}

void BandmapGuideDialog::populateGuideList()
{
    FCT_IDENTIFICATION;

    ui->guideList->clear();

    for ( int i = 0; i < guides.size(); i++ )
    {
        QListWidgetItem *item = new QListWidgetItem(guides[i].name, ui->guideList);
        item->setData(Qt::UserRole, i);
    }
}

void BandmapGuideDialog::guideSelectionChanged()
{
    FCT_IDENTIFICATION;

    if ( currentGuideIndex >= 0 && currentGuideIndex < guides.size() )
        saveCurrentGuide();

    QListWidgetItem *item = ui->guideList->currentItem();

    if ( !item )
    {
        currentGuideIndex = -1;
        ui->nameEdit->clear();
        ui->rangesTable->setRowCount(0);
        return;
    }

    currentGuideIndex = item->data(Qt::UserRole).toInt();
    showGuideEditor(currentGuideIndex);
}

void BandmapGuideDialog::guideNameChanged(const QString &text)
{
    FCT_IDENTIFICATION;

    QListWidgetItem *item = ui->guideList->currentItem();

    if ( item )
        item->setText(text);
}

void BandmapGuideDialog::showGuideEditor(int index)
{
    FCT_IDENTIFICATION;

    if ( index < 0 || index >= guides.size() )
        return;

    ui->nameEdit->setText(guides[index].name);
    populateRangesTable(guides[index].ranges);
}

void BandmapGuideDialog::populateRangesTable(const QList<BandmapGuide::Range> &ranges)
{
    FCT_IDENTIFICATION;

    QList<BandmapGuide::Range> sortedRanges = ranges;
    sortRangesByFrom(&sortedRanges);

    ui->rangesTable->setRowCount(0);
    ui->rangesTable->setRowCount(sortedRanges.size());

    for ( int row = 0; row < sortedRanges.size(); row++ )
        setupRangeRow(row, sortedRanges[row]);

    ui->rangesTable->resizeColumnsToContents();
}

void BandmapGuideDialog::setupRangeRow(int row, const BandmapGuide::Range &range)
{
    FCT_IDENTIFICATION;

    BaseDoubleSpinBox *fromSpin = new BaseDoubleSpinBox(this);
    installWheelGuard(fromSpin);
    fromSpin->setDecimals(6);
    fromSpin->setRange(0.0, 999999.999999);
    fromSpin->setSingleStep(0.001);
    fromSpin->setSuffix(tr(" MHz"));
    fromSpin->setValue(range.from);
    ui->rangesTable->setCellWidget(row, 0, fromSpin);

    BaseDoubleSpinBox *toSpin = new BaseDoubleSpinBox(this);
    installWheelGuard(toSpin);
    toSpin->setDecimals(6);
    toSpin->setRange(0.0, 999999.999999);
    toSpin->setSingleStep(0.001);
    toSpin->setSuffix(tr(" MHz"));
    toSpin->setValue(range.to);
    ui->rangesTable->setCellWidget(row, 1, toSpin);

    QPushButton *colorButton = new QPushButton(this);
    setColorButton(colorButton, range.color.isValid() ? range.color : QColor::fromRgb(0x4d8ef7));
    connect(colorButton, &QPushButton::clicked, this, [this, colorButton]()
    {
        chooseColor(colorButton);
    });
    ui->rangesTable->setCellWidget(row, 2, colorButton);

    QLineEdit *labelEdit = new QLineEdit(range.label, this);
    ui->rangesTable->setCellWidget(row, 3, labelEdit);
}

QList<BandmapGuide::Range> BandmapGuideDialog::readRangesFromTable() const
{
    FCT_IDENTIFICATION;

    QList<BandmapGuide::Range> ranges;

    for ( int row = 0; row < ui->rangesTable->rowCount(); row++ )
    {
        BandmapGuide::Range range;

        BaseDoubleSpinBox *fromSpin = dynamic_cast<BaseDoubleSpinBox *>(ui->rangesTable->cellWidget(row, 0));
        BaseDoubleSpinBox *toSpin = dynamic_cast<BaseDoubleSpinBox *>(ui->rangesTable->cellWidget(row, 1));
        QPushButton *colorButton = qobject_cast<QPushButton *>(ui->rangesTable->cellWidget(row, 2));
        QLineEdit *labelEdit = qobject_cast<QLineEdit *>(ui->rangesTable->cellWidget(row, 3));

        range.from  = fromSpin ? fromSpin->value() : 0.0;
        range.to    = toSpin ? toSpin->value() : 0.0;
        range.color = colorButton ? colorFromButton(colorButton) : QColor();
        range.label = labelEdit ? labelEdit->text().trimmed() : QString();

        ranges.append(range);
    }

    sortRangesByFrom(&ranges);

    return ranges;
}

void BandmapGuideDialog::saveCurrentGuide()
{
    FCT_IDENTIFICATION;

    if ( currentGuideIndex < 0 || currentGuideIndex >= guides.size() )
        return;

    guides[currentGuideIndex].name = ui->nameEdit->text().trimmed();
    guides[currentGuideIndex].ranges = readRangesFromTable();
}

void BandmapGuideDialog::newGuide()
{
    FCT_IDENTIFICATION;

    BandmapGuide::Profile profile;
    profile.id = BandmapGuide::newProfileId();
    profile.name = tr("New Guide");
    resolveGuideNameConflict(&profile);
    profile.ranges.append(BandmapGuide::defaultRange());

    guides.append(profile);
    populateGuideList();
    ui->guideList->setCurrentRow(ui->guideList->count() - 1);
}

void BandmapGuideDialog::copyGuide()
{
    FCT_IDENTIFICATION;

    if ( currentGuideIndex < 0 || currentGuideIndex >= guides.size() )
        return;

    saveCurrentGuide();

    BandmapGuide::Profile profile = guides[currentGuideIndex];
    profile.id = BandmapGuide::newProfileId();
    profile.name = tr("Copy - %1").arg(profile.name);
    resolveGuideNameConflict(&profile);

    guides.append(profile);
    populateGuideList();
    ui->guideList->setCurrentRow(ui->guideList->count() - 1);
}

void BandmapGuideDialog::deleteGuide()
{
    FCT_IDENTIFICATION;

    if ( currentGuideIndex < 0 || currentGuideIndex >= guides.size() )
        return;

    const QString name = guides[currentGuideIndex].name;
    int ret = QMessageBox::question(this, tr("Delete Guide"),
                                    tr("Delete guide '%1'?").arg(name),
                                    QMessageBox::Yes | QMessageBox::No);
    if ( ret != QMessageBox::Yes )
        return;

    guides.removeAt(currentGuideIndex);
    currentGuideIndex = -1;
    populateGuideList();

    if ( ui->guideList->count() > 0 )
        ui->guideList->setCurrentRow(0);
    else
    {
        ui->nameEdit->clear();
        ui->rangesTable->setRowCount(0);
    }
}

void BandmapGuideDialog::importGuide()
{
    FCT_IDENTIFICATION;

    const QString filePath = QFileDialog::getOpenFileName(
                this,
                tr("Import Guide"),
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                tr("QLog Bandmap Guide (*.qbg);;JSON (*.json)"));

    if ( filePath.isEmpty() )
        return;

    QString error;
    BandmapGuide::Profile profile = BandmapGuide::readProfileFromFile(filePath, &error);
    if ( !profile.isValid() )
    {
        QMessageBox::critical(this, tr("Import Failed"), error);
        return;
    }

    sortRangesByFrom(&profile.ranges);
    resolveGuideNameConflict(&profile);
    guides.append(profile);
    populateGuideList();
    ui->guideList->setCurrentRow(ui->guideList->count() - 1);
}

void BandmapGuideDialog::exportGuide()
{
    FCT_IDENTIFICATION;

    if ( currentGuideIndex < 0 || currentGuideIndex >= guides.size() )
        return;

    saveCurrentGuide();

    const BandmapGuide::Profile &profile = guides[currentGuideIndex];
    const QString defaultName = profile.name.simplified().replace(' ', '_') + ".qbg";
    const QString filePath = QFileDialog::getSaveFileName(
                this,
                tr("Export Guide"),
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                + "/" + defaultName,
                tr("QLog Bandmap Guide (*.qbg)"));

    if ( filePath.isEmpty() )
        return;

    QString error;
    if ( !BandmapGuide::writeProfileToFile(profile, filePath, &error) )
        QMessageBox::critical(this, tr("Export Failed"), error);
}

void BandmapGuideDialog::addRange()
{
    FCT_IDENTIFICATION;

    int row = ui->rangesTable->rowCount();
    ui->rangesTable->setRowCount(row + 1);
    setupRangeRow(row, BandmapGuide::defaultRange());
    ui->rangesTable->setCurrentCell(row, 0);
}

void BandmapGuideDialog::removeRange()
{
    FCT_IDENTIFICATION;

    int row = ui->rangesTable->currentRow();
    if ( row >= 0 )
        ui->rangesTable->removeRow(row);
}

void BandmapGuideDialog::sortRangesByFrom(QList<BandmapGuide::Range> *ranges) const
{
    FCT_IDENTIFICATION;

    if ( !ranges )
        return;

    std::sort(ranges->begin(), ranges->end(),
              [](const BandmapGuide::Range &left, const BandmapGuide::Range &right)
    {
        return left.from < right.from;
    });
}

void BandmapGuideDialog::chooseColor(QPushButton *button)
{
    FCT_IDENTIFICATION;

    const QColor selected = QColorDialog::getColor(colorFromButton(button),
                                                   this,
                                                   tr("Guide Color"),
                                                   QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if ( selected.isValid() )
        setColorButton(button, selected);
}

void BandmapGuideDialog::setColorButton(QPushButton *button, const QColor &color)
{
    FCT_IDENTIFICATION;

    const QColor safeColor = color.isValid() ? color : QColor(0x4d8ef7);
    button->setProperty("color", safeColor);

    const QColor textColor = Data::textColorForBackground(safeColor,
                                                          button->palette().color(QPalette::ButtonText),
                                                          button->palette().color(QPalette::Button));
    button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; }")
                          .arg(safeColor.name(QColor::HexRgb), textColor.name(QColor::HexRgb)));
}

QColor BandmapGuideDialog::colorFromButton(QPushButton *button) const
{
    FCT_IDENTIFICATION;

    return button->property("color").value<QColor>();
}

bool BandmapGuideDialog::validateGuides()
{
    FCT_IDENTIFICATION;

    for ( int guideIndex = 0; guideIndex < guides.size(); guideIndex++ )
    {
        const BandmapGuide::Profile &profile = guides[guideIndex];
        const QString profileName = profile.name.trimmed();
        if ( profileName.isEmpty() )
        {
            QMessageBox::warning(this, tr("QLog Warning"), tr("Guide name cannot be empty."));
            ui->guideList->setCurrentRow(guideIndex);
            ui->nameEdit->setFocus();
            return false;
        }

        for ( int previousIndex = 0; previousIndex < guideIndex; previousIndex++ )
        {
            if ( guides[previousIndex].name.trimmed() == profileName )
            {
                QMessageBox::warning(this, tr("QLog Warning"),
                                     tr("Guide name '%1' is already used.").arg(profileName));
                ui->guideList->setCurrentRow(guideIndex);
                ui->nameEdit->setFocus();
                ui->nameEdit->selectAll();
                return false;
            }
        }

        for ( int rangeIndex = 0; rangeIndex < profile.ranges.size(); rangeIndex++ )
        {
            const BandmapGuide::Range &range = profile.ranges[rangeIndex];
            if ( !range.isValid() )
            {
                QMessageBox::warning(this, tr("QLog Warning"),
                                     tr("Guide '%1' contains an invalid range.").arg(profile.name));
                ui->guideList->setCurrentRow(guideIndex);
                ui->rangesTable->setCurrentCell(rangeIndex, 0);
                return false;
            }
        }
    }

    return true;
}

void BandmapGuideDialog::resolveGuideNameConflict(BandmapGuide::Profile *profile) const
{
    FCT_IDENTIFICATION;

    if ( !profile )
        return;

    const QString baseName = profile->name.trimmed().isEmpty() ? tr("New Guide") : profile->name.trimmed();
    profile->name = baseName;

    int suffix = 1;
    bool collision = true;

    while ( collision )
    {
        collision = false;
        for ( const BandmapGuide::Profile &existing : guides )
        {
            if ( existing.id != profile->id && existing.name == profile->name )
            {
                profile->name = QString("%1 (%2)").arg(baseName).arg(suffix++);
                collision = true;
                break;
            }
        }
    }
}

void BandmapGuideDialog::accept()
{
    FCT_IDENTIFICATION;

    saveCurrentGuide();

    for ( int i = 0; i < guides.size(); i++ )
        sortRangesByFrom(&guides[i].ranges);

    if ( !validateGuides() )
        return;

    BandmapGuide::saveProfiles(guides);

    QDialog::accept();
}
