/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"

#include <QObject>
#include <QSet>

class QLocalSocket;

namespace deskflow::core::ipc {

class CoreIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit CoreIpcServer(QObject *parent);

  static CoreIpcServer &instance();

  /**
   * Remember and broadcast the active computer, so a gui that connects later
   * can ask for it with `getActiveComputer`.
   */
  void setActiveComputer(const QString &active, const QString &computers);

Q_SIGNALS:
  void switchToComputerRequested(const QString &computer);

private:
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;

  QString activeComputerMessage() const;

  QString m_activeComputer;
  QString m_computers;
};

} // namespace deskflow::core::ipc
