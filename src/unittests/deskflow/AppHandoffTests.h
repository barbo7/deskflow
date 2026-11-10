/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class AppHandoffTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void apps_findsSameAppFromBothPlatforms();
  void apps_idsAreUnique();
  void apps_knowHowTheyClose();
  void url_acceptsWeb();
  void url_refusesEverythingElse();
  void fileName_stripsPaths();
  void fileName_stripsDangerousCharacters();
  void defaultApp_documentsOnly();
  void uniquePath_avoidsExistingFiles();
  void windowTitle_findsDocumentName();
  void pageState_keepsWhatIsUsable();
  void pageState_refusesUnusable();
};
