/*
 *  Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_png_brush.h"

#include <QDomElement>
#include <QFileInfo>
#include <QImageReader>
#include <QByteArray>
#include <QBuffer>
#include <QPainter>

#include <kis_dom_utils.h>

KisPngBrush::KisPngBrush(const QString& filename)
    : KisColorfulBrush(filename)
{
    setBrushType(INVALID);
    setSpacing(0.25);
}

KisPngBrush::KisPngBrush(const KisPngBrush &rhs)
    : KisColorfulBrush(rhs)
{
}

KisBrush* KisPngBrush::clone() const
{
    return new KisPngBrush(*this);
}

bool KisPngBrush::load()
{
    QFile f(filename());
    if (f.size() == 0) return false;
    if (!f.exists()) return false;
    if (!f.open(QIODevice::ReadOnly)) {
        warnKrita << "Can't open file " << filename();
        return false;
    }
    bool res = loadFromDevice(&f);
    f.close();
    return res;
}

bool KisPngBrush::loadFromDevice(QIODevice *dev)
{

    // Workaround for some OS (Debian, Ubuntu), where loading directly from the QIODevice
    // fails with "libpng error: IDAT: CRC error"
    QByteArray data = dev->readAll();
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, "PNG");

    if (!reader.canRead()) {
        dbgKrita << "Could not read brush" << filename() << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    if (reader.textKeys().contains("brush_spacing")) {
        setSpacing(KisDomUtils::toDouble(reader.text("brush_spacing")));
    }

    if (reader.textKeys().contains("brush_name")) {
        setName(reader.text("brush_name"));
    }
    else {
        QFileInfo info(filename());
        setName(info.completeBaseName());
    }

    QImage image = reader.read();

    if (image.isNull()) {
        dbgKrita << "Could not create image for" << filename() << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    setValid(true);

    bool hasAlpha = false;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if (qAlpha(image.pixel(x, y)) != 255) {
                hasAlpha = true;
                break;
            }
        }
    }

    const bool isAllGray = image.allGray();

    if (isAllGray && !hasAlpha) {
        // Make sure brush tips all have a white background
        // NOTE: drawing it over white background can probably be skipped now...
        //       Any images with an Alpha channel should be loaded as RGBA so
        //       they can have the lightness and gradient options available
        QImage base(image.size(), image.format());
        if ((int)base.format() < (int)QImage::Format_RGB32) {
            base = base.convertToFormat(QImage::Format_ARGB32);
        }
        QPainter gc(&base);
        gc.fillRect(base.rect(), Qt::white);
        gc.drawImage(0, 0, image);
        gc.end();
        QImage converted = base.convertToFormat(QImage::Format_Grayscale8);
        setBrushTipImage(converted);
        setBrushType(MASK);
        setBrushApplication(ALPHAMASK);
        setHasColorAndTransparency(false);
    }
    else {
        if ((int)image.format() < (int)QImage::Format_RGB32) {
            image = image.convertToFormat(QImage::Format_ARGB32);
        }
        setBrushTipImage(image);
        setBrushType(IMAGE);
        setBrushApplication(isAllGray ? ALPHAMASK : IMAGESTAMP);
        setHasColorAndTransparency(!isAllGray);
    }

    setWidth(brushTipImage().width());
    setHeight(brushTipImage().height());

    return valid();
}

bool KisPngBrush::save()
{
    QFile f(filename());
    if (!f.open(QFile::WriteOnly)) return false;
    bool res = saveToDevice(&f);
    f.close();
    return res;
}

bool KisPngBrush::saveToDevice(QIODevice *dev) const
{
    if(brushTipImage().save(dev, "PNG")) {
        KoResource::saveToDevice(dev);
        return true;
    }

    return false;
}

QString KisPngBrush::defaultFileExtension() const
{
    return QString(".png");
}

void KisPngBrush::toXML(QDomDocument& d, QDomElement& e) const
{
    predefinedBrushToXML("png_brush", e);
    KisColorfulBrush::toXML(d, e);
}
