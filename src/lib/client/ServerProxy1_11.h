/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_10.h"

//! Proxy for server implementing protocol version 1.11
/*!
Adds the answer to moving an app: whether it opened on the other computer (DHAK).
*/
class ServerProxy1_11 : public ServerProxy1_10
{
public:
  ServerProxy1_11(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_11() override = default;

  void sendHandoffAck(bool opened) override;

protected:
  ConnectionResult parseHandshakeMessage(const uint8_t *code) override;
  ConnectionResult parseMessage(const uint8_t *code) override;

private:
  //! Handle the messages added in 1.11; returns false if \p code isn't one of them.
  bool parseMessage1_11(const uint8_t *code);
};
