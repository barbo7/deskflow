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
  if (command == QStringLiteral("browserHost")) {
    // said before hello, so its hello doesn't take the messages queued for the gui
    const std::scoped_lock lock(m_browserHostsMutex);
    if (!m_browserHosts.contains(clientSocket)) {
      m_browserHosts.insert(clientSocket, QString());
    }
    return;
  }
  if (command == QStringLiteral("browserHello")) {
    LOG_INFO("browser extension connected: %s", parts.value(1).toUtf8().constData());
    const std::scoped_lock lock(m_browserHostsMutex);
    m_browserHosts.insert(clientSocket, parts.value(1));
    return;
  }
  if (command == QStringLiteral("browserCaptured")) {
    const auto id = parts.value(1).section(QLatin1Char(';'), 0, 0).toInt();
    if (const auto request = m_browserRequests.find(id); request != m_browserRequests.end()) {
      request->second->set_value(QByteArray::fromBase64(
          parts.value(1).section(QLatin1Char(';'), 1).toLatin1(),
          QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals
      ));
      m_browserRequests.erase(request);
    }
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

void CoreIpcServer::setConnectionState(const QString &state)
{
  m_connectionState = state;
  broadcastCommand(QStringLiteral("connectionState"), state);
}

bool CoreIpcServer::wantsBroadcasts(QLocalSocket *clientSocket) const
{
  const std::scoped_lock lock(m_browserHostsMutex);
  return !m_browserHosts.contains(clientSocket);
}

void CoreIpcServer::clientGreeted(QLocalSocket *clientSocket)
{
  if (!m_connectionState.isEmpty()) {
    writeToClientSocket(clientSocket, QStringLiteral("connectionState=%1").arg(m_connectionState));
  }
}

void CoreIpcServer::clientDisconnected(QLocalSocket *clientSocket)
{
  const std::scoped_lock lock(m_browserHostsMutex);
  m_browserHosts.remove(clientSocket);
}

bool CoreIpcServer::hasBrowserHost(const QString &browser) const
{
  return browserHost(browser) != nullptr;
}

QLocalSocket *CoreIpcServer::browserHost(const QString &browser) const
{
  const std::scoped_lock lock(m_browserHostsMutex);
  for (auto host = m_browserHosts.cbegin(); host != m_browserHosts.cend(); ++host) {
    if (host.value() == browser) {
      return host.key();
    }
  }
  return nullptr;
}

void CoreIpcServer::requestBrowserState(
    const QString &browser, const QString &mode, const QString &url,
    const std::shared_ptr<std::promise<QByteArray>> &reply
)
{
  auto *socket = browserHost(browser);
  if (socket == nullptr) {
    LOG_WARN("no browser extension connected for %s", qPrintable(browser));
    reply->set_value({});
    return;
  }
  // whoever asked before has given up by now
  m_browserRequests.clear();
  const int id = m_nextBrowserRequest++;
  m_browserRequests.emplace(id, reply);
  const auto encodedUrl = url.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  writeToClientSocket(
      socket, QStringLiteral("browserCapture=%1;%2;%3").arg(id).arg(mode, QString::fromLatin1(encodedUrl))
  );
  socket->flush();
}

void CoreIpcServer::closeBrowserTabs(const QString &browser, const QList<int> &tabIds)
{
  if (auto *socket = browserHost(browser); socket != nullptr) {
    QStringList ids;
    for (const int id : tabIds) {
      ids.append(QString::number(id));
    }
    writeToClientSocket(socket, QStringLiteral("browserClose=%1").arg(ids.join(QLatin1Char(','))));
    socket->flush();
  }
}

void CoreIpcServer::restoreBrowserTabs(const QString &browser, const QByteArray &json)
{
  if (auto *socket = browserHost(browser); socket != nullptr) {
    const auto encoded = json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    writeToClientSocket(socket, QStringLiteral("browserRestore=%1").arg(QString::fromLatin1(encoded)));
    socket->flush();
  }
}

QString CoreIpcServer::activeComputerMessage() const
{
  // "activeComputer=<active>;<name|platform>,<name|platform>"
  return QStringLiteral("activeComputer=%1;%2").arg(m_activeComputer, m_computers);
}

} // namespace deskflow::core::ipc
