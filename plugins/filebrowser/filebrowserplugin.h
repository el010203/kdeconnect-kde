/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>

#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_FILEBROWSER_REQUEST QStringLiteral("kdeconnect.filebrowser.request")
#define PACKET_TYPE_FILEBROWSER_LISTING QStringLiteral("kdeconnect.filebrowser.listing")
#define PACKET_TYPE_FILEBROWSER_DOWNLOAD QStringLiteral("kdeconnect.filebrowser.download")

/**
 * Lets the paired device list and download files from this computer.
 *
 * Only the user's home directory is exposed. Downloads are sent as normal share packets, so
 * the receiving side saves them with the file transfer code it already has.
 */
class FileBrowserPlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    FileBrowserPlugin(QObject *parent, const QVariantList &args);

    void receivePacket(const NetworkPacket &np) override;

private:
    void sendListing(const QString &path);
    void sendFile(const QString &path);

    /// Resolves a requested path, refusing anything outside the exposed root.
    QString resolvePath(const QString &path) const;

    QString m_root;
};
