/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_10.h"

//! Proxy for client implementing protocol version 1.11
/*!
Adds the answer to moving an app: whether it opened on the other computer (DHAK).
*/
class ClientProxy1_11 : public ClientProxy1_10
{
public:
  ClientProxy1_11(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_11() override = default;

  void sendHandoffAck(bool opened) override;

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;

private:
  //! Handle the messages added in 1.11; returns false if \p code isn't one of them.
  bool parseMessage1_11(const uint8_t *code);
};
