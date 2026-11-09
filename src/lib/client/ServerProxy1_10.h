/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_9.h"

//! Proxy for server implementing protocol version 1.10
/*!
Adds the list of this computer's monitors (DMON) and moving apps between computers
(DHRQ, DHST, DHDT).
*/
class ServerProxy1_10 : public ServerProxy1_9
{
public:
  ServerProxy1_10(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_10() override = default;

  void sendMonitors(const deskflow::MonitorList &monitors) override;
  void sendHandoffStart(
      const std::string &target, const std::string &appId, const std::string &url, const std::string &fileName,
      uint32_t size
  ) override;
  void sendHandoffData(const std::string &chunk) override;

protected:
  ConnectionResult parseHandshakeMessage(const uint8_t *code) override;
  ConnectionResult parseMessage(const uint8_t *code) override;

private:
  //! Handle the messages added in 1.10; returns false if \p code isn't one of them.
  bool parseMessage1_10(const uint8_t *code);

  bool m_sentMonitors = false;
};
