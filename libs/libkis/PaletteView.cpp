/*
 *  SPDX-FileCopyrightText: 2017 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PaletteView.h>
#include <QVBoxLayout>

struct PaletteView::Private
{
    KisPaletteModel *model = 0;
    KisPaletteView *widget = 0;
    bool allowPaletteModification = true;
};

PaletteView::PaletteView(QWidget *parent)
    : QWidget(parent), d(new Private)
{
    d->widget = new KisPaletteView();
    d->model = new KisPaletteModel();
    d->widget->setPaletteModel(d->model);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(d->widget);

    //forward signals.

//    connect(d->widget, SIGNAL(entrySelected(KisSwatch)),
//                 this, SLOT(fgSelected(KisSwatch)));
//    connect(d->widget, SIGNAL(entrySelectedBackGround(KisSwatch)),
//            this, SLOT(bgSelected(KisSwatch)));
    connect(d->widget, SIGNAL(sigColorSelected(const KoColor &)),
            this, SLOT(colorSelected(const KoColor &)));

    connect(d->widget, SIGNAL(sigIndexSelected(const QModelIndex &)),
            this, SLOT(colorSelectedEntry(const QModelIndex &)));

}

PaletteView::~PaletteView()
{
    delete d->widget;
    delete d->model;
}

void PaletteView::setPalette(Palette *palette)
{
    d->model->setColorSet(palette->colorSet());
    d->widget->setPaletteModel(d->model);
}

bool PaletteView::addEntryWithDialog(ManagedColor *color)
{
    if (d->model->colorSet()) {
        return d->widget->addEntryWithDialog(color->color());
    }
    return false;
}

bool PaletteView::addGroupWithDialog()
{
    if (d->model->colorSet()) {
        return d->widget->addGroupWithDialog();
    }
    return false;
}

bool PaletteView::removeSelectedEntryWithDialog()
{
    if (d->model->colorSet()) {
        return d->widget->removeEntryWithDialog(d->widget->currentIndex());
    }
    return false;
}

void PaletteView::trySelectClosestColor(ManagedColor *color)
{
    d->widget->selectClosestColor(color->color());
}

void PaletteView::fgSelected(QModelIndex index)
{
    KisSwatch swatch = d->model->getSwatch(index);
    Q_EMIT entrySelectedForeGround(Swatch(swatch));
}

void PaletteView::colorSelected(const KoColor & color)
{
    emit entryColorSelected(color);
}

void PaletteView::colorSelectedEntry(const QModelIndex & index)
{
    emit entryColorSelectedEntry(index);
}
