/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
  // do nothing
}

std::string ClientProxy1_9::getPlatform() const
{
  return m_platform;
}

void ClientProxy1_9::activeComputerChanged(const std::string &active, const std::string &computers)
{
  LOG_DEBUG("send active computer to \"%s\": active=%s", getName().c_str(), active.c_str());
  ProtocolUtil::writef(getStream(), kMsgDActiveComputer, &active, &computers);
}

bool ClientProxy1_9::parseHandshakeMessage(const uint8_t *code)
{
  return parseMessage1_9(code) || ClientProxy1_8::parseHandshakeMessage(code);
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  return parseMessage1_9(code) || ClientProxy1_8::parseMessage(code);
}

bool ClientProxy1_9::parseMessage1_9(const uint8_t *code)
{
  if (memcmp(code, kMsgDPlatform, 4) == 0) {
    std::string platform;
    if (!ProtocolUtil::readf(getStream(), kMsgDPlatform + 4, &platform)) {
      return false;
    }
    LOG_DEBUG("client \"%s\" platform: %s", getName().c_str(), platform.c_str());
    m_platform = platform;
    if (auto *server = getServer(); server != nullptr) {
      // the platform is part of the computer list, so resend it
      server->notifyActiveComputer();
    }
    return true;
  }

  if (memcmp(code, kMsgDSwitchRequest, 4) == 0) {
    std::string computer;
    if (!ProtocolUtil::readf(getStream(), kMsgDSwitchRequest + 4, &computer)) {
      return false;
    }
    LOG_INFO("client \"%s\" asked to switch to \"%s\"", getName().c_str(), computer.c_str());
    if (auto *server = getServer(); server != nullptr) {
      server->switchToComputerByName(computer);
    }
    return true;
  }

  return false;
}
