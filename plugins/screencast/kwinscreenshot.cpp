/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kwinscreenshot.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>

#if defined(Q_OS_LINUX)
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace
{
// KWin answers with a delayed reply and then writes the image from a worker thread
constexpr int ReplyTimeoutMs = 5000;
constexpr int ReadTimeoutMs = 5000;
constexpr qsizetype MaxImageBytes = 128 * 1024 * 1024;

QString describeReply(const QDBusMessage &reply)
{
    const QString errorMessage = reply.errorMessage();
    return errorMessage.isEmpty() ? i18n("KWin did not answer the screen capture request") : errorMessage;
}
}

QImage captureWithKWinScreenShot2(QString *errorMessage)
{
#if !defined(Q_OS_LINUX)
    if (errorMessage) {
        *errorMessage = i18n("Capturing through KWin is only available on Linux");
    }
    return QImage();
#else
    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) {
        if (errorMessage) {
            *errorMessage = i18n("Could not create a pipe for the screen capture");
        }
        return QImage();
    }
    ::fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);

    // QDBusUnixFileDescriptor duplicates the descriptor it is given and does not take
    // ownership of it, so ours still has to be closed here
    const QDBusUnixFileDescriptor writeDescriptor(pipeFds[1]);
    ::close(pipeFds[1]);
    pipeFds[1] = -1;

    if (!writeDescriptor.isValid()) {
        ::close(pipeFds[0]);
        if (errorMessage) {
            *errorMessage = i18n("Could not pass the screen capture pipe to KWin");
        }
        return QImage();
    }

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.ScreenShot2"),
                                                       QStringLiteral("/org/kde/KWin/ScreenShot2"),
                                                       QStringLiteral("org.kde.KWin.ScreenShot2"),
                                                       QStringLiteral("CaptureWorkspace"));

    // Our own window must not be hidden while streaming
    QVariantMap options;
    options.insert(QStringLiteral("hide-caller-windows"), false);
    options.insert(QStringLiteral("native-resolution"), false);

    call << options << QVariant::fromValue(writeDescriptor);

    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, ReplyTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        ::close(pipeFds[0]);
        if (errorMessage) {
            *errorMessage = describeReply(reply);
        }
        return QImage();
    }

    // "format" is a QImage::Format, and is returned as a uint like the other numbers
    const QVariantMap results = reply.arguments().value(0).toMap();
    const int width = results.value(QStringLiteral("width")).toInt();
    const int height = results.value(QStringLiteral("height")).toInt();
    const int stride = results.value(QStringLiteral("stride")).toInt();
    const int format = results.value(QStringLiteral("format")).toInt();
    const double scale = results.value(QStringLiteral("scale")).toDouble();

    const bool isRaw = results.value(QStringLiteral("type")).toString() == QLatin1String("raw");
    if (!isRaw || width <= 0 || height <= 0 || stride <= 0) {
        ::close(pipeFds[0]);
        if (errorMessage) {
            *errorMessage = i18n("KWin returned an unsupported screen capture");
        }
        return QImage();
    }

    const qsizetype expectedBytes = qsizetype(stride) * height;
    if (expectedBytes > MaxImageBytes) {
        ::close(pipeFds[0]);
        if (errorMessage) {
            *errorMessage = i18n("The screen capture is too large");
        }
        return QImage();
    }

    // KWin writes from another process, so read until the image is complete instead of
    // assuming it all arrived with the reply
    QByteArray data;
    data.reserve(expectedBytes);
    while (data.size() < expectedBytes) {
        pollfd descriptor = {pipeFds[0], POLLIN, 0};
        if (::poll(&descriptor, 1, ReadTimeoutMs) <= 0) {
            break;
        }
        char buffer[64 * 1024];
        const ssize_t count = ::read(pipeFds[0], buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        data.append(buffer, int(count));
    }
    ::close(pipeFds[0]);

    if (data.size() < expectedBytes) {
        if (errorMessage) {
            *errorMessage = i18n("The screen capture was truncated");
        }
        return QImage();
    }

    QImage image(reinterpret_cast<const uchar *>(data.constData()), width, height, stride, static_cast<QImage::Format>(format));
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = i18n("KWin returned an unreadable image");
        }
        return QImage();
    }

    // Detach from the buffer that is about to be destroyed
    image = image.copy();
    if (scale > 0) {
        image.setDevicePixelRatio(scale);
    }
    return image;
#endif
}
