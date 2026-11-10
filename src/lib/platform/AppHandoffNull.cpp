/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/AppHandoff.h"

#include <QFileInfo>
#include <QProcess>

namespace deskflow::handoff {

// linux can't tell which app is in front (it varies by desktop), but it can receive

CapturedApp captureFrontApp()
{
  return {};
}

void closeCapturedApp(const CapturedApp &)
{
  // nothing was captured
}

void forgetCapturedApp(const CapturedApp &)
{
  // nothing is held
}

bool isWindowBeingDragged()
{
  return false;
}

void cancelWindowDrag()
{
  // nothing to cancel
}

void releaseLeftButton()
{
  // nothing to release
}

QStringList draggedFiles()
{
  return {};
}

void cancelFileDrag()
{
  // nothing to cancel
}

void noteLeftButtonPressed()
{
  // drags aren't watched here
}

void revealFiles(const QStringList &paths)
{
  // the folder they're in is the best this can do
  if (!paths.isEmpty()) {
    QProcess::startDetached(QStringLiteral("xdg-open"), {QFileInfo(paths.first()).absolutePath()});
  }
}

bool launchApp(const App &, const QString &)
{
  return false;
}

bool openWithDefaultApp(const QString &target)
{
  return !target.isEmpty() && QProcess::startDetached(QStringLiteral("xdg-open"), {target});
}

} // namespace deskflow::handoff
