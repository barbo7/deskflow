/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_10.h"

#include "base/Log.h"
#include "client/Client.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ServerProxy1_10::ServerProxy1_10(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_9(client, stream, events)
{
  // do nothing. the monitors can't be sent here: the hello back hasn't gone out yet.
}

void ServerProxy1_10::sendMonitors(const deskflow::MonitorList &monitors)
{
  const auto text = deskflow::formatMonitors(monitors).toStdString();
  LOG_DEBUG("sending monitors: %s", text.c_str());
  ProtocolUtil::writef(getStream(), kMsgDMonitors, &text);
}

void ServerProxy1_10::sendHandoffStart(
    const std::string &target, const std::string &appId, const std::string &url, const std::string &fileName,
    uint32_t size
)
{
  ProtocolUtil::writef(getStream(), kMsgDHandoffStart, &target, &appId, &url, &fileName, size);
}

void ServerProxy1_10::sendHandoffData(const std::string &chunk)
{
  ProtocolUtil::writef(getStream(), kMsgDHandoffData, &chunk);
}

ServerProxy::ConnectionResult ServerProxy1_10::parseHandshakeMessage(const uint8_t *code)
{
  // the first message from the server means it has our hello back
  if (!m_sentMonitors) {
    m_sentMonitors = true;
    sendMonitors(getClient()->getMonitors());
  }

  if (parseMessage1_10(code)) {
    return ConnectionResult::Okay;
  }
  return ServerProxy1_9::parseHandshakeMessage(code);
}

ServerProxy::ConnectionResult ServerProxy1_10::parseMessage(const uint8_t *code)
{
  if (parseMessage1_10(code)) {
    return ConnectionResult::Okay;
  }
  return ServerProxy1_9::parseMessage(code);
}

bool ServerProxy1_10::parseMessage1_10(const uint8_t *code)
{
  if (memcmp(code, kMsgDHandoffRequest, 4) == 0) {
    std::string target;
    ProtocolUtil::readf(getStream(), kMsgDHandoffRequest + 4, &target);
    getClient()->captureForHandoff(target);
    return true;
  }

  if (memcmp(code, kMsgDHandoffStart, 4) == 0) {
    std::string target;
    std::string appId;
    std::string url;
    std::string fileName;
    uint32_t size = 0;
    ProtocolUtil::readf(getStream(), kMsgDHandoffStart + 4, &target, &appId, &url, &fileName, &size);
    getClient()->handoffStart(appId, url, fileName, size);
    return true;
  }

  if (memcmp(code, kMsgDHandoffData, 4) == 0) {
    std::string chunk;
    ProtocolUtil::readf(getStream(), kMsgDHandoffData + 4, &chunk);
    getClient()->handoffData(chunk);
    return true;
  }

  return false;
}
