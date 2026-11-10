/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppHandoffTests.h"

#include "deskflow/AppHandoff.h"

#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
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

void AppHandoffTests::apps_knowHowTheyClose()
{
  QVERIFY(closesWithTabKey(QStringLiteral("chrome")));
  QVERIFY(closesWithTabKey(QStringLiteral("vscode")));
  QVERIFY(!closesWithTabKey(QStringLiteral("word")));
  QVERIFY(!closesWithTabKey(QStringLiteral("unknown")));

  QVERIFY(hasNoDocuments(QStringLiteral("spotify")));
  QVERIFY(!hasNoDocuments(QStringLiteral("word")));
  QVERIFY(!hasNoDocuments(QStringLiteral("chrome")));
  for (const auto &app : knownApps()) {
    QVERIFY2(!(app.isBrowser && hasNoDocuments(app.id)), qPrintable(app.id));
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

void AppHandoffTests::pageState_keepsWhatIsUsable()
{
  const auto state = parsePageState(R"({
    "wholeWindow": true,
    "tabIds": [7, 9],
    "tabs": [
      {"url": "https://a.example/x", "active": true, "state": {"scrollY": 40}},
      {"url": "file:///etc/passwd", "state": {"scrollY": 1}},
      {"url": "https://b.example/", "state": "not an object"}
    ]
  })");
  QVERIFY(state.has_value());
  const QStringList urls = {QStringLiteral("https://a.example/x"), QStringLiteral("https://b.example/")};
  QCOMPARE(state->urls, urls);
  const QList<int> ids = {7, 9};
  QCOMPARE(state->tabIds, ids);

  // the ids mean nothing elsewhere, and a state that isn't an object isn't passed on
  QVERIFY(!state->json.contains("tabIds"));
  QVERIFY(!state->json.contains("not an object"));
  QVERIFY(!state->json.contains("passwd"));
  QVERIFY(state->json.contains("\"wholeWindow\":true"));
}

void AppHandoffTests::pageState_refusesUnusable()
{
  QVERIFY(!parsePageState({}).has_value());
  QVERIFY(!parsePageState("not json").has_value());
  QVERIFY(!parsePageState(R"({"tabs": []})").has_value());
  QVERIFY(!parsePageState(R"j({"tabs": [{"url": "javascript:alert(1)"}]})j").has_value());
  QVERIFY(!parsePageState(QByteArray(kMaxPageState + 1, 'x')).has_value());
}

namespace {
void writeFile(const QString &path, const QByteArray &contents)
{
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(contents);
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}
} // namespace

void AppHandoffTests::files_roundTripFoldersAndFiles()
{
  QTemporaryDir from;
  QTemporaryDir to;
  writeFile(from.filePath("notes.txt"), "hello");
  writeFile(from.filePath("project/src/main.cpp"), "int main() {}");
  writeFile(from.filePath("project/.gitignore"), "build/");
  writeFile(from.filePath("project/.DS_Store"), "junk");
  QDir(from.path()).mkpath("project/empty");
  // a symbolic link on macOS, a shortcut on Windows (which only counts as one with this suffix)
  QFile::link(from.filePath("notes.txt"), from.filePath("project/link.lnk"));

  const auto bundle = deskflow::handoff::packFiles({from.filePath("notes.txt"), from.filePath("project")});
  QVERIFY2(bundle.error.isEmpty(), qPrintable(bundle.error));
  QCOMPARE(bundle.files, 3);

  const auto written = deskflow::handoff::unpackFiles(bundle.data, to.path());
  QCOMPARE(written.size(), 2);
  QCOMPARE(readFile(to.filePath("notes.txt")), QByteArray("hello"));
  QCOMPARE(readFile(to.filePath("project/src/main.cpp")), QByteArray("int main() {}"));
  QCOMPARE(readFile(to.filePath("project/.gitignore")), QByteArray("build/"));
  QVERIFY(QFileInfo(to.filePath("project/empty")).isDir());
  QVERIFY(!QFileInfo::exists(to.filePath("project/.DS_Store")));
  QVERIFY(!QFileInfo::exists(to.filePath("project/link.lnk")));
}

void AppHandoffTests::files_neverOverwrite()
{
  QTemporaryDir from;
  QTemporaryDir to;
  writeFile(from.filePath("report.pdf"), "new");
  writeFile(to.filePath("report.pdf"), "old");

  const auto written =
      deskflow::handoff::unpackFiles(deskflow::handoff::packFiles({from.filePath("report.pdf")}).data, to.path());
  QCOMPARE(written, QStringList{to.filePath("report (2).pdf")});
  QCOMPARE(readFile(to.filePath("report.pdf")), QByteArray("old"));
  QCOMPARE(readFile(to.filePath("report (2).pdf")), QByteArray("new"));
}

void AppHandoffTests::files_stayInsideTheFolder()
{
  // a peer could send any names at all
  QByteArray bundle;
  QDataStream out(&bundle, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_0);
  out << quint32(0x44464642) << quint32(1) << quint32(4);
  out << QString("../escaped.txt") << quint8(0) << QByteArray("x");
  out << QString("/etc/evil") << quint8(0) << QByteArray("x");
  out << QString("ok/../../up.txt") << quint8(0) << QByteArray("x");
  out << QString("C:\\Windows\\bad.txt") << quint8(0) << QByteArray("x");

  QTemporaryDir parent;
  const auto folder = parent.filePath("inbox");
  const auto written = deskflow::handoff::unpackFiles(bundle, folder);

  for (const auto &path : written) {
    QVERIFY2(QFileInfo(path).absoluteFilePath().startsWith(QFileInfo(folder).absoluteFilePath() + "/"), qPrintable(path));
  }
  QVERIFY(!QFileInfo::exists(parent.filePath("escaped.txt")));
  QVERIFY(!QFileInfo::exists(parent.filePath("up.txt")));
  QDirIterator it(parent.path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QVERIFY2(it.next().startsWith(QFileInfo(folder).absoluteFilePath() + "/"), qPrintable(it.filePath()));
  }
}

void AppHandoffTests::files_refuseTooMany()
{
  QTemporaryDir from;
  for (int i = 0; i <= deskflow::handoff::kMaxBundleEntries; ++i) {
    writeFile(from.filePath(QStringLiteral("many/%1.txt").arg(i)), "x");
  }
  const auto bundle = deskflow::handoff::packFiles({from.filePath("many")});
  QVERIFY(bundle.data.isEmpty());
  QVERIFY(!bundle.error.isEmpty());

  QVERIFY(deskflow::handoff::unpackFiles(QByteArray("not a bundle"), from.filePath("out")).isEmpty());
}

QTEST_MAIN(AppHandoffTests)
