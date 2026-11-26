/**
 * SPDX-FileCopyrightText: 2026 KDE Connect contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QImage>
#include <QString>

/**
 * Captures the whole desktop through KWin's org.kde.KWin.ScreenShot2 D-Bus interface.
 *
 * KWin takes the pipe it should write the image into as an input argument and answers with
 * a results map describing the raw image, so the caller creates the pipe and reads it here.
 *
 * @param errorMessage set to a human readable reason when an empty image is returned
 */
QImage captureWithKWinScreenShot2(QString *errorMessage);
