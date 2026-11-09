/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class EdgePushAccumulatorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void feed_pushIntoWatchedEdge_triggersAtThreshold();
  void feed_pushIntoUnwatchedEdge_neverTriggers();
  void feed_movingAlongEdge_neverTriggers();
  void feed_awayFromEdge_resetsPush();
  void feed_pause_resetsPush();
  void feed_afterTrigger_staysQuiet();
  void pushDirection_findsTheEdge();
};
