/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/MonitorLayout.h"

#include <QtMath>

#include <algorithm>
#include <limits>
#include <optional>

namespace deskflow {

namespace {

// far more than anyone has; stops a bad peer from sending an endless list
const int kMaxMonitors = 32;

// computers closer than this (in layout units) count as touching
const int kTouchTolerance = 2;

struct Span
{
  int start;
  int end;
};

enum class Side
{
  Left,
  Right,
  Up,
  Down
};

const Side kSides[] = {Side::Left, Side::Right, Side::Up, Side::Down};

QString sideName(Side side)
{
  switch (side) {
  case Side::Left:
    return QStringLiteral("left");
  case Side::Right:
    return QStringLiteral("right");
  case Side::Up:
    return QStringLiteral("up");
  default:
    return QStringLiteral("down");
  }
}

Side opposite(Side side)
{
  switch (side) {
  case Side::Left:
    return Side::Right;
  case Side::Right:
    return Side::Left;
  case Side::Up:
    return Side::Down;
  default:
    return Side::Up;
  }
}

bool isHorizontal(Side side)
{
  return side == Side::Left || side == Side::Right;
}

// the coordinate of an edge. QRect::right() is width-1, so it's not used here
int edgeOf(const QRect &r, Side side)
{
  switch (side) {
  case Side::Left:
    return r.x();
  case Side::Right:
    return r.x() + r.width();
  case Side::Up:
    return r.y();
  default:
    return r.y() + r.height();
  }
}

// the stretches of a computer's outer edge that have a monitor behind them
QList<Span> exposedSpans(const ComputerPlacement &placement, Side side)
{
  const auto bounds = placement.bounds();
  const int edge = edgeOf(bounds, side);

  QList<Span> spans;
  for (const auto &local : placement.monitors) {
    const auto monitor = local.translated(placement.pos);
    if (edgeOf(monitor, side) != edge) {
      continue;
    }
    if (isHorizontal(side)) {
      spans.append({monitor.y(), monitor.y() + monitor.height()});
    } else {
      spans.append({monitor.x(), monitor.x() + monitor.width()});
    }
  }

  std::sort(spans.begin(), spans.end(), [](const Span &a, const Span &b) { return a.start < b.start; });

  QList<Span> merged;
  for (const auto &span : std::as_const(spans)) {
    if (!merged.isEmpty() && span.start <= merged.last().end) {
      merged.last().end = std::max(merged.last().end, span.end);
    } else {
      merged.append(span);
    }
  }
  return merged;
}

// moves each computer in result so it still meets the computers it met in previous, along one axis
void followNeighbours(const PlacementList &previous, PlacementList &result, bool horizontal)
{
  const auto start = [horizontal](const QRect &r) { return edgeOf(r, horizontal ? Side::Left : Side::Up); };
  const auto end = [horizontal](const QRect &r) { return edgeOf(r, horizontal ? Side::Right : Side::Down); };
  const auto acrossStart = [horizontal](const QRect &r) { return edgeOf(r, horizontal ? Side::Up : Side::Left); };
  const auto acrossEnd = [horizontal](const QRect &r) { return edgeOf(r, horizontal ? Side::Down : Side::Right); };

  // nearest the origin first, so that a move carries along a whole row of computers
  QList<int> order;
  for (int i = 0; i < previous.size(); ++i) {
    order.append(i);
  }
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return start(previous.at(a).bounds()) < start(previous.at(b).bounds());
  });

  for (const int index : std::as_const(order)) {
    const auto area = previous.at(index).bounds();
    if (area.isEmpty()) {
      continue;
    }

    std::optional<int> edge;
    for (int other = 0; other < previous.size(); ++other) {
      const auto neighbour = previous.at(other).bounds();
      if (other != index && !neighbour.isEmpty() && end(neighbour) == start(area) &&
          acrossStart(neighbour) < acrossEnd(area) && acrossStart(area) < acrossEnd(neighbour)) {
        edge = std::max(edge.value_or(std::numeric_limits<int>::min()), end(result.at(other).bounds()));
      }
    }
    if (edge.has_value()) {
      const int shift = *edge - start(result.at(index).bounds());
      (horizontal ? result[index].pos.rx() : result[index].pos.ry()) += shift;
    }
  }
}

int percentAlong(int value, int start, int length)
{
  return std::clamp(qRound(100.0 * (value - start) / length), 0, 100);
}

} // namespace

QString formatMonitors(const MonitorList &monitors)
{
  QStringList parts;
  for (const auto &m : monitors) {
    parts.append(QStringLiteral("%1,%2,%3,%4").arg(m.x()).arg(m.y()).arg(m.width()).arg(m.height()));
  }
  return parts.join(';');
}

MonitorList parseMonitors(const QString &text)
{
  MonitorList monitors;
  const auto parts = text.split(';', Qt::SkipEmptyParts);
  for (const auto &part : parts) {
    if (monitors.size() >= kMaxMonitors) {
      break;
    }
    const auto values = part.split(',');
    if (values.size() != 4) {
      continue;
    }
    bool ok[4];
    const QRect rect(
        values[0].toInt(&ok[0]), values[1].toInt(&ok[1]), values[2].toInt(&ok[2]), values[3].toInt(&ok[3])
    );
    if (ok[0] && ok[1] && ok[2] && ok[3] && rect.width() > 0 && rect.height() > 0) {
      monitors.append(rect);
    }
  }
  return monitors;
}

QString formatMonitorMap(const MonitorMap &map)
{
  QStringList parts;
  for (auto it = map.cbegin(); it != map.cend(); ++it) {
    parts.append(QStringLiteral("%1:%2").arg(it.key(), formatMonitors(it.value())));
  }
  return parts.join('|');
}

MonitorMap parseMonitorMap(const QString &text)
{
  MonitorMap map;
  const auto parts = text.split('|', Qt::SkipEmptyParts);
  for (const auto &part : parts) {
    const auto separator = part.lastIndexOf(':');
    if (separator <= 0) {
      continue;
    }
    if (const auto monitors = parseMonitors(part.mid(separator + 1)); !monitors.isEmpty()) {
      map.insert(part.left(separator), monitors);
    }
  }
  return map;
}

MonitorList normalizedMonitors(const MonitorList &monitors)
{
  if (monitors.isEmpty()) {
    return {};
  }
  int left = std::numeric_limits<int>::max();
  int top = std::numeric_limits<int>::max();
  for (const auto &m : monitors) {
    left = std::min(left, m.x());
    top = std::min(top, m.y());
  }
  MonitorList result;
  for (const auto &m : monitors) {
    result.append(m.translated(-left, -top));
  }
  return result;
}

QRect ComputerPlacement::bounds() const
{
  QRect area;
  for (const auto &m : monitors) {
    area = area.united(m);
  }
  return area.translated(pos);
}

QString DisplayLink::toConfigLine() const
{
  const auto interval = [](int start, int end) {
    return start == 0 && end == 100 ? QString() : QStringLiteral("(%1,%2)").arg(start).arg(end);
  };
  return QStringLiteral("%1%2 = %3%4").arg(side, interval(fromStart, fromEnd), to, interval(toStart, toEnd));
}

QList<DisplayLink> computeDisplayLinks(const PlacementList &placements)
{
  QList<DisplayLink> links;

  for (const auto &a : placements) {
    const auto aBounds = a.bounds();
    if (aBounds.isEmpty()) {
      continue;
    }

    for (const auto side : kSides) {
      QList<DisplayLink> sideLinks;
      const int aLength = isHorizontal(side) ? aBounds.height() : aBounds.width();
      const int aStart = isHorizontal(side) ? aBounds.y() : aBounds.x();
      const auto aSpans = exposedSpans(a, side);

      for (const auto &b : placements) {
        const auto bBounds = b.bounds();
        if (b.name == a.name || bBounds.isEmpty() ||
            std::abs(edgeOf(aBounds, side) - edgeOf(bBounds, opposite(side))) > kTouchTolerance) {
          continue;
        }

        const int bLength = isHorizontal(side) ? bBounds.height() : bBounds.width();
        const int bStart = isHorizontal(side) ? bBounds.y() : bBounds.x();

        for (const auto &aSpan : aSpans) {
          for (const auto &bSpan : exposedSpans(b, opposite(side))) {
            const int start = std::max(aSpan.start, bSpan.start);
            const int end = std::min(aSpan.end, bSpan.end);
            if (start >= end) {
              continue;
            }
            DisplayLink link;
            link.from = a.name;
            link.side = sideName(side);
            link.fromStart = percentAlong(start, aStart, aLength);
            link.fromEnd = percentAlong(end, aStart, aLength);
            link.to = b.name;
            link.toStart = percentAlong(start, bStart, bLength);
            link.toEnd = percentAlong(end, bStart, bLength);
            // a sliver that rounds away to nothing would be refused by the configuration reader
            if (link.fromStart < link.fromEnd && link.toStart < link.toEnd) {
              sideLinks.append(link);
            }
          }
        }
      }

      // the configuration reader refuses overlapping intervals on one edge
      std::sort(sideLinks.begin(), sideLinks.end(), [](const DisplayLink &x, const DisplayLink &y) {
        return x.fromStart < y.fromStart;
      });
      int covered = 0;
      for (const auto &link : std::as_const(sideLinks)) {
        if (link.fromStart >= covered) {
          links.append(link);
          covered = link.fromEnd;
        }
      }
    }
  }

  return links;
}

bool placementOverlaps(const PlacementList &placements, int index, const QPoint &pos)
{
  auto moved = placements.at(index);
  moved.pos = pos;
  const auto area = moved.bounds();
  for (int i = 0; i < placements.size(); ++i) {
    if (i != index && placements.at(i).bounds().intersects(area)) {
      return true;
    }
  }
  return false;
}

QPoint snapPlacement(const PlacementList &placements, int index, const QPoint &pos, int tolerance)
{
  auto moved = placements.at(index);
  moved.pos = pos;
  const auto area = moved.bounds();

  int bestX = tolerance + 1;
  int bestY = tolerance + 1;
  QPoint delta;

  const auto consider = [](int from, int to, int &best, int &out) {
    if (const int distance = std::abs(to - from); distance < best) {
      best = distance;
      out = to - from;
    }
  };

  for (int i = 0; i < placements.size(); ++i) {
    if (i == index) {
      continue;
    }
    const auto other = placements.at(i).bounds();
    const int left = edgeOf(other, Side::Left);
    const int right = edgeOf(other, Side::Right);
    const int top = edgeOf(other, Side::Up);
    const int bottom = edgeOf(other, Side::Down);

    for (const int target : {left, right}) {
      consider(edgeOf(area, Side::Left), target, bestX, delta.rx());
      consider(edgeOf(area, Side::Right), target, bestX, delta.rx());
    }
    for (const int target : {top, bottom}) {
      consider(edgeOf(area, Side::Up), target, bestY, delta.ry());
      consider(edgeOf(area, Side::Down), target, bestY, delta.ry());
    }
  }

  return pos + delta;
}

PlacementList withoutOverlaps(const PlacementList &placements)
{
  auto result = placements;
  for (int i = 1; i < result.size(); ++i) {
    // moving past one computer can land on another, so look again from the start
    for (int j = 0; j < i; ++j) {
      const auto other = result.at(j).bounds();
      if (result.at(i).bounds().intersects(other)) {
        result[i].pos.setX(result[i].pos.x() + other.x() + other.width() - result.at(i).bounds().x());
        j = -1;
      }
    }
  }
  return result;
}

PlacementList keepNeighboursTouching(const PlacementList &before, const PlacementList &after)
{
  PlacementList previous;
  previous.reserve(after.size());
  for (const auto &placement : after) {
    const auto found = std::find_if(before.begin(), before.end(), [&placement](const auto &other) {
      return other.name == placement.name;
    });
    // no monitors: new to the layout, so it has no neighbours to keep
    previous.append(found != before.end() ? *found : ComputerPlacement{placement.name, placement.pos, {}});
  }

  auto result = after;
  followNeighbours(previous, result, true);
  followNeighbours(previous, result, false);
  return result;
}

QStringList unreachableComputers(const PlacementList &placements)
{
  if (placements.size() < 2) {
    return {};
  }
  const auto links = computeDisplayLinks(placements);
  QStringList names;
  for (const auto &placement : placements) {
    const bool linked = std::any_of(links.begin(), links.end(), [&placement](const DisplayLink &link) {
      return link.from == placement.name;
    });
    if (!linked) {
      names.append(placement.name);
    }
  }
  return names;
}

} // namespace deskflow
