/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "screencastwindow.h"

#include <KLocalizedString>

#include <QCloseEvent>
#include <QPainter>
#include <QPushButton>

ScreencastWindow::ScreencastWindow(const QString &deviceName, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_shareButton(new QPushButton(i18nc("@action:button", "Share this screen"), this))
{
    setWindowTitle(i18nc("@title:window", "Screen sharing with %1", deviceName));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(320, 240);
    resize(640, 400);

    connect(m_shareButton, &QPushButton::clicked, this, &ScreencastWindow::shareRequested);
    m_shareButton->move(12, 12);
}

void ScreencastWindow::setFrame(const QImage &frame)
{
    m_frame = frame;
    // Sharing back only makes sense while nothing is being shown
    m_shareButton->setVisible(frame.isNull());
    update();
}

void ScreencastWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));

    if (m_frame.isNull()) {
        painter.drawText(rect().adjusted(12, 12, -12, -12),
                         Qt::AlignCenter,
                         i18nc("@info", "Waiting for the other device to share its screen..."));
        return;
    }

    QSize targetSize = m_frame.size();
    targetSize.scale(size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - targetSize.width()) / 2, (height() - targetSize.height()) / 2), targetSize);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, m_frame);
}

void ScreencastWindow::closeEvent(QCloseEvent *event)
{
    Q_EMIT closed();
    QWidget::closeEvent(event);
}

#include "moc_screencastwindow.cpp"
