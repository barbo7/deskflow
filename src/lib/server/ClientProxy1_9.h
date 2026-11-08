/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_8.h"

#include <string>

//! Proxy for client implementing protocol version 1.9
/*!
Adds the active computer notification (DACT), switch requests from the
client (DSWR) and the client platform announcement (DPLT).
*/
class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  std::string getPlatform() const override;
  void activeComputerChanged(const std::string &active, const std::string &computers) override;
  void sendSwapInfo(uint32_t edges, const std::string &config) override;
  void roleSwapAccepted() override;

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;

private:
  //! Handle the messages added in 1.9; returns false if \p code isn't one of them.
  bool parseMessage1_9(const uint8_t *code);

  std::string m_platform;
};
