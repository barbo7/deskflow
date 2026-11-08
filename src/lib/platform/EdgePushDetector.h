/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace deskflow {

/**
 * Watches the user's own (physical) mouse on a client and calls back when it is pushed
 * against an edge that leads to the server. Input that Deskflow itself injects is ignored.
 *
 * Runs on its own thread; the callback is called from that thread.
 */
class EdgePushDetector
{
public:
  using Callback = std::function<void()>;

  virtual ~EdgePushDetector() = default;

  /**
   * \param edges DirectionMask of the edges to watch.
   * \param onPush Called (from a worker thread) when the mouse is pushed past one of them.
   * \return nullptr if this platform can't watch the mouse.
   */
  static std::unique_ptr<EdgePushDetector> create(uint32_t edges, Callback onPush);
};

} // namespace deskflow
