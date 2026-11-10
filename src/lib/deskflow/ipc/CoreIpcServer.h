/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>

#include <future>
#include <map>
#include <memory>
#include <mutex>

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
  void setMonitors(const QString &monitors);

  /**
   * Remember and broadcast the connection state. Every client gets it again when it says hello:
   * a broadcast reaches only the clients connected at the time, and the queue for when there are
   * none goes to whichever says hello first, which can be a browser extension, not the gui.
   */
  void setConnectionState(const QString &state);

  //! Whether the extension of \p browser is connected. Can be called from any thread.
  bool hasBrowserHost(const QString &browser) const;

  // these three only on the thread of this object

  /**
   * Asks the extension of \p browser for the state of its pages; answers through \p reply.
   * \p url is the address the window being sent shows: the window the browser last focused
   * isn't always the one being dragged.
   */
  void requestBrowserState(
      const QString &browser, const QString &mode, const QString &url,
      const std::shared_ptr<std::promise<QByteArray>> &reply
  );
  void closeBrowserTabs(const QString &browser, const QList<int> &tabIds);
  void restoreBrowserTabs(const QString &browser, const QByteArray &json);

Q_SIGNALS:
  void switchToComputerRequested(const QString &computer);
  void reloadConfigRequested();

private:
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;
  bool wantsBroadcasts(QLocalSocket *clientSocket) const override;
  void clientGreeted(QLocalSocket *clientSocket) override;
  void clientDisconnected(QLocalSocket *clientSocket) override;
  QLocalSocket *browserHost(const QString &browser) const;

  QString activeComputerMessage() const;

  QString m_activeComputer;
  QString m_computers;
  QString m_monitors;
  QString m_connectionState;

  mutable std::mutex m_browserHostsMutex;
  QHash<QLocalSocket *, QString> m_browserHosts;
  std::map<int, std::shared_ptr<std::promise<QByteArray>>> m_browserRequests;
  int m_nextBrowserRequest = 1;
};

} // namespace deskflow::core::ipc
