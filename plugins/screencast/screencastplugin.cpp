/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "screencastplugin.h"

#include <KPluginFactory>

#include <QBuffer>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QSharedPointer>

#include <core/device.h>

#include "kwinscreenshot.h"
#include "plugin_screencast_debug.h"
#include "screencastwindow.h"

K_PLUGIN_CLASS_WITH_JSON(ScreencastPlugin, "kdeconnect_screencast.json")

/**
 * Reads one incoming frame.
 *
 * A payload travels over its own socket, which is still being connected when the packet is
 * dispatched, so the bytes have to be collected as they arrive. The reader keeps the socket
 * alive and deletes itself once the frame has been decoded.
 */
class ScreencastFrameReader : public QObject
{
    Q_OBJECT

public:
    ScreencastFrameReader(const QSharedPointer<QIODevice> &payload, qint64 payloadSize, QObject *parent)
        : QObject(parent)
        , m_payload(payload)
        , m_payloadSize(payloadSize)
    {
        connect(m_payload.data(), &QIODevice::readyRead, this, &ScreencastFrameReader::read);
        connect(m_payload.data(), &QIODevice::readChannelFinished, this, &ScreencastFrameReader::read);
        read();
    }

Q_SIGNALS:
    void frameReceived(const QImage &frame);

private:
    void read()
    {
        QIODevice *stream = m_payload.data();
        if (m_finished || !stream) {
            return;
        }

        m_data.append(stream->readAll());
        // A short buffer means the transfer is not finished yet
        if (m_data.isEmpty() || (m_payloadSize > 0 && m_data.size() < m_payloadSize)) {
            return;
        }

        const QImage frame = QImage::fromData(m_data, "JPEG");
        m_finished = true;
        m_data.clear();

        if (frame.isNull()) {
            qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Could not decode the received frame";
        } else {
            Q_EMIT frameReceived(frame);
        }
        deleteLater();
    }

    QSharedPointer<QIODevice> m_payload;
    qint64 m_payloadSize;
    QByteArray m_data;
    bool m_finished = false;
};

namespace
{
constexpr int MaxFrameWidth = 1280;
constexpr int MaxFrameHeight = 720;
constexpr int JpegQuality = 60;
constexpr int FramesPerSecond = 5;

/**
 * Captures through Qt, which only works on X11: Wayland compositors do not let clients read
 * each other's pixels, so KWin's ScreenShot2 interface is used there instead.
 */
QImage captureWithQt()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen || QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        return QImage();
    }
    return screen->grabWindow(0).toImage();
}
}

ScreencastPlugin::ScreencastPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    m_captureTimer.setInterval(1000 / FramesPerSecond);
    connect(&m_captureTimer, &QTimer::timeout, this, &ScreencastPlugin::sendFrame);
}

void ScreencastPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_SCREENCAST_FRAME) {
        handleFrame(np);
    } else if (np.type() == PACKET_TYPE_SCREENCAST_REQUEST) {
        // We never start capturing without the user asking, so just offer the window
        ensureWindow();
    } else if (np.type() == PACKET_TYPE_SCREENCAST_STOP) {
        // Either the peer stopped sharing, or it asked us to stop
        m_captureTimer.stop();
        if (m_window) {
            m_window->setFrame(QImage());
        }
    }
}

void ScreencastPlugin::handleFrame(const NetworkPacket &np)
{
    const QSharedPointer<QIODevice> payload = np.payload();
    if (!payload) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Received a frame without payload";
        return;
    }

    auto *reader = new ScreencastFrameReader(payload, np.payloadSize(), this);
    connect(reader, &ScreencastFrameReader::frameReceived, this, &ScreencastPlugin::showFrame);
}

void ScreencastPlugin::showFrame(const QImage &frame)
{
    ensureWindow();
    if (m_window) {
        m_window->setFrame(frame);
    }
}

void ScreencastPlugin::sendFrame()
{
    const QImage frame = captureScreen();
    if (frame.isNull()) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Screen capture is not available, stopping the stream";
        m_captureTimer.stop();
        return;
    }

    QImage scaled = frame;
    if (scaled.width() > MaxFrameWidth || scaled.height() > MaxFrameHeight) {
        scaled = scaled.scaled(MaxFrameWidth, MaxFrameHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray encoded;
    QBuffer encodedBuffer(&encoded);
    encodedBuffer.open(QIODevice::WriteOnly);
    if (!scaled.save(&encodedBuffer, "JPEG", JpegQuality)) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Could not encode a frame";
        return;
    }

    NetworkPacket np(PACKET_TYPE_SCREENCAST_FRAME);
    auto payload = QSharedPointer<QBuffer>::create();
    payload->setData(encoded);
    // Left closed on purpose: the upload job opens the payload before reading it
    np.setPayload(payload, encoded.size());

    if (!sendPacket(np)) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Could not send a frame, stopping the stream";
        m_captureTimer.stop();
    }
}

QImage ScreencastPlugin::captureScreen() const
{
    QImage frame = captureWithQt();
    if (!frame.isNull()) {
        return frame;
    }

    QString error;
    frame = captureWithKWinScreenShot2(&error);
    if (frame.isNull() && !error.isEmpty()) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Screen capture failed:" << error;
    }
    return frame;
}

void ScreencastPlugin::ensureWindow()
{
    // Called for every received frame, so it must not raise the window and steal focus
    if (m_window) {
        return;
    }

    auto *window = new ScreencastWindow(device()->name());
    connect(window, &ScreencastWindow::shareRequested, this, &ScreencastPlugin::startStreaming);
    connect(window, &ScreencastWindow::closed, this, [this] {
        m_captureTimer.stop();
    });

    m_window = window;
    window->show();
}

void ScreencastPlugin::requestStream()
{
    ensureWindow();
    if (m_window) {
        m_window->show();
        m_window->raise();
    }

    NetworkPacket np(PACKET_TYPE_SCREENCAST_REQUEST);
    if (!sendPacket(np)) {
        qCWarning(KDECONNECT_PLUGIN_SCREENCAST) << "Could not ask for a stream";
    }
}

void ScreencastPlugin::startStreaming()
{
    ensureWindow();
    m_captureTimer.start();
}

QString ScreencastPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/screencast").arg(device()->id());
}

#include "moc_screencastplugin.cpp"
#include "screencastplugin.moc"
