/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpc.h"

#include "CoreIpcServer.h"

#include <QMetaEnum>

#include <chrono>
#include <future>
#include <memory>

void ipcSendToClient(const QString &command, const QString &args)
{
  // Queued because callers may not be on the main thread,
  // and QLocalSocket can only be written to from its owning thread.
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [command, args] { deskflow::core::ipc::CoreIpcServer::instance().broadcastCommand(command, args); },
      Qt::QueuedConnection
  );
}

void ipcSendConnectionState(deskflow::core::ConnectionState state)
{
  const auto metaEnum = QMetaEnum::fromType<deskflow::core::ConnectionState>();
  const QString key = QString::fromLatin1(metaEnum.valueToKey(static_cast<int>(state)));
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [key] { deskflow::core::ipc::CoreIpcServer::instance().setConnectionState(key); }, Qt::QueuedConnection
  );
}

void ipcSendActiveComputer(const QString &active, const QString &computers)
{
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server,
      [active, computers] { deskflow::core::ipc::CoreIpcServer::instance().setActiveComputer(active, computers); },
      Qt::QueuedConnection
  );
}

void ipcSendMonitors(const QString &monitors)
{
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [monitors] { deskflow::core::ipc::CoreIpcServer::instance().setMonitors(monitors); },
      Qt::QueuedConnection
  );
}

bool ipcHasBrowserHost(const QString &browser)
{
  return deskflow::core::ipc::CoreIpcServer::instance().hasBrowserHost(browser);
}

QByteArray ipcCaptureBrowserState(const QString &browser, const QString &mode, const QString &url, int timeoutMs)
{
  auto reply = std::make_shared<std::promise<QByteArray>>();
  auto answer = reply->get_future();

  // not a blocking call into the server's thread: at shutdown that thread waits for this one
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server,
      [browser, mode, url, reply] {
        deskflow::core::ipc::CoreIpcServer::instance().requestBrowserState(browser, mode, url, reply);
      },
      Qt::QueuedConnection
  );

  if (answer.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready) {
    return {};
  }
  return answer.get();
}

void ipcCloseBrowserTabs(const QString &browser, const QList<int> &tabIds)
{
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [browser, tabIds] { deskflow::core::ipc::CoreIpcServer::instance().closeBrowserTabs(browser, tabIds); },
      Qt::QueuedConnection
  );
}

void ipcRestoreBrowserTabs(const QString &browser, const QByteArray &json)
{
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [browser, json] { deskflow::core::ipc::CoreIpcServer::instance().restoreBrowserTabs(browser, json); },
      Qt::QueuedConnection
  );
}
