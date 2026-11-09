/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ServerProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void handleKeepAliveAlarm_timeout_queuesDisconnectRequest();
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void requestSwitchToComputer_before1_9_sendsNothing();
  void requestSwitchToComputer_1_9_sendsSwitchRequest();
  void parseHandshakeMessage_1_9_sendsPlatformOnce();
  void requestRoleSwap_1_9_sendsSwapRequest();
  void sendMonitors_1_10_sendsMonitors();
  void sendMonitors_before1_10_sendsNothing();
  void sendHandoffData_1_10_sendsChunk();

private:
  Log m_log;
};
