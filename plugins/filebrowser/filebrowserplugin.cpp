/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "filebrowserplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSharedPointer>
#include <QVariantList>
#include <QVariantMap>

#include "plugin_filebrowser_debug.h"

K_PLUGIN_CLASS_WITH_JSON(FileBrowserPlugin, "kdeconnect_filebrowser.json")

FileBrowserPlugin::FileBrowserPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_root(QFileInfo(QDir::homePath()).canonicalFilePath())
{
    if (m_root.isEmpty()) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Could not resolve the home directory, browsing is disabled";
    }
}

void FileBrowserPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_FILEBROWSER_REQUEST) {
        sendListing(np.get<QString>(QStringLiteral("path"), m_root));
    } else if (np.type() == PACKET_TYPE_FILEBROWSER_DOWNLOAD) {
        sendFile(np.get<QString>(QStringLiteral("path")));
    }
}

QString FileBrowserPlugin::resolvePath(const QString &path) const
{
    if (m_root.isEmpty()) {
        return QString();
    }

    const QString requested = path.isEmpty() ? m_root : QDir::cleanPath(path);
    // canonicalFilePath() also resolves symlinks, so a link pointing outside the root is
    // rejected like any other outside path
    const QString canonical = QFileInfo(requested).canonicalFilePath();
    if (canonical.isEmpty()) {
        return QString();
    }
    if (canonical != m_root && !canonical.startsWith(m_root + QLatin1Char('/'))) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Refusing a path outside the home directory";
        return QString();
    }
    return canonical;
}

void FileBrowserPlugin::sendListing(const QString &path)
{
    const QString directory = resolvePath(path);

    NetworkPacket np(PACKET_TYPE_FILEBROWSER_LISTING);
    np.set(QStringLiteral("path"), directory.isEmpty() ? path : directory);

    if (directory.isEmpty()) {
        np.set(QStringLiteral("error"), i18n("That folder is not available"));
        sendPacket(np);
        return;
    }

    const QDir dir(directory);
    const QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    QVariantList entries;
    entries.reserve(children.size());
    for (const QFileInfo &child : children) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), child.fileName());
        entry.insert(QStringLiteral("dir"), child.isDir());
        // A folder has no size of its own, and adding them up would need a full walk
        entry.insert(QStringLiteral("size"), child.isDir() ? 0 : child.size());
        entry.insert(QStringLiteral("modified"), child.lastModified().toSecsSinceEpoch());
        entries.append(entry);
    }
    np.set(QStringLiteral("entries"), entries);

    // Never let the parent point above the exposed root
    const QString parent = directory == m_root ? m_root : resolvePath(QFileInfo(directory).absolutePath());
    np.set(QStringLiteral("parent"), parent.isEmpty() ? m_root : parent);

    if (!sendPacket(np)) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Could not send a directory listing";
    }
}

void FileBrowserPlugin::sendFile(const QString &path)
{
    const QString filePath = resolvePath(path);
    const QFileInfo info(filePath);
    if (filePath.isEmpty() || !info.isFile() || !info.isReadable()) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Refusing to send a file that cannot be read";
        return;
    }

    // Reuse the share packet so the phone saves the file with its existing transfer code
    NetworkPacket np(QStringLiteral("kdeconnect.share.request"));
    np.set(QStringLiteral("filename"), info.fileName());
    np.set(QStringLiteral("lastModified"), info.lastModified().toMSecsSinceEpoch());

    auto file = QSharedPointer<QFile>::create(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Could not open the requested file";
        return;
    }

    np.setPayload(file, info.size());
    if (!sendPacket(np)) {
        qCWarning(KDECONNECT_PLUGIN_FILEBROWSER) << "Could not send the requested file";
    }
}

#include "filebrowserplugin.moc"
#include "moc_filebrowserplugin.cpp"
