/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>

namespace deskflow::core::ipc {

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(QObject *parent) : IpcServer(parent, kCoreIpcName, QStringLiteral("core"))
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  if (command == QStringLiteral("switchTo")) {
    if (parts.size() < 2 || parts.at(1).isEmpty()) {
      LOG_WARN("core ipc server got switchTo without a computer name");
      return;
    }
    LOG_DEBUG("core ipc server got switch request: %s", parts.at(1).toUtf8().constData());
    Q_EMIT switchToComputerRequested(parts.at(1));
    return;
  }
  if (command == QStringLiteral("getActiveComputer")) {
    writeToClientSocket(clientSocket, activeComputerMessage());
    return;
  }
  if (command == QStringLiteral("getMonitors")) {
    writeToClientSocket(clientSocket, QStringLiteral("monitors=%1").arg(m_monitors));
    return;
  }
  if (command == QStringLiteral("reloadConfig")) {
    LOG_DEBUG("core ipc server got reload config request");
    Q_EMIT reloadConfigRequested();
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

void CoreIpcServer::setActiveComputer(const QString &active, const QString &computers)
{
  m_activeComputer = active;
  m_computers = computers;
  const auto message = activeComputerMessage();
  const auto equals = message.indexOf('=');
  broadcastCommand(message.left(equals), message.mid(equals + 1));
}

void CoreIpcServer::setMonitors(const QString &monitors)
{
  m_monitors = monitors;
  broadcastCommand(QStringLiteral("monitors"), monitors);
}

QString CoreIpcServer::activeComputerMessage() const
{
  // "activeComputer=<active>;<name|platform>,<name|platform>"
  return QStringLiteral("activeComputer=%1;%2").arg(m_activeComputer, m_computers);
}

} // namespace deskflow::core::ipc
