/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_11.h"

#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_11::ClientProxy1_11(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_10(name, adoptedStream, server, events)
{
  // do nothing
}

void ClientProxy1_11::sendHandoffAck(bool opened)
{
  ProtocolUtil::writef(getStream(), kMsgDHandoffAck, static_cast<uint8_t>(opened ? 1 : 0));
}

bool ClientProxy1_11::parseHandshakeMessage(const uint8_t *code)
{
  return parseMessage1_11(code) || ClientProxy1_10::parseHandshakeMessage(code);
}

bool ClientProxy1_11::parseMessage(const uint8_t *code)
{
  return parseMessage1_11(code) || ClientProxy1_10::parseMessage(code);
}

bool ClientProxy1_11::parseMessage1_11(const uint8_t *code)
{
  if (memcmp(code, kMsgDHandoffAck, 4) != 0) {
    return false;
  }

  uint8_t opened = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgDHandoffAck + 4, &opened)) {
    return false;
  }
  if (auto *server = getServer(); server != nullptr) {
    server->handleHandoffAck(this, opened != 0);
  }
  return true;
}
