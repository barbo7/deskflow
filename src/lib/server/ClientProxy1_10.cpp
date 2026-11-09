/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_10.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_10::ClientProxy1_10(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_9(name, adoptedStream, server, events)
{
  // do nothing
}

deskflow::MonitorList ClientProxy1_10::getMonitors() const
{
  return m_monitors.isEmpty() ? ClientProxy1_9::getMonitors() : m_monitors;
}

void ClientProxy1_10::requestHandoffCapture(const std::string &target)
{
  LOG_DEBUG("ask \"%s\" to send its front app to \"%s\"", getName().c_str(), target.c_str());
  ProtocolUtil::writef(getStream(), kMsgDHandoffRequest, &target);
}

void ClientProxy1_10::requestHandoffDrag(const std::string &target)
{
  LOG_DEBUG("ask \"%s\" to send a dragged window to \"%s\"", getName().c_str(), target.c_str());
  ProtocolUtil::writef(getStream(), kMsgDHandoffDrag, &target);
}

void ClientProxy1_10::sendHandoffStart(
    const std::string &appId, const std::string &url, const std::string &fileName, uint32_t size
)
{
  // the target is the receiver itself here, so it goes out empty
  const std::string target;
  ProtocolUtil::writef(getStream(), kMsgDHandoffStart, &target, &appId, &url, &fileName, size);
}

void ClientProxy1_10::sendHandoffData(const std::string &chunk)
{
  ProtocolUtil::writef(getStream(), kMsgDHandoffData, &chunk);
}

bool ClientProxy1_10::parseHandshakeMessage(const uint8_t *code)
{
  return parseMessage1_10(code) || ClientProxy1_9::parseHandshakeMessage(code);
}

bool ClientProxy1_10::parseMessage(const uint8_t *code)
{
  return parseMessage1_10(code) || ClientProxy1_9::parseMessage(code);
}

bool ClientProxy1_10::parseMessage1_10(const uint8_t *code)
{
  auto *server = getServer();

  if (memcmp(code, kMsgDMonitors, 4) == 0) {
    std::string monitors;
    if (!ProtocolUtil::readf(getStream(), kMsgDMonitors + 4, &monitors)) {
      return false;
    }
    LOG_DEBUG("client \"%s\" monitors: %s", getName().c_str(), monitors.c_str());
    m_monitors = deskflow::parseMonitors(QString::fromStdString(monitors));
    if (server != nullptr) {
      server->notifyMonitors();
    }
    return true;
  }

  if (memcmp(code, kMsgDHandoffStart, 4) == 0) {
    std::string target;
    std::string appId;
    std::string url;
    std::string fileName;
    uint32_t size = 0;
    if (!ProtocolUtil::readf(getStream(), kMsgDHandoffStart + 4, &target, &appId, &url, &fileName, &size)) {
      return false;
    }
    if (server != nullptr) {
      server->handleHandoffStart(this, target, appId, url, fileName, size);
    }
    return true;
  }

  if (memcmp(code, kMsgDHandoffData, 4) == 0) {
    std::string chunk;
    if (!ProtocolUtil::readf(getStream(), kMsgDHandoffData + 4, &chunk)) {
      return false;
    }
    if (server != nullptr) {
      server->handleHandoffData(this, chunk);
    }
    return true;
  }

  return false;
}
