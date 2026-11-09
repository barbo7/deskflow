/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_11.h"

#include "client/Client.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ServerProxy1_11::ServerProxy1_11(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_10(client, stream, events)
{
  // do nothing
}

void ServerProxy1_11::sendHandoffAck(bool opened)
{
  ProtocolUtil::writef(getStream(), kMsgDHandoffAck, static_cast<uint8_t>(opened ? 1 : 0));
}

ServerProxy::ConnectionResult ServerProxy1_11::parseHandshakeMessage(const uint8_t *code)
{
  if (parseMessage1_11(code)) {
    return ConnectionResult::Okay;
  }
  return ServerProxy1_10::parseHandshakeMessage(code);
}

ServerProxy::ConnectionResult ServerProxy1_11::parseMessage(const uint8_t *code)
{
  if (parseMessage1_11(code)) {
    return ConnectionResult::Okay;
  }
  return ServerProxy1_10::parseMessage(code);
}

bool ServerProxy1_11::parseMessage1_11(const uint8_t *code)
{
  if (memcmp(code, kMsgDHandoffAck, 4) != 0) {
    return false;
  }

  uint8_t opened = 0;
  ProtocolUtil::readf(getStream(), kMsgDHandoffAck + 4, &opened);
  getClient()->handoffDelivered(opened != 0);
  return true;
}
