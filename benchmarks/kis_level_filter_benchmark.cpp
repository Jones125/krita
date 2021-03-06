/*
 *  Copyright (c) 2015 Thorsten Zachmann <zachmann@kde.org>
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

#include <QTest>

#include "kis_level_filter_benchmark.h"
#include "kis_benchmark_values.h"

#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColor.h>

#include <kis_image.h>

#include "filter/kis_filter_registry.h"
#include "filter/kis_filter_configuration.h"
#include "filter/kis_color_transformation_configuration.h"
#include "filter/kis_filter.h"

#include "kis_processing_information.h"

#include "kis_selection.h"
#include <kis_iterator_ng.h>
#include "krita_utils.h"

void KisLevelFilterBenchmark::initTestCase()
{
    m_colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    m_device = new KisPaintDevice(m_colorSpace);
    m_color = KoColor(m_colorSpace);

    QColor qcolor(Qt::red);
    srand(31524744);

    int r,g,b;

    KisSequentialIterator it(m_device, QRect(0,0,GMP_IMAGE_WIDTH, GMP_IMAGE_HEIGHT));
    while (it.nextPixel()) {
        r = rand() % 255;
        g = rand() % 255;
        b = rand() % 255;

        m_color.fromQColor(QColor(r,g,b));
        memcpy(it.rawData(), m_color.data(), m_colorSpace->pixelSize());
    }
}

void KisLevelFilterBenchmark::cleanupTestCase()
{
}

void KisLevelFilterBenchmark::benchmarkFilter()
{
    KisFilterSP filter = KisFilterRegistry::instance()->value("levels");
    //KisFilterConfigurationSP  kfc = filter->defaultConfiguration(m_device);

    KisColorTransformationConfiguration * kfc= new KisColorTransformationConfiguration("levels", 1);

    kfc->setProperty("blackvalue", 75);
    kfc->setProperty("whitevalue", 231);
    kfc->setProperty("gammavalue", 1.0);
    kfc->setProperty("outblackvalue", 0);
    kfc->setProperty("outwhitevalue", 255);
    // Get the predefined configuration from a file
    QFile file(QString(FILES_DATA_DIR) + '/' + filter->id() + ".cfg");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << kfc->toXML();
    } else {
        QString s;
        QTextStream in(&file);
        in.setCodec("UTF-8");
        s = in.readAll();
        kfc->fromXML(s);
    }

    QSize size = KritaUtils::optimalPatchSize();
    QVector<QRect> rects = KritaUtils::splitRectIntoPatches(QRect(0, 0, GMP_IMAGE_WIDTH,GMP_IMAGE_HEIGHT), size);

    QBENCHMARK{
        Q_FOREACH (const QRect &rc, rects) {
            filter->process(m_device, rc, kfc);
        }
    }
}



QTEST_MAIN(KisLevelFilterBenchmark)
