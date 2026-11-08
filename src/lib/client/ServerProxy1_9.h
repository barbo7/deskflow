/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"

#include <string>

//! Proxy for server implementing protocol version 1.9
/*!
Adds the active computer notification (DACT), switch requests to the
server (DSWR) and the client platform announcement (DPLT).
*/
class ServerProxy1_9 : public ServerProxy1_8
{
public:
  ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_9() override = default;

  void requestSwitchToComputer(const std::string &computer) override;

protected:
  ConnectionResult parseHandshakeMessage(const uint8_t *code) override;
  ConnectionResult parseMessage(const uint8_t *code) override;

private:
  //! Read a DACT message and pass it on to the client.
  void activeComputer();

  bool m_sentPlatform = false;
};
