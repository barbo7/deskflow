/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <chrono>
#include <cstdint>

namespace deskflow {

//! DirectionMask of every edge.
inline constexpr uint32_t kAllEdgesMask =
    static_cast<uint32_t>(DirectionMask::LeftMask) | static_cast<uint32_t>(DirectionMask::RightMask) |
    static_cast<uint32_t>(DirectionMask::TopMask) | static_cast<uint32_t>(DirectionMask::BottomMask);

/**
 * Decides when the user's own mouse has been pushed hard enough against an edge.
 *
 * Feed it every physical mouse movement. Movement counts toward a push only while
 * the cursor sits on a watched edge and moves into it; anything else, or a pause
 * longer than the window, starts the count again. After a push it stays quiet for
 * a cooldown, so one long push gives one swap.
 */
class EdgePushAccumulator
{
public:
  using Clock = std::chrono::steady_clock;

  /**
   * \param edges DirectionMask of the edges to watch.
   * \param threshold How far the mouse must push past the edge (in the platform's units).
   */
  explicit EdgePushAccumulator(uint32_t edges, double threshold) : m_edges(edges), m_threshold(threshold)
  {
    // do nothing
  }

  struct Edges
  {
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;
  };

  //! The edge a movement is pushing into, if any.
  static Direction pushDirection(double dx, double dy, const Edges &at)
  {
    if (at.left && dx < 0) {
      return Direction::Left;
    }
    if (at.right && dx > 0) {
      return Direction::Right;
    }
    if (at.top && dy < 0) {
      return Direction::Top;
    }
    if (at.bottom && dy > 0) {
      return Direction::Bottom;
    }
    return Direction::NoDirection;
  }

  //! Returns true when this movement completes a push.
  bool feed(double dx, double dy, const Edges &at, Clock::time_point now)
  {
    if (now < m_quietUntil) {
      return false;
    }

    double push = 0;
    if (watches(DirectionMask::LeftMask) && at.left && dx < 0) {
      push = -dx;
    } else if (watches(DirectionMask::RightMask) && at.right && dx > 0) {
      push = dx;
    } else if (watches(DirectionMask::TopMask) && at.top && dy < 0) {
      push = -dy;
    } else if (watches(DirectionMask::BottomMask) && at.bottom && dy > 0) {
      push = dy;
    }

    if (push <= 0) {
      m_pushed = 0;
      return false;
    }

    if (now - m_last > kWindow) {
      m_pushed = 0;
    }
    m_last = now;
    m_pushed += push;

    if (m_pushed < m_threshold) {
      return false;
    }

    m_pushed = 0;
    m_quietUntil = now + kCooldown;
    return true;
  }

private:
  static constexpr auto kWindow = std::chrono::milliseconds(400);
  static constexpr auto kCooldown = std::chrono::seconds(5);

  bool watches(DirectionMask mask) const
  {
    return (m_edges & static_cast<uint32_t>(mask)) != 0;
  }

  uint32_t m_edges;
  double m_threshold;
  double m_pushed = 0;
  Clock::time_point m_last;
  Clock::time_point m_quietUntil;
};

} // namespace deskflow
