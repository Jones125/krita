/*
    Copyright (C) 2005 Tim Beaulen <tbscope@gmail.org>
    Copyright (C) 2007 Jan Hambrecht <jaham@gmx.net>
    Copyright (c) 2007 Sven Langkamp <sven.langkamp@gmail.com>

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

#include <resources/KoStopGradient.h>

#include <cfloat>

#include <QColor>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QBuffer>

#include <klocalizedstring.h>
#include <DebugPigment.h>

#include "KoColorSpaceRegistry.h"
#include "KoMixColorsOp.h"

#include "kis_dom_utils.h"

#include <math.h>
#include <KoColorModelStandardIds.h>
#include <KoXmlNS.h>

KoStopGradient::KoStopGradient(const QString& filename)
    : KoAbstractGradient(filename)
{
}

KoStopGradient::~KoStopGradient()
{
}

bool KoStopGradient::operator==(const KoStopGradient& rhs) const
{
    return
        *colorSpace() == *rhs.colorSpace() &&
        spread() == rhs.spread() &&
        type() == rhs.type() &&
        m_start == rhs.m_start &&
        m_stop == rhs.m_stop &&
        m_focalPoint == rhs.m_focalPoint &&
        m_stops == rhs.m_stops;
}

KoAbstractGradient* KoStopGradient::clone() const
{
    return new KoStopGradient(*this);
}

bool KoStopGradient::load()
{
    QFile f(filename());
    if (!f.open(QIODevice::ReadOnly)) {
        warnPigment << "Can't open file " << filename();
        return false;
    }
    bool res = loadFromDevice(&f);
    f.close();
    return res;
}

bool KoStopGradient::loadFromDevice(QIODevice* dev)
{
    QString strExt;
    const int result = filename().lastIndexOf('.');
    if (result >= 0) {
        strExt = filename().mid(result).toLower();
    }
    QByteArray ba = dev->readAll();

    QBuffer buf(&ba);
    loadSvgGradient(&buf);
    if (m_stops.count() >= 2) {
        setValid(true);
    }
    updatePreview();
    return true;
}

bool KoStopGradient::save()
{
    QFile fileOut(filename());
    if (!fileOut.open(QIODevice::WriteOnly))
        return false;

    bool retval = saveToDevice(&fileOut);
    fileOut.close();

    return retval;
}

QGradient* KoStopGradient::toQGradient() const
{
    QGradient* gradient;

    switch (type()) {
    case QGradient::LinearGradient: {
        gradient = new QLinearGradient(m_start, m_stop);
        break;
    }
    case QGradient::RadialGradient: {
        QPointF diff = m_stop - m_start;
        qreal radius = sqrt(diff.x() * diff.x() + diff.y() * diff.y());
        gradient = new QRadialGradient(m_start, radius, m_focalPoint);
        break;
    }
    case QGradient::ConicalGradient: {
        qreal angle = atan2(m_start.y(), m_start.x()) * 180.0 / M_PI;
        if (angle < 0.0)
            angle += 360.0;
        gradient = new QConicalGradient(m_start, angle);
        break;
    }
    default:
        return 0;
    }
    QColor color;
    for (QList<KoGradientStop>::const_iterator i = m_stops.begin(); i != m_stops.end(); ++i) {
        i->color.toQColor(&color);
        gradient->setColorAt(i->position, color);
    }

    gradient->setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient->setSpread(this->spread());

    return gradient;
}

bool KoStopGradient::stopsAt(KoGradientStop& leftStop, KoGradientStop& rightStop, qreal t) const
{
    if (!m_stops.count())
        return false;
    if (t <= m_stops.first().position || m_stops.count() == 1) {
        // we have only one stop or t is before the first stop
        leftStop = m_stops.first();
        rightStop = KoGradientStop(-std::numeric_limits<double>::infinity(), leftStop.color, leftStop.type);
        return true;
    } else if (t >= m_stops.last().position) {
        // t is after the last stop
        rightStop = m_stops.last();
        leftStop = KoGradientStop(std::numeric_limits<double>::infinity(), rightStop.color, rightStop.type);
        return true;
    } else {
        // we have at least two color stops
        // -> find the two stops which frame our t
        auto it = std::lower_bound(m_stops.begin(), m_stops.end(), KoGradientStop(t, KoColor(), COLORSTOP), [](const KoGradientStop& a, const KoGradientStop& b) {
            return a.position < b.position;
            });
        leftStop = *(it - 1);
        rightStop = *(it);
        return true;
    }
}

void KoStopGradient::colorAt(KoColor& dst, qreal t) const
{
    KoColor buffer;

    KoGradientStop leftStop, rightStop;
    if (!stopsAt(leftStop, rightStop, t)) return;

    const KoColorSpace* mixSpace = KoColorSpaceRegistry::instance()->rgb8(dst.colorSpace()->profile());

    KoColor startDummy, endDummy;
    if (mixSpace) {
        startDummy = KoColor(leftStop.color, mixSpace);
        endDummy = KoColor(rightStop.color, mixSpace);
    } else {
        startDummy = leftStop.color;
        endDummy = rightStop.color;
    }
    const quint8* colors[2];
    colors[0] = startDummy.data();
    colors[1] = endDummy.data();

    qreal localT;
    qreal stopDistance = rightStop.position - leftStop.position;
    if (stopDistance < DBL_EPSILON) {
        localT = 0.5;
    } else {
        localT = (t - leftStop.position) / stopDistance;
    }
    qint16 colorWeights[2];
    colorWeights[0] = static_cast<quint8>((1.0 - localT) * 255 + 0.5);
    colorWeights[1] = 255 - colorWeights[0];

    //check if our mixspace exists, it doesn't at startup.
    if (mixSpace) {
        if (*buffer.colorSpace() != *mixSpace) {
            buffer = KoColor(mixSpace);
        }
        mixSpace->mixColorsOp()->mixColors(colors, colorWeights, 2, buffer.data());
    } else {
        buffer = KoColor(colorSpace());
        colorSpace()->mixColorsOp()->mixColors(colors, colorWeights, 2, buffer.data());
    }

    dst.fromKoColor(buffer);
}

KoStopGradient* KoStopGradient::fromQGradient(const QGradient* gradient)
{
    if (!gradient)
        return 0;

    KoStopGradient* newGradient = new KoStopGradient(QString());
    newGradient->setType(gradient->type());
    newGradient->setSpread(gradient->spread());

    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        const QLinearGradient* g = static_cast<const QLinearGradient*>(gradient);
        newGradient->m_start = g->start();
        newGradient->m_stop = g->finalStop();
        newGradient->m_focalPoint = g->start();
        break;
    }
    case QGradient::RadialGradient: {
        const QRadialGradient* g = static_cast<const QRadialGradient*>(gradient);
        newGradient->m_start = g->center();
        newGradient->m_stop = g->center() + QPointF(g->radius(), 0);
        newGradient->m_focalPoint = g->focalPoint();
        break;
    }
    case QGradient::ConicalGradient: {
        const QConicalGradient* g = static_cast<const QConicalGradient*>(gradient);
        qreal radian = g->angle() * M_PI / 180.0;
        newGradient->m_start = g->center();
        newGradient->m_stop = QPointF(100.0 * cos(radian), 100.0 * sin(radian));
        newGradient->m_focalPoint = g->center();
        break;
    }
    default:
        delete newGradient;
        return 0;
    }

    Q_FOREACH(const QGradientStop & stop, gradient->stops()) {
        KoColor color(newGradient->colorSpace());
        color.fromQColor(stop.second);
        newGradient->m_stops.append(KoGradientStop(stop.first, color, COLORSTOP));
    }

    newGradient->setValid(true);

    return newGradient;
}

void KoStopGradient::setStops(QList< KoGradientStop > stops)
{
    m_stops.clear();
    m_hasVariableStops = false;
    KoColor color;
    Q_FOREACH(const KoGradientStop & stop, stops) {
        color = stop.color;
        color.convertTo(colorSpace());
        m_stops.append(KoGradientStop(stop.position, color, stop.type));
        if (stop.type != COLORSTOP) {
            m_hasVariableStops = true;
        }
    }
    updatePreview();
}

QList<KoGradientStop> KoStopGradient::stops() const
{
    return m_stops;
}

bool KoStopGradient::hasVariableColors() const {
    return m_hasVariableStops;
}

void KoStopGradient::setVariableColors(const KoColor& foreground, const KoColor& background) {
    KoColor color;
    for (int i = 0; i < m_stops.count(); i++){
        if (m_stops[i].type == FOREGROUNDSTOP) {
            color = foreground;
        } else if (m_stops[i].type == BACKGROUNDSTOP) {
            color = background;
        } else continue;
        color.convertTo(colorSpace());
        m_stops[i].color = color;
    }
    updatePreview();
}

void KoStopGradient::bakeVariableColors(const KoColor& foreground, const KoColor& background) {
    const KoColor fgColor = foreground.convertedTo(colorSpace());
    const KoColor bgColor = background.convertedTo(colorSpace());

    for (int i = 0; i < m_stops.count(); i++){
        if (m_stops[i].type == FOREGROUNDSTOP) {
            m_stops[i].color = fgColor;
            m_stops[i].type = COLORSTOP;
        } else if (m_stops[i].type == BACKGROUNDSTOP) {
            m_stops[i].color = bgColor;
            m_stops[i].type = COLORSTOP;
        } else continue;
    }
    updatePreview();
}

void KoStopGradient::loadSvgGradient(QIODevice* file)
{
    QDomDocument doc;

    if (!(doc.setContent(file))) {
        file->close();
    } else {
        for (QDomNode n = doc.documentElement().firstChild(); !n.isNull(); n = n.nextSibling()) {
            QDomElement e = n.toElement();

            if (e.isNull()) continue;

            if (e.tagName() == "linearGradient" || e.tagName() == "radialGradient") {
                parseSvgGradient(e);
                return;
            }
            // Inkscape gradients are in another defs
            if (e.tagName() == "defs") {
                for (QDomNode defnode = e.firstChild(); !defnode.isNull(); defnode = defnode.nextSibling()) {
                    QDomElement defelement = defnode.toElement();

                    if (defelement.isNull()) continue;

                    if (defelement.tagName() == "linearGradient" || defelement.tagName() == "radialGradient") {
                        parseSvgGradient(defelement);
                        return;
                    }
                }
            }
        }
    }
}


void KoStopGradient::parseSvgGradient(const QDomElement& element)
{
    m_stops.clear();
    m_hasVariableStops = false;
    setSpread(QGradient::PadSpread);

    /*QString href = e.attribute( "xlink:href" ).mid( 1 );
    if( !href.isEmpty() )
    {
    }*/
    setName(element.attribute("id", i18n("SVG Gradient")));

    const KoColorSpace* rgbColorSpace = KoColorSpaceRegistry::instance()->rgb8();

    bool bbox = element.attribute("gradientUnits") != "userSpaceOnUse";

    if (element.tagName() == "linearGradient") {

        if (bbox) {
            QString s;

            s = element.attribute("x1", "0%");
            qreal xOrigin;
            if (s.endsWith('%'))
                xOrigin = s.remove('%').toDouble();
            else
                xOrigin = s.toDouble() * 100.0;

            s = element.attribute("y1", "0%");
            qreal yOrigin;
            if (s.endsWith('%'))
                yOrigin = s.remove('%').toDouble();
            else
                yOrigin = s.toDouble() * 100.0;

            s = element.attribute("x2", "100%");
            qreal xVector;
            if (s.endsWith('%'))
                xVector = s.remove('%').toDouble();
            else
                xVector = s.toDouble() * 100.0;

            s = element.attribute("y2", "0%");
            qreal yVector;
            if (s.endsWith('%'))
                yVector = s.remove('%').toDouble();
            else
                yVector = s.toDouble() * 100.0;

            m_start = QPointF(xOrigin, yOrigin);
            m_stop = QPointF(xVector, yVector);
        }
        else {
            m_start = QPointF(element.attribute("x1").toDouble(), element.attribute("y1").toDouble());
            m_stop = QPointF(element.attribute("x2").toDouble(), element.attribute("y2").toDouble());
        }
        setType(QGradient::LinearGradient);
    }
    else {
        if (bbox) {
            QString s;

            s = element.attribute("cx", "50%");
            qreal xOrigin;
            if (s.endsWith('%'))
                xOrigin = s.remove('%').toDouble();
            else
                xOrigin = s.toDouble() * 100.0;

            s = element.attribute("cy", "50%");
            qreal yOrigin;
            if (s.endsWith('%'))
                yOrigin = s.remove('%').toDouble();
            else
                yOrigin = s.toDouble() * 100.0;

            s = element.attribute("cx", "50%");
            qreal xVector;
            if (s.endsWith('%'))
                xVector = s.remove('%').toDouble();
            else
                xVector = s.toDouble() * 100.0;

            s = element.attribute("r", "50%");
            if (s.endsWith('%'))
                xVector += s.remove('%').toDouble();
            else
                xVector += s.toDouble() * 100.0;

            s = element.attribute("cy", "50%");
            qreal yVector;
            if (s.endsWith('%'))
                yVector = s.remove('%').toDouble();
            else
                yVector = s.toDouble() * 100.0;

            s = element.attribute("fx", "50%");
            qreal xFocal;
            if (s.endsWith('%'))
                xFocal = s.remove('%').toDouble();
            else
                xFocal = s.toDouble() * 100.0;

            s = element.attribute("fy", "50%");
            qreal yFocal;
            if (s.endsWith('%'))
                yFocal = s.remove('%').toDouble();
            else
                yFocal = s.toDouble() * 100.0;

            m_start = QPointF(xOrigin, yOrigin);
            m_stop = QPointF(xVector, yVector);
            m_focalPoint = QPointF(xFocal, yFocal);
        }
        else {
            m_start = QPointF(element.attribute("cx").toDouble(), element.attribute("cy").toDouble());
            m_stop = QPointF(element.attribute("cx").toDouble() + element.attribute("r").toDouble(),
                element.attribute("cy").toDouble());
            m_focalPoint = QPointF(element.attribute("fx").toDouble(), element.attribute("fy").toDouble());
        }
        setType(QGradient::RadialGradient);
    }
    // handle spread method
    QString spreadMethod = element.attribute("spreadMethod");
    if (!spreadMethod.isEmpty()) {
        if (spreadMethod == "reflect")
            setSpread(QGradient::ReflectSpread);
        else if (spreadMethod == "repeat")
            setSpread(QGradient::RepeatSpread);
    }

    for (QDomNode n = element.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement colorstop = n.toElement();
        if (colorstop.tagName() == "stop") {
            qreal opacity = 0.0;
            QColor c;
            float off;
            QString temp = colorstop.attribute("offset");
            if (temp.contains('%')) {
                temp = temp.left(temp.length() - 1);
                off = temp.toFloat() / 100.0;
            }
            else
                off = temp.toFloat();

            if (!colorstop.attribute("stop-color").isEmpty())
                parseSvgColor(c, colorstop.attribute("stop-color"));
            else {
                // try style attr
                QString style = colorstop.attribute("style").simplified();
                QStringList substyles = style.split(';', QString::SkipEmptyParts);
                Q_FOREACH(const QString & s, substyles) {
                    QStringList substyle = s.split(':');
                    QString command = substyle[0].trimmed();
                    QString params = substyle[1].trimmed();
                    if (command == "stop-color")
                        parseSvgColor(c, params);
                    if (command == "stop-opacity")
                        opacity = params.toDouble();
                }

            }
            if (!colorstop.attribute("stop-opacity").isEmpty())
                opacity = colorstop.attribute("stop-opacity").toDouble();

            KoColor color(rgbColorSpace);
            color.fromQColor(c);
            color.setOpacity(static_cast<quint8>(opacity * OPACITY_OPAQUE_U8 + 0.5));
            QString stopTypeStr = colorstop.attribute("krita:stop-type", "color-stop");
            KoGradientStopType stopType = KoGradientStop::typeFromString(stopTypeStr);
            if (stopType != COLORSTOP) {
                m_hasVariableStops = true;
            }
            //According to the SVG spec each gradient offset has to be equal to or greater than the previous one
            //if not it needs to be adjusted to be equal
            if (m_stops.count() > 0 && m_stops.last().position >= off) {
                off = m_stops.last().position;
            }
            m_stops.append(KoGradientStop(off, color, stopType));
        }
    }
}

void KoStopGradient::parseSvgColor(QColor& color, const QString& s)
{
    if (s.startsWith("rgb(")) {
        QString parse = s.trimmed();
        QStringList colors = parse.split(',');
        QString r = colors[0].right((colors[0].length() - 4));
        QString g = colors[1];
        QString b = colors[2].left((colors[2].length() - 1));

        if (r.contains('%')) {
            r = r.left(r.length() - 1);
            r = QString::number(int((qreal(255 * r.toDouble()) / 100.0)));
        }

        if (g.contains('%')) {
            g = g.left(g.length() - 1);
            g = QString::number(int((qreal(255 * g.toDouble()) / 100.0)));
        }

        if (b.contains('%')) {
            b = b.left(b.length() - 1);
            b = QString::number(int((qreal(255 * b.toDouble()) / 100.0)));
        }

        color = QColor(r.toInt(), g.toInt(), b.toInt());
    }
    else {
        QString rgbColor = s.trimmed();
        QColor c;
        if (rgbColor.startsWith('#'))
            c.setNamedColor(rgbColor);
        else {
            c = QColor(rgbColor);
        }
        color = c;
    }
}

QString KoStopGradient::defaultFileExtension() const
{
    return QString(".svg");
}

void KoStopGradient::toXML(QDomDocument& doc, QDomElement& gradientElt) const
{
    gradientElt.setAttribute("type", "stop");
    for (int s = 0; s < m_stops.size(); s++) {
        KoGradientStop stop = m_stops.at(s);
        QDomElement stopElt = doc.createElement("stop");
        stopElt.setAttribute("offset", KisDomUtils::toString(stop.position));
        stopElt.setAttribute("bitdepth", stop.color.colorSpace()->colorDepthId().id());
        stopElt.setAttribute("alpha", KisDomUtils::toString(stop.color.opacityF()));
        stopElt.setAttribute("stoptype", KisDomUtils::toString(stop.type));
        stop.color.toXML(doc, stopElt);
        gradientElt.appendChild(stopElt);
    }
}

KoStopGradient KoStopGradient::fromXML(const QDomElement& elt)
{
    KoStopGradient gradient;
    QList<KoGradientStop> stops;
    QDomElement stopElt = elt.firstChildElement("stop");
    while (!stopElt.isNull()) {
        qreal offset = KisDomUtils::toDouble(stopElt.attribute("offset", "0.0"));
        QString bitDepth = stopElt.attribute("bitdepth", Integer8BitsColorDepthID.id());
        KoColor color = KoColor::fromXML(stopElt.firstChildElement(), bitDepth);
        color.setOpacity(KisDomUtils::toDouble(stopElt.attribute("alpha", "1.0")));
        KoGradientStopType stoptype = static_cast<KoGradientStopType>(KisDomUtils::toInt(stopElt.attribute("stoptype", "0")));
        stops.append(KoGradientStop(offset, color, stoptype));
        stopElt = stopElt.nextSiblingElement("stop");
    }
    gradient.setStops(stops);
    return gradient;
}

bool KoStopGradient::saveToDevice(QIODevice* dev) const
{
    QTextStream stream(dev);

    const QString spreadMethod[3] = {
        QString("spreadMethod=\"pad\" "),
        QString("spreadMethod=\"reflect\" "),
        QString("spreadMethod=\"repeat\" ")
    };

    const QString indent = "    ";

    stream << "<svg xmlns=\"http://www.w3.org/2000/svg\" \n";
    stream << QString("    xmlns:krita=\"%1\"\n").arg(KoXmlNS::krita);
    stream << ">" << endl;


    stream << indent;
    stream << "<linearGradient id=\"" << name() << "\" ";
    stream << "gradientUnits=\"objectBoundingBox\" ";
    stream << spreadMethod[spread()];
    stream << ">" << endl;

    QColor color;

    // color stops
    Q_FOREACH(const KoGradientStop & stop, m_stops) {
        stop.color.toQColor(&color);
        stream << indent << indent;
        stream << "<stop stop-color=\"";
        stream << color.name();
        stream << "\" offset=\"" << QString().setNum(stop.position);
        stream << "\" stop-opacity=\"" << static_cast<float>(color.alpha()) / 255.0f;
        stream << "\" krita:stop-type=\"" << stop.typeString() << "\"";
            
        stream << " />" << endl;
    }
    stream << indent;
    stream << "</linearGradient>" << endl;
    stream << "</svg>" << endl;

    KoResource::saveToDevice(dev);

    return true;
}