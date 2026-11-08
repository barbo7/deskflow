/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

#include <QString>

void ipcSendToClient(const QString &command, const QString &args = "");
void ipcSendConnectionState(deskflow::core::ConnectionState state);

/**
 * Tell the gui which computer has the cursor, and which computers are connected.
 * \param active Name of the computer that has the cursor (empty if unknown).
 * \param computers Connected computers as "name|platform" pairs joined by ",".
 */
void ipcSendActiveComputer(const QString &active, const QString &computers);
