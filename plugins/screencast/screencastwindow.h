/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QImage>
#include <QWidget>

class QPushButton;

/**
 * A small, resizable window showing the peer's screen. It keeps the aspect ratio of the
 * received frames and offers to share this screen in return.
 */
class ScreencastWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScreencastWindow(const QString &deviceName, QWidget *parent = nullptr);

    void setFrame(const QImage &frame);

Q_SIGNALS:
    void shareRequested();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QImage m_frame;
    QPushButton *m_shareButton;
};
