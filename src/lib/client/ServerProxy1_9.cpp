/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "base/Log.h"
#include "client/Client.h"
#include "deskflow/ActiveComputer.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ServerProxy1_9::ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events)
{
  // do nothing. the platform can't be sent here: the hello back hasn't gone out yet.
}

void ServerProxy1_9::requestSwitchToComputer(const std::string &computer)
{
  LOG_DEBUG("asking server to switch to \"%s\"", computer.c_str());
  ProtocolUtil::writef(getStream(), kMsgDSwitchRequest, &computer);
}

ServerProxy::ConnectionResult ServerProxy1_9::parseHandshakeMessage(const uint8_t *code)
{
  // the first message from the server means it has our hello back, so it's
  // now safe to announce our platform.
  if (!m_sentPlatform) {
    m_sentPlatform = true;
    const auto platform = deskflow::localPlatformName();
    ProtocolUtil::writef(getStream(), kMsgDPlatform, &platform);
  }

  if (memcmp(code, kMsgDActiveComputer, 4) == 0) {
    activeComputer();
    return ConnectionResult::Okay;
  }
  return ServerProxy1_8::parseHandshakeMessage(code);
}

ServerProxy::ConnectionResult ServerProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDActiveComputer, 4) == 0) {
    activeComputer();
    return ConnectionResult::Okay;
  }
  return ServerProxy1_8::parseMessage(code);
}

void ServerProxy1_9::activeComputer()
{
  std::string active;
  std::string computers;
  ProtocolUtil::readf(getStream(), kMsgDActiveComputer + 4, &active, &computers);
  LOG_DEBUG("recv active computer: %s (%s)", active.c_str(), computers.c_str());
  getClient()->activeComputerChanged(active, computers);
}
