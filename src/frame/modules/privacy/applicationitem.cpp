// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "applicationitem.h"

#include <DPinyin>

ApplicationItem::ApplicationItem(QObject *parent)
    : QObject(parent)
    , m_uniqueID(0)
{
    connect(this, &ApplicationItem::objectNameChanged, this, &ApplicationItem::dataChanged);
}

ApplicationItem::~ApplicationItem()
{
}

void ApplicationItem::setUniqueID(unsigned ID)
{
    m_uniqueID = ID;
}

QString ApplicationItem::id() const
{
    return m_id;
}

QString ApplicationItem::name() const
{
    return m_name;
}

QString ApplicationItem::appPath() const
{
    return m_appPath;
}

QStringList ApplicationItem::executablePaths() const
{
    return m_executablePaths;
}

QIcon ApplicationItem::icon() const
{
    return m_icon;
}

QString ApplicationItem::sortField() const
{
    return m_sortField;
}
// 默认允许
bool ApplicationItem::isPremissionEnabled(int premission) const
{
    return m_premissionMap.value(premission, true);
}

void ApplicationItem::setPremissionEnabled(int premission, bool enabled)
{
    // worker 实现
    Q_EMIT requestSetPremissionEnabled(premission, enabled, this);
}

void ApplicationItem::emitDataChanged()
{
    Q_EMIT dataChanged();
}

void ApplicationItem::onIdChanged(const QString &id)
{
    if (m_id == id)
        return;
    m_id = id;
    emitDataChanged();
}

void ApplicationItem::onNameChanged(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    m_sortField = DTK_CORE_NAMESPACE::Chinese2Pinyin(m_name);
    emitDataChanged();
}

void ApplicationItem::onAppPathChanged(const QString &path)
{
    if (m_appPath == path)
        return;
    m_appPath = path;
    emitDataChanged();
}

void ApplicationItem::onExecutablePathsChanged(const QStringList &paths)
{
    if (m_executablePaths == paths)
        return;
    m_executablePaths = paths;
}

void ApplicationItem::onIconChanged(const QIcon &icon)
{
    m_icon = icon;
    emitDataChanged();
}

void ApplicationItem::onPremissionEnabledChanged(int premission, bool enabled)
{
    if (m_premissionMap.contains(premission) && m_premissionMap.value(premission) == enabled)
        return;
    m_premissionMap[premission] = enabled;
    emitDataChanged();
}
