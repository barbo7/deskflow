/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ActiveComputerIcon.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

namespace deskflow::gui {

namespace {

// apple silhouette with a bite and a leaf, in a 64x64 box
QPainterPath applePath()
{
  QPainterPath body;
  body.moveTo(32, 20);
  body.cubicTo(38, 14, 55, 13, 57, 30);
  body.cubicTo(58, 44, 51, 58, 45, 59);
  body.cubicTo(40, 60, 37, 56.5, 32, 56.5);
  body.cubicTo(27, 56.5, 24, 60, 19, 59);
  body.cubicTo(13, 58, 6, 44, 7, 30);
  body.cubicTo(9, 13, 26, 14, 32, 20);
  body.closeSubpath();

  QPainterPath bite;
  bite.addEllipse(QPointF(60, 38), 8.5, 8.5);
  body = body.subtracted(bite);

  QPainterPath leaf;
  leaf.addEllipse(QPointF(0, 0), 4.5, 9);
  QTransform t;
  t.translate(37.5, 9.5);
  t.rotate(40);
  body.addPath(t.map(leaf));
  return body;
}

// four window panes in a 64x64 box
QPainterPath windowsPath()
{
  QPainterPath path;
  const qreal pane = 25;
  const qreal gap = 4;
  const qreal start = (64 - (2 * pane + gap)) / 2;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      path.addRect(start + col * (pane + gap), start + row * (pane + gap), pane, pane);
    }
  }
  return path;
}

} // namespace

QString platformLabel(const QString &platform)
{
  if (platform == QStringLiteral("mac"))
    return QStringLiteral("Mac");
  if (platform == QStringLiteral("windows"))
    return QStringLiteral("Windows");
  if (platform == QStringLiteral("linux"))
    return QStringLiteral("Linux");
  return {};
}

QImage drawActiveComputerIcon(const QString &platform, const QString &name, const QColor &color, int size)
{
  QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.scale(size / 64.0, size / 64.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);

  if (platform == QStringLiteral("mac")) {
    painter.drawPath(applePath());
  } else if (platform == QStringLiteral("windows")) {
    painter.drawPath(windowsPath());
  } else {
    // unknown platform: the computer's initial in a rounded box
    painter.setPen(QPen(color, 5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(5, 5, 54, 54), 12, 12);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(36);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter, name.left(1).toUpper());
  }

  painter.end();
  return image;
}

} // namespace deskflow::gui
