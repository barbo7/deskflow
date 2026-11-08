/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QColor>
#include <QImage>
#include <QString>

namespace deskflow::gui {

//! "Mac", "Windows" or "Linux" for a platform sent by the core, or empty if unknown.
QString platformLabel(const QString &platform);

/**
 * Draw the tray icon that shows which computer has the cursor:
 * an apple for mac, four panes for windows, otherwise the computer's initial.
 * \param size Width and height in pixels.
 */
QImage drawActiveComputerIcon(const QString &platform, const QString &name, const QColor &color, int size = 64);

} // namespace deskflow::gui
