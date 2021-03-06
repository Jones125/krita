/*
 * KDE. Krita Project.
 *
 * Copyright (c) 2020 Deif Lou <ginoba@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <generator/kis_generator.h>
#include <generator/kis_generator_registry.h>

#include "KisHalftoneFilterConfiguration.h"

KisHalftoneFilterConfiguration::KisHalftoneFilterConfiguration(const QString & name,
                                                               qint32 version)
    : KisFilterConfiguration(name, version)
{}

// KisHalftoneFilterConfiguration::KisHalftoneFilterConfiguration(const KisHalftoneFilterConfiguration &rhs)
//     : KisFilterConfiguration(rhs)
// {
//     for (auto i = rhs.m_generatorConfigurations.constBegin(); i != rhs.m_generatorConfigurations.constEnd(); ++i) {
//         m_generatorConfigurations.insert(i.key(), i.value()->clone());
//     }
// }

KisHalftoneFilterConfiguration::~KisHalftoneFilterConfiguration()
{}

// KisFilterConfigurationSP KisHalftoneFilterConfiguration::clone() const
// {
//     return new KisHalftoneFilterConfiguration(*this);
// }

// QList<KoResourceSP> KisHalftoneFilterConfiguration::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
// {
//     QList<KoResourceSP> resourcesList;

//     for (KisFilterConfigurationSP filterConfig : m_generatorConfigurations) {
//         resourcesList += filterConfig->linkedResources(globalResourcesInterface);
//     }

//     return resourcesList;
// }

// QList<KoResourceSP> KisHalftoneFilterConfiguration::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
// {
//     QList<KoResourceSP> resourcesList;

//     for (KisFilterConfigurationSP filterConfig : m_generatorConfigurations) {
//         resourcesList += filterConfig->embeddedResources(globalResourcesInterface);
//     }

//     return resourcesList;
// }

QString KisHalftoneFilterConfiguration::colorModelId() const
{
    return getString("color_model_id", "");
}

QString KisHalftoneFilterConfiguration::mode() const
{
    return getString("mode", "");
}

QString KisHalftoneFilterConfiguration::generatorId(const QString &prefix) const
{
    return getString(prefix + "generator", "");
}

KisFilterConfigurationSP KisHalftoneFilterConfiguration::generatorConfiguration(const QString &prefix) const
{
    QStringList generatorIds = KisGeneratorRegistry::instance()->keys();
    QString generatorId = this->generatorId(prefix);
    if (generatorIds.indexOf(generatorId) != -1) {
        QString fullGeneratorId = prefix + "generator_" + generatorId;
        // if (m_generatorConfigurations.contains(fullGeneratorId)) {
        //     return m_generatorConfigurations.value(fullGeneratorId);
        // } else {
            KisGeneratorSP generator = KisGeneratorRegistry::instance()->get(generatorId);
            KisFilterConfigurationSP generatorConfig = generator->defaultConfiguration();
            getPrefixedProperties(fullGeneratorId + "_", generatorConfig);
            return generatorConfig;
        // }
    }
    return nullptr;
}

qreal KisHalftoneFilterConfiguration::hardness(const QString &prefix) const
{
    return getDouble(prefix + "hardness", defaultHardness());
}

bool KisHalftoneFilterConfiguration::invert(const QString &prefix) const
{
    return getBool(prefix + "invert", defaultInvert());
}

KoColor KisHalftoneFilterConfiguration::foregroundColor(const QString &prefix) const
{
    return getColor(prefix + "foreground_color", defaultForegroundColor());
}

int KisHalftoneFilterConfiguration::foregroundOpacity(const QString &prefix) const
{
    return getInt(prefix + "foreground_opacity", defaultForegroundOpacity());
}

KoColor KisHalftoneFilterConfiguration::backgroundColor(const QString &prefix) const
{
    return getColor(prefix + "background_color", defaultBackgroundColor());
}

int KisHalftoneFilterConfiguration::backgroundOpacity(const QString &prefix) const
{
    return getInt(prefix + "background_opacity", defaultForegroundOpacity());
}

void KisHalftoneFilterConfiguration::setColorModelId(const QString &newColorModelId)
{
    setProperty("color_model_id", newColorModelId);
}

void KisHalftoneFilterConfiguration::setMode(const QString &newMode)
{
    setProperty("mode", newMode);
}

void KisHalftoneFilterConfiguration::setGeneratorId(const QString &prefix, const QString &id)
{
    setProperty(prefix + "generator", id);
}

void KisHalftoneFilterConfiguration::setGeneratorConfiguration(const QString &prefix, KisFilterConfigurationSP config)
{
    if (!config) {
        return;
    }

    QString generatorId = this->generatorId(prefix);
    QString fullGeneratorId = prefix + "generator_" + generatorId;

    // m_generatorConfigurations.insert(fullGeneratorId, config);
    setPrefixedProperties(fullGeneratorId + "_", config);
}

void KisHalftoneFilterConfiguration::setHardness(const QString & prefix, qreal newHardness)
{
    setProperty(prefix + "hardness", newHardness);
}

void KisHalftoneFilterConfiguration::setInvert(const QString & prefix, bool newInvert)
{
    setProperty(prefix + "invert", newInvert);
}

void KisHalftoneFilterConfiguration::setForegroundColor(const QString & prefix, const KoColor & newForegroundColor)
{
    QVariant v;
    v.setValue(newForegroundColor);
    setProperty(prefix + "foreground_color", v);
}

void KisHalftoneFilterConfiguration::setForegroundOpacity(const QString & prefix, int newForegroundOpacity)
{
    setProperty(prefix + "foreground_opacity", newForegroundOpacity);
}
void KisHalftoneFilterConfiguration::setBackgroundColor(const QString & prefix, const KoColor & newBackgroundColor)
{
    QVariant v;
    v.setValue(newBackgroundColor);
    setProperty(prefix + "background_color", v);
}

void KisHalftoneFilterConfiguration::setBackgroundOpacity(const QString & prefix, int newBackgroundOpacity)
{
    setProperty(prefix + "background_opacity", newBackgroundOpacity);
}
