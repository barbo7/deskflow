/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorLayoutView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <limits>

using namespace deskflow;

namespace {

const double kMargin = 24.0;
const double kMonitorGap = 1.5;
const double kSnapPixels = 18.0;
const QColor kConnected(46, 160, 67);
const QColor kInvalid(218, 54, 51);
const QColor kInUse(245, 158, 11);

} // namespace

MonitorLayoutView::MonitorLayoutView(QWidget *parent) : QWidget(parent)
{
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize MonitorLayoutView::minimumSizeHint() const
{
  return {420, 240};
}

void MonitorLayoutView::setPlacements(const PlacementList &placements)
{
  // a live update must not yank the computer out from under the mouse
  if (m_dragIndex != -1) {
    return;
  }
  m_placements = placements;
  fitToView();
  update();
}

void MonitorLayoutView::setStatus(const QSet<QString> &connected, const QString &active)
{
  m_connected = connected;
  m_active = active;
  update();
}

void MonitorLayoutView::fitToView()
{
  QRect area;
  for (const auto &placement : std::as_const(m_placements)) {
    area = area.united(placement.bounds());
  }
  if (area.isEmpty()) {
    m_scale = 1.0;
    m_offset = {};
    return;
  }

  const double availableWidth = std::max(1.0, width() - 2 * kMargin);
  const double availableHeight = std::max(1.0, height() - 2 * kMargin);
  m_scale = std::min(availableWidth / area.width(), availableHeight / area.height());
  m_offset = {
      (width() - area.width() * m_scale) / 2 - area.x() * m_scale,
      (height() - area.height() * m_scale) / 2 - area.y() * m_scale
  };
}

void MonitorLayoutView::moveOriginToZero()
{
  int left = std::numeric_limits<int>::max();
  int top = std::numeric_limits<int>::max();
  for (const auto &placement : std::as_const(m_placements)) {
    left = std::min(left, placement.pos.x());
    top = std::min(top, placement.pos.y());
  }
  for (auto &placement : m_placements) {
    placement.pos -= QPoint(left, top);
  }
}

QRectF MonitorLayoutView::toView(const QRect &rect) const
{
  return {
      rect.x() * m_scale + m_offset.x(), rect.y() * m_scale + m_offset.y(), rect.width() * m_scale,
      rect.height() * m_scale
  };
}

int MonitorLayoutView::computerAt(const QPointF &viewPos) const
{
  for (int i = static_cast<int>(m_placements.size()) - 1; i >= 0; --i) {
    const auto &placement = m_placements.at(i);
    for (const auto &monitor : placement.monitors) {
      if (toView(monitor.translated(placement.pos)).contains(viewPos)) {
        return i;
      }
    }
  }
  return -1;
}

void MonitorLayoutView::drawComputer(QPainter &painter, const ComputerPlacement &placement, bool invalid) const
{
  const bool connected = m_connected.contains(placement.name);
  const bool active = placement.name == m_active;

  auto fill = palette().color(QPalette::Mid);
  if (connected) {
    fill = palette().color(QPalette::Highlight);
  }
  if (invalid) {
    fill = kInvalid;
  }
  const auto textColor = connected || invalid ? palette().color(QPalette::HighlightedText)
                                              : palette().color(QPalette::WindowText);

  // the largest monitor gets the computer's name
  int labelMonitor = 0;
  for (int i = 1; i < placement.monitors.size(); ++i) {
    const auto &candidate = placement.monitors.at(i);
    const auto &best = placement.monitors.at(labelMonitor);
    if (candidate.width() * candidate.height() > best.width() * best.height()) {
      labelMonitor = i;
    }
  }

  auto nameFont = font();
  nameFont.setBold(true);

  for (int i = 0; i < placement.monitors.size(); ++i) {
    const auto &monitor = placement.monitors.at(i);
    const auto rect = toView(monitor.translated(placement.pos)).adjusted(kMonitorGap, kMonitorGap, -kMonitorGap, -kMonitorGap);

    painter.setPen(active ? QPen(kInUse, 3) : QPen(palette().color(QPalette::Dark), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(rect, 4, 4);

    painter.setPen(textColor);
    const auto size = QStringLiteral("%1 × %2").arg(monitor.width()).arg(monitor.height());
    if (i == labelMonitor) {
      const auto state = active ? tr("In use") : (connected ? tr("Connected") : tr("Not connected"));
      painter.setFont(nameFont);
      const auto metrics = painter.fontMetrics();
      const auto name = metrics.elidedText(placement.name, Qt::ElideMiddle, static_cast<int>(rect.width()) - 8);
      const QRectF textArea = rect.adjusted(4, 0, -4, 0);
      const double lineHeight = metrics.height();
      const QPointF middle = textArea.center();

      painter.drawText(
          QRectF(textArea.left(), middle.y() - lineHeight * 1.5, textArea.width(), lineHeight), Qt::AlignCenter, name
      );
      painter.setFont(font());
      painter.drawText(
          QRectF(textArea.left(), middle.y() - lineHeight * 0.5, textArea.width(), lineHeight), Qt::AlignCenter, state
      );
      painter.drawText(
          QRectF(textArea.left(), middle.y() + lineHeight * 0.5, textArea.width(), lineHeight), Qt::AlignCenter, size
      );
    } else {
      painter.setFont(font());
      painter.drawText(rect, Qt::AlignCenter, size);
    }
  }
}

void MonitorLayoutView::drawLinks(QPainter &painter) const
{
  const auto findBounds = [this](const QString &name) {
    for (const auto &placement : m_placements) {
      if (placement.name == name) {
        return placement.bounds();
      }
    }
    return QRect();
  };

  painter.setPen(QPen(kConnected, 4, Qt::SolidLine, Qt::RoundCap));
  for (const auto &link : computeDisplayLinks(m_placements)) {
    // each link exists in both directions; draw it once
    if (link.side != QStringLiteral("right") && link.side != QStringLiteral("down")) {
      continue;
    }
    const auto view = toView(findBounds(link.from));
    if (link.side == QStringLiteral("right")) {
      painter.drawLine(QLineF(
          view.right(), view.top() + view.height() * link.fromStart / 100.0, view.right(),
          view.top() + view.height() * link.fromEnd / 100.0
      ));
    } else {
      painter.drawLine(QLineF(
          view.left() + view.width() * link.fromStart / 100.0, view.bottom(),
          view.left() + view.width() * link.fromEnd / 100.0, view.bottom()
      ));
    }
  }
}

void MonitorLayoutView::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  if (m_placements.isEmpty()) {
    painter.drawText(rect(), Qt::AlignCenter, tr("No computers yet. Add some on the Computers tab."));
    return;
  }

  for (int i = 0; i < m_placements.size(); ++i) {
    if (i != m_dragIndex) {
      drawComputer(painter, m_placements.at(i), false);
    }
  }
  if (m_dragIndex == -1) {
    drawLinks(painter);
  } else {
    // what's being dragged goes on top
    painter.setOpacity(0.85);
    drawComputer(painter, m_placements.at(m_dragIndex), m_dragInvalid);
  }
}

void MonitorLayoutView::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || !isEnabled()) {
    return;
  }
  m_dragIndex = computerAt(event->position());
  if (m_dragIndex == -1) {
    return;
  }
  m_dragStartMouse = event->position();
  m_dragStartPos = m_placements.at(m_dragIndex).pos;
  m_dragInvalid = false;
  setCursor(Qt::ClosedHandCursor);
  update();
}

void MonitorLayoutView::mouseMoveEvent(QMouseEvent *event)
{
  if (m_dragIndex == -1) {
    setCursor(isEnabled() && computerAt(event->position()) != -1 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }

  const auto delta = (event->position() - m_dragStartMouse) / m_scale;
  const QPoint proposed = m_dragStartPos + delta.toPoint();
  const int tolerance = static_cast<int>(kSnapPixels / m_scale);
  const auto snapped = snapPlacement(m_placements, m_dragIndex, proposed, tolerance);

  m_placements[m_dragIndex].pos = snapped;
  m_dragInvalid = placementOverlaps(m_placements, m_dragIndex, snapped);
  update();
}

void MonitorLayoutView::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || m_dragIndex == -1) {
    return;
  }

  const bool moved = !m_dragInvalid && m_placements.at(m_dragIndex).pos != m_dragStartPos;
  if (m_dragInvalid) {
    m_placements[m_dragIndex].pos = m_dragStartPos;
  }
  m_dragIndex = -1;
  m_dragInvalid = false;
  setCursor(Qt::OpenHandCursor);

  moveOriginToZero();
  fitToView();
  update();

  if (moved) {
    Q_EMIT placementsChanged();
  }
}

void MonitorLayoutView::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  fitToView();
}
