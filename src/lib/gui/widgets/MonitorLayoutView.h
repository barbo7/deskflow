/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/MonitorLayout.h"

#include <QPointF>
#include <QSet>
#include <QWidget>

/**
 * Shows every computer's real monitors to scale, and lets the user drag computers around.
 * The computers link up wherever their monitors touch.
 */
class MonitorLayoutView : public QWidget
{
  Q_OBJECT

public:
  explicit MonitorLayoutView(QWidget *parent = nullptr);

  void setPlacements(const deskflow::PlacementList &placements);
  [[nodiscard]] const deskflow::PlacementList &placements() const
  {
    return m_placements;
  }

  /**
   * \param connected Names of the computers that are connected right now.
   * \param active Name of the computer that has the cursor.
   */
  void setStatus(const QSet<QString> &connected, const QString &active);

  [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
  //! The user moved a computer.
  void placementsChanged();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void fitToView();
  void moveOriginToZero();
  [[nodiscard]] QRectF toView(const QRect &rect) const;
  [[nodiscard]] int computerAt(const QPointF &viewPos) const;
  void drawComputer(QPainter &painter, const deskflow::ComputerPlacement &placement, bool invalid) const;
  void drawLinks(QPainter &painter) const;

  deskflow::PlacementList m_placements;
  QSet<QString> m_connected;
  QString m_active;

  double m_scale = 1.0;
  QPointF m_offset;

  int m_dragIndex = -1;
  QPointF m_dragStartMouse;
  QPoint m_dragStartPos;
  bool m_dragInvalid = false;
};
