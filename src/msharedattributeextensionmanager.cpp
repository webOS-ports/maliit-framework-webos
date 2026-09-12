/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2012 Mattia Barbon <mattia@develer.com>
 * Copyright (C) 2017-2021 LG Electronics, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include <climits>

#include <QDebug>
#include <utility>

#include "msharedattributeextensionmanager.h"
#include "mimsettings.h"

struct MSharedAttributeExtensionManagerPluginSetting
{
    MSharedAttributeExtensionManagerPluginSetting(const QString &key, Maliit::SettingEntryType type, QVariantMap attributes) :
        setting(key, MImSettings::GroupPlugin),
        type(type),
        attributes(std::move(attributes))
    {
    }

    MImSettings setting;
    Maliit::SettingEntryType type;
    QVariantMap attributes;
};


MSharedAttributeExtensionManager::MSharedAttributeExtensionManager() = default;

MSharedAttributeExtensionManager::~MSharedAttributeExtensionManager() = default;

void MSharedAttributeExtensionManager::registerPluginSetting(const QString &fullName, Maliit::SettingEntryType type,
                                                             QVariantMap attributes)
{
    QString key = fullName.section(QChar(1), -1);
    QSharedPointer<MSharedAttributeExtensionManagerPluginSetting> value(new MSharedAttributeExtensionManagerPluginSetting(key, type, std::move(attributes)));

    sharedAttributeExtensions[key] = value;

    connect(&value.data()->setting, SIGNAL(valueChanged()), this, SLOT(attributeValueChanged()));
}

void MSharedAttributeExtensionManager::handleClientDisconnect(unsigned int clientId)
{
    if (clientId > static_cast<unsigned int>(INT_MAX)) {
        qWarning() << "Client id does not fit into an int, ignoring. clientId:" << clientId;
        return;
    }
    clientIds.removeOne(static_cast<int>(clientId));
}

void MSharedAttributeExtensionManager::handleAttributeExtensionRegistered(unsigned int clientId, int id,
                                                                          const QString &attributeExtension)
{
    Q_UNUSED(attributeExtension);

    if (id != PluginSettings)
        return;

    if (clientId > static_cast<unsigned int>(INT_MAX)) {
        qWarning() << "Client id does not fit into an int, ignoring. clientId:" << clientId;
        return;
    }

    const int client = static_cast<int>(clientId);
    if (clientIds.contains(client))
        return;

    clientIds.append(client);
}

void MSharedAttributeExtensionManager::handleAttributeExtensionUnregistered(unsigned int clientId, int id)
{
    if (id != PluginSettings)
        return;

    if (clientId > static_cast<unsigned int>(INT_MAX)) {
        qWarning() << "Client id does not fit into an int, ignoring. clientId:" << clientId;
        return;
    }

    clientIds.removeOne(static_cast<int>(clientId));
}

void MSharedAttributeExtensionManager::handleExtendedAttributeUpdate(unsigned int clientId, int id,
                                   const QString &target, const QString &targetName,
                                   const QString &attribute, const QVariant &value)
{
    Q_UNUSED(clientId);

    if (id != PluginSettings)
        return;

    QString key = QString::fromLatin1("%1/%2/%3").arg(target, targetName, attribute);
    SharedAttributeExtensionContainer::iterator it = sharedAttributeExtensions.find(key);

    if (it == sharedAttributeExtensions.end())
        return;
    // TODO error notification
    if (!validateSettingValue(it->data()->type, it->data()->attributes, value))
        return;

    it->data()->setting.set(value);
}

void MSharedAttributeExtensionManager::attributeValueChanged()
{
    MImSettings *value = qobject_cast<MImSettings *>(sender());

    if (!value)
        return;
    if (sharedAttributeExtensions.find(value->key()) == sharedAttributeExtensions.end())
        return;

    const QString fullName = value->key();
    const QString &target = QString::fromLatin1("/") + fullName.section(QChar('/'), 1, 1);
    const QString &targetItem = fullName.section(QChar('/'), 2, -2);
    const QString &attribute = fullName.section(QChar('/'), -1, -1);

    Q_EMIT notifyExtensionAttributeChanged(clientIds, PluginSettings, target, targetItem, attribute, value->value());
}
