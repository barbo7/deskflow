/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppHandoffTests.h"

#include "deskflow/AppHandoff.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>

using namespace deskflow::handoff;

void AppHandoffTests::apps_findsSameAppFromBothPlatforms()
{
  const auto *mac = findAppByMacBundle(QStringLiteral("com.microsoft.Word"));
  const auto *windows = findAppByWindowsExe(QStringLiteral("WINWORD.EXE"));
  QVERIFY(mac != nullptr);
  QCOMPARE(mac, windows);
  QCOMPARE(findAppById(QStringLiteral("word")), mac);
  QCOMPARE(findAppById(QStringLiteral("/bin/sh")), nullptr);
}

void AppHandoffTests::apps_idsAreUnique()
{
  QSet<QString> ids;
  for (const auto &app : knownApps()) {
    QVERIFY2(!ids.contains(app.id), qPrintable(app.id));
    ids.insert(app.id);
    QVERIFY2(!app.macBundleIds.isEmpty() || !app.windowsLaunch.isEmpty(), qPrintable(app.id));
  }
}

void AppHandoffTests::url_acceptsWeb()
{
  QVERIFY(isAllowedUrl(QStringLiteral("https://github.com/deskflow/deskflow?tab=readme#top")));
  QVERIFY(isAllowedUrl(QStringLiteral("http://192.168.1.10:8080/page")));
}

void AppHandoffTests::url_refusesEverythingElse()
{
  QVERIFY(!isAllowedUrl({}));
  QVERIFY(!isAllowedUrl(QStringLiteral("file:///etc/passwd")));
  QVERIFY(!isAllowedUrl(QStringLiteral("javascript:alert(1)")));
  QVERIFY(!isAllowedUrl(QStringLiteral("ms-msdt:/id")));
  QVERIFY(!isAllowedUrl(QStringLiteral("https://")));
  QVERIFY(!isAllowedUrl(QStringLiteral("https://a.com\" --gpu-launcher=calc")));
  QVERIFY(!isAllowedUrl(QStringLiteral("https://a.com/a b")));
}

void AppHandoffTests::fileName_stripsPaths()
{
  QCOMPARE(safeFileName(QStringLiteral("../../etc/passwd")), QStringLiteral("passwd"));
  QCOMPARE(safeFileName(QStringLiteral("..\\..\\Windows\\evil.docx")), QStringLiteral("evil.docx"));
  QCOMPARE(safeFileName(QStringLiteral("rapor.docx")), QStringLiteral("rapor.docx"));
  QCOMPARE(safeFileName(QStringLiteral("..")), QString());
  QCOMPARE(safeFileName(QStringLiteral("dir/")), QString());
}

void AppHandoffTests::fileName_stripsDangerousCharacters()
{
  QCOMPARE(safeFileName(QStringLiteral("a\"b<c>:d|e?f*.txt")), QStringLiteral("abcdef.txt"));
  QCOMPARE(safeFileName(QStringLiteral(".hidden")), QStringLiteral("hidden"));
  QCOMPARE(safeFileName(QStringLiteral("trailing. . ")), QStringLiteral("trailing"));
  QCOMPARE(safeFileName(QStringLiteral("a\nb.txt")), QStringLiteral("ab.txt"));
}

void AppHandoffTests::defaultApp_documentsOnly()
{
  QVERIFY(canOpenWithDefaultApp(QStringLiteral("Rapor.PDF")));
  QVERIFY(canOpenWithDefaultApp(QStringLiteral("tablo.xlsx")));
  for (const auto *name : {"a.exe", "a.bat", "a.cmd", "a.ps1", "a.msi", "a.lnk", "a.scr", "a.js", "a.vbs", "a.sh", "a.command", "a.app", "a.dmg", "a.pkg", "a.docm", "a.pdf.exe", "noextension"}) {
    QVERIFY2(!canOpenWithDefaultApp(QString::fromLatin1(name)), name);
  }
}

void AppHandoffTests::uniquePath_avoidsExistingFiles()
{
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto first = uniqueFilePath(dir.path(), QStringLiteral("a.tar.gz"));
  QVERIFY(first.endsWith(QStringLiteral("/a.tar.gz")));

  QFile file(first);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();
  QVERIFY(uniqueFilePath(dir.path(), QStringLiteral("a.tar.gz")).endsWith(QStringLiteral("/a.tar (2).gz")));
}

void AppHandoffTests::windowTitle_findsDocumentName()
{
  QCOMPARE(documentNameFromWindowTitle(QStringLiteral("Rapor.docx - Word")), QStringLiteral("Rapor.docx"));
  QCOMPARE(documentNameFromWindowTitle(QStringLiteral("Rapor - Saved - Word")), QStringLiteral("Rapor"));
  QCOMPARE(documentNameFromWindowTitle(QStringLiteral("*notes.txt - Notepad")), QStringLiteral("notes.txt"));
  QCOMPARE(
      documentNameFromWindowTitle(QString::fromUtf8("\xE2\x97\x8F main.cpp - proj - Visual Studio Code")),
      QStringLiteral("main.cpp")
  );
  QCOMPARE(
      documentNameFromWindowTitle(QStringLiteral("Book1 [Compatibility Mode] - Excel")), QStringLiteral("Book1")
  );
}

QTEST_MAIN(AppHandoffTests)
