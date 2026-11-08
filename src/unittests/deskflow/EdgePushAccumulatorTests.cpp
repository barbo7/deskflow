/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EdgePushAccumulatorTests.h"

#include "deskflow/EdgePushAccumulator.h"

using deskflow::EdgePushAccumulator;
using namespace std::chrono_literals;

namespace {

const auto kRight = static_cast<uint32_t>(DirectionMask::RightMask);

EdgePushAccumulator::Edges atRight()
{
  EdgePushAccumulator::Edges at;
  at.right = true;
  return at;
}

} // namespace

void EdgePushAccumulatorTests::feed_pushIntoWatchedEdge_triggersAtThreshold()
{
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();

  QVERIFY(!push.feed(40, 0, atRight(), now));
  QVERIFY(!push.feed(40, 0, atRight(), now += 10ms));
  QVERIFY(push.feed(40, 0, atRight(), now += 10ms));
}

void EdgePushAccumulatorTests::feed_pushIntoUnwatchedEdge_neverTriggers()
{
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();
  EdgePushAccumulator::Edges atLeft;
  atLeft.left = true;

  for (int i = 0; i < 10; ++i) {
    QVERIFY(!push.feed(-50, 0, atLeft, now += 10ms));
  }
}

void EdgePushAccumulatorTests::feed_movingAlongEdge_neverTriggers()
{
  // e.g. dragging a scroll bar at the right edge of the screen
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();

  for (int i = 0; i < 10; ++i) {
    QVERIFY(!push.feed(0, 50, atRight(), now += 10ms));
  }
}

void EdgePushAccumulatorTests::feed_awayFromEdge_resetsPush()
{
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();

  QVERIFY(!push.feed(60, 0, atRight(), now += 10ms));
  QVERIFY(!push.feed(-5, 0, atRight(), now += 10ms));
  QVERIFY(!push.feed(60, 0, atRight(), now += 10ms));
  QVERIFY(push.feed(60, 0, atRight(), now += 10ms));
}

void EdgePushAccumulatorTests::feed_pause_resetsPush()
{
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();

  QVERIFY(!push.feed(60, 0, atRight(), now));
  QVERIFY(!push.feed(60, 0, atRight(), now += 1s));
  QVERIFY(push.feed(60, 0, atRight(), now += 10ms));
}

void EdgePushAccumulatorTests::feed_afterTrigger_staysQuiet()
{
  EdgePushAccumulator push(kRight, 100);
  auto now = EdgePushAccumulator::Clock::now();

  QVERIFY(push.feed(200, 0, atRight(), now));
  QVERIFY(!push.feed(200, 0, atRight(), now += 1s));
  QVERIFY(push.feed(200, 0, atRight(), now += 5s));
}

QTEST_MAIN(EdgePushAccumulatorTests)
