/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorLayoutTests.h"

#include "common/MonitorLayout.h"

using namespace deskflow;

namespace {

ComputerPlacement computer(const QString &name, QPoint pos, const MonitorList &monitors)
{
  return {name, pos, monitors};
}

QStringList linesFrom(const QList<DisplayLink> &links, const QString &name)
{
  QStringList lines;
  for (const auto &link : links) {
    if (link.from == name) {
      lines.append(link.toConfigLine());
    }
  }
  return lines;
}

} // namespace

void MonitorLayoutTests::monitors_roundTrip()
{
  const MonitorList monitors = {QRect(0, 0, 1920, 1080), QRect(-1440, -200, 1440, 900)};
  QCOMPARE(parseMonitors(formatMonitors(monitors)), monitors);
}

void MonitorLayoutTests::monitors_skipsMalformed()
{
  const MonitorList expected = {QRect(1, 2, 3, 4)};
  QCOMPARE(parseMonitors(QStringLiteral("a,b,c,d;1,2,3;1,2,3,4;5,5,0,10;5,5,-1,10")), expected);
}

void MonitorLayoutTests::monitorMap_roundTrip()
{
  const MonitorMap map = {{QStringLiteral("mac.local"), {QRect(0, 0, 100, 50)}}, {QStringLiteral("pc"), {QRect(0, 0, 10, 10), QRect(10, 0, 10, 10)}}};
  QCOMPARE(parseMonitorMap(formatMonitorMap(map)), map);
}

void MonitorLayoutTests::normalized_movesToOrigin()
{
  const MonitorList expected = {QRect(1440, 200, 1920, 1080), QRect(0, 0, 1440, 900)};
  QCOMPARE(normalizedMonitors({QRect(0, 0, 1920, 1080), QRect(-1440, -200, 1440, 900)}), expected);
}

void MonitorLayoutTests::links_sideBySide_wholeEdges()
{
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {1000, 0}, {QRect(0, 0, 800, 500)}),
  });
  QCOMPARE(linesFrom(links, QStringLiteral("a")), QStringList{QStringLiteral("right = b")});
  QCOMPARE(linesFrom(links, QStringLiteral("b")), QStringList{QStringLiteral("left = a")});
}

void MonitorLayoutTests::links_partialOverlap_usesPercentages()
{
  // b is half as tall and sits against the lower half of a
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 1000)}),
      computer(QStringLiteral("b"), {1000, 500}, {QRect(0, 0, 800, 500)}),
  });
  QCOMPARE(linesFrom(links, QStringLiteral("a")), QStringList{QStringLiteral("right(50,100) = b")});
  QCOMPARE(linesFrom(links, QStringLiteral("b")), QStringList{QStringLiteral("left = a(50,100)")});
}

void MonitorLayoutTests::links_notTouching_none()
{
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {1100, 0}, {QRect(0, 0, 800, 500)}),
  });
  QVERIFY(links.isEmpty());
}

void MonitorLayoutTests::links_deadZone_noLinkWithoutMonitor()
{
  // a is L shaped: a wide monitor on top of a narrow one. the lower half of its right edge
  // has no monitor behind it, so the cursor can only cross in the top half
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 2000, 1000), QRect(0, 1000, 1000, 1000)}),
      computer(QStringLiteral("b"), {2000, 0}, {QRect(0, 0, 1000, 2000)}),
  });
  QCOMPARE(linesFrom(links, QStringLiteral("a")), QStringList{QStringLiteral("right(0,50) = b(0,50)")});
  QCOMPARE(linesFrom(links, QStringLiteral("b")), QStringList{QStringLiteral("left(0,50) = a(0,50)")});
}

void MonitorLayoutTests::links_stackedMonitors_mergeIntoOneSpan()
{
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500), QRect(0, 500, 1000, 500)}),
      computer(QStringLiteral("b"), {1000, 0}, {QRect(0, 0, 1000, 1000)}),
  });
  QCOMPARE(linesFrom(links, QStringLiteral("a")), QStringList{QStringLiteral("right = b")});
}

void MonitorLayoutTests::links_twoNeighboursOnOneSide()
{
  const auto links = computeDisplayLinks({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 1000)}),
      computer(QStringLiteral("b"), {1000, 0}, {QRect(0, 0, 500, 500)}),
      computer(QStringLiteral("c"), {1000, 500}, {QRect(0, 0, 500, 500)}),
  });
  const QStringList expected = {QStringLiteral("right(0,50) = b"), QStringLiteral("right(50,100) = c")};
  QCOMPARE(linesFrom(links, QStringLiteral("a")), expected);
}

void MonitorLayoutTests::snap_pullsOntoEdge()
{
  const PlacementList placements = {
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {1030, 15}, {QRect(0, 0, 800, 500)}),
  };
  QCOMPARE(snapPlacement(placements, 1, {1030, 15}, 50), QPoint(1000, 0));
}

void MonitorLayoutTests::snap_farAway_unchanged()
{
  const PlacementList placements = {
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {3000, 3000}, {QRect(0, 0, 800, 500)}),
  };
  QCOMPARE(snapPlacement(placements, 1, {3000, 3000}, 50), QPoint(3000, 3000));
}

void MonitorLayoutTests::overlaps_detected()
{
  const PlacementList placements = {
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {1000, 0}, {QRect(0, 0, 800, 500)}),
  };
  QVERIFY(!placementOverlaps(placements, 1, {1000, 0}));
  QVERIFY(placementOverlaps(placements, 1, {999, 0}));
}

void MonitorLayoutTests::unreachable_listed()
{
  const PlacementList placements = {
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 1000, 500)}),
      computer(QStringLiteral("b"), {1000, 0}, {QRect(0, 0, 800, 500)}),
      computer(QStringLiteral("c"), {5000, 0}, {QRect(0, 0, 800, 500)}),
  };
  QCOMPARE(unreachableComputers(placements), QStringList{QStringLiteral("c")});
}

void MonitorLayoutTests::withoutOverlaps_movesLaterComputersRight()
{
  const auto result = withoutOverlaps({
      computer(QStringLiteral("a"), {0, 0}, {QRect(0, 0, 3840, 1080)}),
      computer(QStringLiteral("b"), {1920, 0}, {QRect(0, 0, 2560, 1440)}),
      computer(QStringLiteral("c"), {4000, 0}, {QRect(0, 0, 1000, 1000)}),
  });
  QCOMPARE(result.at(0).pos, QPoint(0, 0));
  QCOMPARE(result.at(1).pos, QPoint(3840, 0));
  QCOMPARE(result.at(2).pos, QPoint(6400, 0));
}

QTEST_MAIN(MonitorLayoutTests)
