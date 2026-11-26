/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_SCREENCAST_REQUEST QStringLiteral("kdeconnect.screencast.request")
#define PACKET_TYPE_SCREENCAST_FRAME QStringLiteral("kdeconnect.screencast.frame")
#define PACKET_TYPE_SCREENCAST_STOP QStringLiteral("kdeconnect.screencast.stop")

class ScreencastWindow;

/**
 * Shares screens with the paired device in both directions.
 *
 * The peer either asks us for our screen, or we ask it for its screen. Frames travel as
 * JPEG payloads, so no new transport is needed.
 */
class ScreencastPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.screencast")

public:
    ScreencastPlugin(QObject *parent, const QVariantList &args);

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

public Q_SLOTS:
    /// Ask the peer for its screen and show it in a window.
    Q_SCRIPTABLE void requestStream();
    /// Start sending our screen to the peer.
    Q_SCRIPTABLE void startStreaming();

private:
    void handleFrame(const NetworkPacket &np);
    void sendFrame();
    void showFrame(const QImage &frame);
    void ensureWindow();

    QImage captureScreen() const;

    QPointer<ScreencastWindow> m_window;
    QTimer m_captureTimer;
};
