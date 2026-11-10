/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

#include <QByteArray>
#include <QList>
#include <QString>

void ipcSendToClient(const QString &command, const QString &args = "");
void ipcSendConnectionState(deskflow::core::ConnectionState state);

/**
 * Tell the gui which computer has the cursor, and which computers are connected.
 * \param active Name of the computer that has the cursor (empty if unknown).
 * \param computers Connected computers as "name|platform" pairs joined by ",".
 */
void ipcSendActiveComputer(const QString &active, const QString &computers);

/**
 * Tell the gui about the monitors of every connected computer.
 * \param monitors As written by deskflow::formatMonitorMap().
 */
void ipcSendMonitors(const QString &monitors);

//! Whether the extension of \p browser is connected. Can be called from any thread.
bool ipcHasBrowserHost(const QString &browser);

/**
 * Asks a browser's extension for the state of its pages, and waits for the answer.
 * \param mode "drag" or "hotkey".
 * \param url The address of the window being sent, so the extension can find that window.
 * \return The extension's answer as JSON; empty if it didn't give one in \p timeoutMs.
 */
QByteArray ipcCaptureBrowserState(const QString &browser, const QString &mode, const QString &url, int timeoutMs);

void ipcCloseBrowserTabs(const QString &browser, const QList<int> &tabIds);
void ipcRestoreBrowserTabs(const QString &browser, const QByteArray &json);
