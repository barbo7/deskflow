/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

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
  using DragCallback = std::function<void(Direction)>;

  virtual ~EdgePushDetector() = default;

  /**
   * \param edges DirectionMask of the edges to watch.
   * \param onPush Called (from a worker thread) when the mouse is pushed past one of them.
   * \param onInputBlocked Called (from a worker thread) when this computer switches to a screen
   *        deskflow can't control, such as an elevation prompt; may not be supported everywhere.
   * \param onHandoff Called (from a worker thread) when ctrl+alt+super+H is pressed on this
   *        computer's own keyboard.
   * \param dragEdges DirectionMask of the edges where pushing with the left button held counts.
   * \param onDragPush Called (from a worker thread) with the edge pushed against.
   * \return nullptr if this platform can't watch the mouse.
   *
   * A push with the left button held is never a role swap. Pass 0 and empty callbacks for what
   * isn't wanted: a server watches only for drags, and has hotkeys of its own.
   */
  static std::unique_ptr<EdgePushDetector> create(
      uint32_t edges, Callback onPush, Callback onInputBlocked, Callback onHandoff, uint32_t dragEdges,
      DragCallback onDragPush
  );
};

} // namespace deskflow
