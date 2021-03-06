/*
 * This file is part of Krita
 *
 * Copyright (c) 2020 L. E. Segovia <amy@amyspark.me>
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

#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>
#include <QTest>
#include <generator/kis_generator_registry.h>
#include <kis_fill_painter.h>
#include <kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <kis_selection.h>
#include <resources/KisSeExprScript.h>
#include <sdk/tests/kistest.h>
#include <testutil.h>

#include "kis_seexpr_generator_test.h"

#define BASE_SCRIPT                                                                                                                                                                                                                            \
    "$val=voronoi(5*[$u,$v,.5],4,.6,.2); \n \
$color=ccurve($val,\n\
    0.000, [0.141, 0.059, 0.051], 4,\n\
    0.185, [0.302, 0.176, 0.122], 4,\n\
    0.301, [0.651, 0.447, 0.165], 4,\n\
    0.462, [0.976, 0.976, 0.976], 4);\n\
$color\n\
"

void KisSeExprGeneratorTest::initTestCase()
{
    KisGeneratorRegistry::instance();
}

void KisSeExprGeneratorTest::testGenerationFromScript()
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("seexpr");
    QVERIFY(generator);

    KisFilterConfigurationSP config = generator->defaultConfiguration();
    QVERIFY(config);

    config->setProperty("script", BASE_SCRIPT);

    QPoint point(0, 0);
    QSize testSize(256, 256);

    KisDefaultBoundsBaseSP bounds(new KisWrapAroundBoundsWrapper(new KisDefaultBounds(), QRect(point.x(), point.y(), testSize.width(), testSize.height())));
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(bounds);

    KisFillPainter fillPainter(dev);
    fillPainter.fillRect(point.x(), point.y(), 256, 256, config);

    QImage qimage(QString(FILES_DATA_DIR) + QDir::separator() + "noisecolor2.png");

    QPoint errpoint;
    if (!TestUtil::compareQImages(errpoint, qimage, dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()), 1)) {
        dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()).save("filtertest.png");
        QFAIL(QString("Failed to create image, first different pixel: %1,%2 ").arg(errpoint.x()).arg(errpoint.y()).toLatin1());
    }
}

void KisSeExprGeneratorTest::testGenerationFromKoResource()
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("seexpr");
    QVERIFY(generator);

    KisFilterConfigurationSP config = generator->defaultConfiguration();
    QVERIFY(config);

    auto resource = new KisSeExprScript(TestUtil::fetchDataFileLazy("Disney_noisecolor2.kse"));
    resource->load();
    Q_ASSERT(resource->valid());

    config->setProperty("script", resource->script());

    QPoint point(0, 0);
    QSize testSize(256, 256);

    KisDefaultBoundsBaseSP bounds(new KisWrapAroundBoundsWrapper(new KisDefaultBounds(), QRect(point.x(), point.y(), testSize.width(), testSize.height())));
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(bounds);

    KisFillPainter fillPainter(dev);
    fillPainter.fillRect(point.x(), point.y(), 256, 256, config);

    QImage qimage(QString(FILES_DATA_DIR) + QDir::separator() + "noisecolor2.png");

    QPoint errpoint;
    if (!TestUtil::compareQImages(errpoint, qimage, dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()), 1)) {
        dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()).save("filtertest.png");
        QFAIL(QString("Failed to create image, first different pixel: %1,%2 ").arg(errpoint.x()).arg(errpoint.y()).toLatin1());
    }
}

KISTEST_MAIN(KisSeExprGeneratorTest)
