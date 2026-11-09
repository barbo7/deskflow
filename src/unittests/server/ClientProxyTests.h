/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ClientProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void keyDown_data();
  void keyDown();
  void keyRepeat_data();
  void keyRepeat();
  void keyUp_data();
  void keyUp();
  void activeComputerChanged_data();
  void activeComputerChanged();
  void sendHandoffStart_data();
  void sendHandoffStart();
  void sendHandoffData_data();
  void sendHandoffData();
  void requestHandoffCapture_data();
  void requestHandoffCapture();
  void supportsHandoff_from1_10();
  void sendSwapInfo_data();
  void sendSwapInfo();
  void roleSwapAccepted_data();
  void roleSwapAccepted();

private:
  Log m_log;
};
