/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class MonitorLayoutTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void monitors_roundTrip();
  void monitors_skipsMalformed();
  void monitorMap_roundTrip();
  void normalized_movesToOrigin();
  void links_sideBySide_wholeEdges();
  void links_partialOverlap_usesPercentages();
  void links_notTouching_none();
  void links_deadZone_noLinkWithoutMonitor();
  void links_stackedMonitors_mergeIntoOneSpan();
  void links_twoNeighboursOnOneSide();
  void snap_pullsOntoEdge();
  void snap_farAway_unchanged();
  void overlaps_detected();
  void unreachable_listed();
  void withoutOverlaps_movesLaterComputersRight();
};
