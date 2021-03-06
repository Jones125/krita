/* This file is part of the KDE project

   Copyright (c) 2017 L. E. Segovia <amy@amyspark.me>


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <resources/KoSvgSymbolCollectionResource.h>

#include <QDebug>
#include <QVector>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QPainter>

#include <klocalizedstring.h>

#include <KoStore.h>
#include <KoDocumentResourceManager.h>
#include "kis_debug.h"

#include <KoShape.h>
#include <KoShapeGroup.h>
#include <KoShapeManager.h>
#include <KoShapePaintingContext.h>
#include <SvgParser.h>

#include <FlakeDebug.h>

QImage KoSvgSymbol::icon()
{
    KoShapeGroup *group = dynamic_cast<KoShapeGroup*>(shape);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(group, QImage());

    QRectF rc = group->boundingRect().normalized();

    QImage image(rc.width(), rc.height(), QImage::Format_ARGB32_Premultiplied);
    QPainter gc(&image);
    image.fill(Qt::gray);

    KoShapePaintingContext ctx;

//        debugFlake << "Going to render. Original bounding rect:" << group->boundingRect()
//                 << "Normalized: " << rc
//                 << "Scale W" << 256 / rc.width() << "Scale H" << 256 / rc.height();

    gc.translate(-rc.x(), -rc.y());
    KoShapeManager::renderSingleShape(group, gc, ctx);
    gc.end();
    image = image.scaled(128, 128, Qt::KeepAspectRatio);
    return image;
}



struct KoSvgSymbolCollectionResource::Private {
    QVector<KoSvgSymbol*> symbols;
    QString title;
    QString description;
};


KoSvgSymbolCollectionResource::KoSvgSymbolCollectionResource(const QString& filename)
    : KoResource(filename)
    , d(new Private())
{
}

KoSvgSymbolCollectionResource::KoSvgSymbolCollectionResource()
    : KoResource(QString())
    , d(new Private())
{
}

KoSvgSymbolCollectionResource::KoSvgSymbolCollectionResource(const KoSvgSymbolCollectionResource& rhs)
    : QObject(0)
    , KoResource(QString())
    , d(new Private())
{
    setFilename(rhs.filename());

    Q_FOREACH(KoSvgSymbol *symbol, rhs.d->symbols) {
        d->symbols << new KoSvgSymbol(*symbol);
    }

    setValid(true);
}

KoSvgSymbolCollectionResource::~KoSvgSymbolCollectionResource()
{
    qDeleteAll(d->symbols);
}

bool KoSvgSymbolCollectionResource::load()
{
    QFile file(filename());
    if (file.size() == 0) return false;
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    bool res =  loadFromDevice(&file);
    file.close();
    return res;
}



bool KoSvgSymbolCollectionResource::loadFromDevice(QIODevice *dev)
{
    if (!dev->isOpen()) dev->open(QIODevice::ReadOnly);

    QString errorMsg;
    int errorLine = 0;
    int errorColumn;

    KoXmlDocument doc = SvgParser::createDocumentFromSvg(dev, &errorMsg, &errorLine, &errorColumn);
    if (doc.isNull()) {

        errKrita << "Parsing error in " << filename() << "! Aborting!" << endl
        << " In line: " << errorLine << ", column: " << errorColumn << endl
        << " Error message: " << errorMsg << endl;
        errKrita << i18n("Parsing error in the main document at line %1, column %2\nError message: %3"
                         , errorLine , errorColumn , errorMsg);
        return false;
    }

    KoDocumentResourceManager manager;
    SvgParser parser(&manager);
    parser.setResolution(QRectF(0,0,100,100), 72); // initialize with default values
    QSizeF fragmentSize;
    // We're not interested in the shapes themselves
    qDeleteAll(parser.parseSvg(doc.documentElement(), &fragmentSize));
    d->symbols = parser.takeSymbols();
//    debugFlake << "Loaded" << filename() << "\n\t"
//             << "Title" << parser.documentTitle() << "\n\t"
//             << "Description" << parser.documentDescription()
//             << "\n\tgot" << d->symbols.size() << "symbols"
//             << d->symbols[0]->shape->outlineRect()
//             << d->symbols[0]->shape->size();

    d->title = parser.documentTitle();
    setName(d->title);
    d->description = parser.documentDescription();

    if (d->symbols.size() < 1) {
        setValid(false);
        return false;
    }
    setValid(true);
    setImage(d->symbols[0]->icon());
    return true;
}

bool KoSvgSymbolCollectionResource::save()
{
    QFile file(filename());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    saveToDevice(&file);
    file.close();
    return true;
}

bool KoSvgSymbolCollectionResource::saveToDevice(QIODevice *dev) const
{
    bool res = false;
    // XXX
    if (res) {
        KoResource::saveToDevice(dev);
    }
    return res;
}

QString KoSvgSymbolCollectionResource::defaultFileExtension() const
{
    return QString(".svg");
}

QString KoSvgSymbolCollectionResource::title() const
{
    return d->title;
}

QString KoSvgSymbolCollectionResource::description() const
{
    return d->description;
}

QString KoSvgSymbolCollectionResource::creator() const
{
    return "";
}

QString KoSvgSymbolCollectionResource::rights() const
{
    return "";
}

QString KoSvgSymbolCollectionResource::language() const
{
    return "";
}

QStringList KoSvgSymbolCollectionResource::subjects() const
{
    return QStringList();
}

QString KoSvgSymbolCollectionResource::license() const
{
    return "";
}

QStringList KoSvgSymbolCollectionResource::permits() const
{
    return QStringList();
}

QVector<KoSvgSymbol *> KoSvgSymbolCollectionResource::symbols() const
{
    return d->symbols;
}
