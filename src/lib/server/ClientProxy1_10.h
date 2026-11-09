/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_9.h"

//! Proxy for client implementing protocol version 1.10
/*!
Adds the list of the client's monitors (DMON) and moving apps between computers
(DHRQ, DHST, DHDT).
*/
class ClientProxy1_10 : public ClientProxy1_9
{
public:
  ClientProxy1_10(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_10() override = default;

  deskflow::MonitorList getMonitors() const override;
  bool supportsHandoff() const override
  {
    return true;
  }
  void requestHandoffCapture(const std::string &target) override;
  void sendHandoffStart(
      const std::string &appId, const std::string &url, const std::string &fileName, uint32_t size
  ) override;
  void sendHandoffData(const std::string &chunk) override;

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;

private:
  //! Handle the messages added in 1.10; returns false if \p code isn't one of them.
  bool parseMessage1_10(const uint8_t *code);

  deskflow::MonitorList m_monitors;
};
