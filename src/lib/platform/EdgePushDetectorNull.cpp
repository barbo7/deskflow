/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/EdgePushDetector.h"

namespace deskflow {

std::unique_ptr<EdgePushDetector> EdgePushDetector::create(uint32_t, Callback)
{
  // not implemented on this platform yet
  return nullptr;
}

} // namespace deskflow
