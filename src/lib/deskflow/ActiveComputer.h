/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "base/Event.h"

#include <string>
#include <utility>

namespace deskflow {

//! Platform name of this build, as sent in kMsgDPlatform and kMsgDActiveComputer.
inline std::string localPlatformName()
{
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "mac";
#else
  return "linux";
#endif
}

//! Event data for EventTypes::AppDragPush.
class DragPushInfo : public EventData
{
public:
  explicit DragPushInfo(Direction direction) : m_direction(direction)
  {
    // do nothing
  }
  ~DragPushInfo() override = default;

  Direction m_direction;
};

//! Event data for EventTypes::AppSwitchToComputerRequest.
class SwitchRequestInfo : public EventData
{
public:
  explicit SwitchRequestInfo(std::string computer) : m_computer(std::move(computer))
  {
    // do nothing
  }
  ~SwitchRequestInfo() override = default;

  std::string m_computer;
};

} // namespace deskflow
