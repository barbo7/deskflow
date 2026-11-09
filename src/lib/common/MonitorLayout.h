/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

namespace deskflow {

//! The physical monitors of one computer, in that computer's own desktop coordinates.
using MonitorList = QList<QRect>;

//! Monitors of each computer, by computer name.
using MonitorMap = QMap<QString, MonitorList>;

//! "x,y,w,h;x,y,w,h"
QString formatMonitors(const MonitorList &monitors);

//! Reads what formatMonitors() wrote. Malformed or empty rectangles are skipped.
MonitorList parseMonitors(const QString &text);

//! "name:x,y,w,h;x,y,w,h|name:x,y,w,h" (no "=": the ipc channel splits on it)
QString formatMonitorMap(const MonitorMap &map);
MonitorMap parseMonitorMap(const QString &text);

//! Moves monitors so that the top left of the area they cover is (0,0).
MonitorList normalizedMonitors(const MonitorList &monitors);

//! Where one computer sits in the layout editor.
struct ComputerPlacement
{
  QString name;
  //! Top left of the computer's desktop, in layout coordinates.
  QPoint pos;
  //! The computer's monitors, normalized; see normalizedMonitors().
  MonitorList monitors;

  //! The area this computer covers, in layout coordinates.
  [[nodiscard]] QRect bounds() const;

  bool operator==(const ComputerPlacement &other) const = default;
};

using PlacementList = QList<ComputerPlacement>;

//! One edge link in the server configuration, percentages along each computer's edge.
struct DisplayLink
{
  QString from;
  QString side; // left, right, up or down, as in the configuration file
  int fromStart = 0;
  int fromEnd = 100;
  QString to;
  int toStart = 0;
  int toEnd = 100;

  //! "right(0,50) = other(25,75)"; a whole edge has no interval.
  [[nodiscard]] QString toConfigLine() const;

  bool operator==(const DisplayLink &other) const = default;
};

/**
 * Works out the edge links for a layout.
 *
 * A link exists only where a monitor really reaches the outer edge of its computer's desktop:
 * the cursor can never reach a stretch of edge with no monitor behind it.
 */
QList<DisplayLink> computeDisplayLinks(const PlacementList &placements);

//! Whether computer \p index would cover part of another computer if placed at \p pos.
bool placementOverlaps(const PlacementList &placements, int index, const QPoint &pos);

//! Pulls \p pos onto the edges of the other computers when it's within \p tolerance of them.
QPoint snapPlacement(const PlacementList &placements, int index, const QPoint &pos, int tolerance);

//! Moves computers to the right until none covers another, as can happen when a computer turns
//! out to have more or larger monitors than it was placed with. Earlier computers stay put.
PlacementList withoutOverlaps(const PlacementList &placements);

//! Moves computers so that those touching in \p before still touch in \p after, as can stop being
//! the case when a computer turns out to have smaller or larger monitors than it was placed with.
//! Computers are matched by name; those that didn't touch anything stay where they are.
PlacementList keepNeighboursTouching(const PlacementList &before, const PlacementList &after);

//! Computers that have no link to any other computer, so the cursor could never reach them.
QStringList unreachableComputers(const PlacementList &placements);

} // namespace deskflow
