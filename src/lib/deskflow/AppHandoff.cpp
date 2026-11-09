/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/AppHandoff.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace deskflow::handoff {

namespace {

const QString kProgramsCode = QStringLiteral("%LOCALAPPDATA%\\Programs");

// Extensions of plain documents. Anything that can run code when opened (programs, scripts,
// installers, shortcuts, disk images) is deliberately missing.
const QStringList kDocumentExtensions = {
    QStringLiteral("pdf"),  QStringLiteral("txt"),  QStringLiteral("md"),   QStringLiteral("rtf"),
    QStringLiteral("csv"),  QStringLiteral("tsv"),  QStringLiteral("doc"),  QStringLiteral("docx"),
    QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("ppt"),  QStringLiteral("pptx"),
    QStringLiteral("odt"),  QStringLiteral("ods"),  QStringLiteral("odp"),  QStringLiteral("pages"),
    QStringLiteral("numbers"), QStringLiteral("key"), QStringLiteral("png"), QStringLiteral("jpg"),
    QStringLiteral("jpeg"), QStringLiteral("gif"),  QStringLiteral("webp"), QStringLiteral("heic"),
    QStringLiteral("bmp"),  QStringLiteral("tiff"), QStringLiteral("mp3"),  QStringLiteral("wav"),
    QStringLiteral("m4a"),  QStringLiteral("mp4"),  QStringLiteral("mov"),  QStringLiteral("mkv"),
};

} // namespace

const std::vector<App> &knownApps()
{
  static const std::vector<App> apps = {
      // browsers
      {QStringLiteral("chrome"),
       QStringLiteral("Google Chrome"),
       true,
       {QStringLiteral("com.google.Chrome")},
       {QStringLiteral("chrome.exe")},
       {QStringLiteral("chrome.exe")}},
      {QStringLiteral("edge"),
       QStringLiteral("Microsoft Edge"),
       true,
       {QStringLiteral("com.microsoft.edgemac")},
       {QStringLiteral("msedge.exe")},
       {QStringLiteral("msedge.exe")}},
      {QStringLiteral("brave"),
       QStringLiteral("Brave"),
       true,
       {QStringLiteral("com.brave.Browser")},
       {QStringLiteral("brave.exe")},
       {QStringLiteral("brave.exe")}},
      {QStringLiteral("firefox"),
       QStringLiteral("Firefox"),
       true,
       {QStringLiteral("org.mozilla.firefox")},
       {QStringLiteral("firefox.exe")},
       {QStringLiteral("firefox.exe")}},
      {QStringLiteral("safari"), QStringLiteral("Safari"), true, {QStringLiteral("com.apple.Safari")}, {}, {}},
      // office
      {QStringLiteral("word"),
       QStringLiteral("Microsoft Word"),
       false,
       {QStringLiteral("com.microsoft.Word")},
       {QStringLiteral("winword.exe")},
       {QStringLiteral("winword.exe")}},
      {QStringLiteral("excel"),
       QStringLiteral("Microsoft Excel"),
       false,
       {QStringLiteral("com.microsoft.Excel")},
       {QStringLiteral("excel.exe")},
       {QStringLiteral("excel.exe")}},
      {QStringLiteral("powerpoint"),
       QStringLiteral("Microsoft PowerPoint"),
       false,
       {QStringLiteral("com.microsoft.Powerpoint")},
       {QStringLiteral("powerpnt.exe")},
       {QStringLiteral("powerpnt.exe")}},
      {QStringLiteral("acrobat"),
       QStringLiteral("Adobe Acrobat"),
       false,
       {QStringLiteral("com.adobe.Reader"), QStringLiteral("com.adobe.Acrobat.Pro")},
       {QStringLiteral("acrobat.exe"), QStringLiteral("acrord32.exe")},
       {QStringLiteral("acrobat.exe"), QStringLiteral("acrord32.exe")}},
      // editors
      {QStringLiteral("vscode"),
       QStringLiteral("Visual Studio Code"),
       false,
       {QStringLiteral("com.microsoft.VSCode")},
       {QStringLiteral("code.exe")},
       {kProgramsCode + QStringLiteral("\\Microsoft VS Code\\Code.exe"),
        QStringLiteral("%ProgramFiles%\\Microsoft VS Code\\Code.exe"), QStringLiteral("vscode:")}},
      {QStringLiteral("cursor"),
       QStringLiteral("Cursor"),
       false,
       {QStringLiteral("com.todesktop.230313mzl4w4u92")},
       {QStringLiteral("cursor.exe")},
       {kProgramsCode + QStringLiteral("\\cursor\\Cursor.exe"), QStringLiteral("cursor:")}},
      {QStringLiteral("sublime"),
       QStringLiteral("Sublime Text"),
       false,
       {QStringLiteral("com.sublimetext.4"), QStringLiteral("com.sublimetext.3")},
       {QStringLiteral("sublime_text.exe")},
       {QStringLiteral("%ProgramFiles%\\Sublime Text\\sublime_text.exe")}},
      {QStringLiteral("obsidian"),
       QStringLiteral("Obsidian"),
       false,
       {QStringLiteral("md.obsidian")},
       {QStringLiteral("obsidian.exe")},
       {QStringLiteral("obsidian:")}},
      {QStringLiteral("notion"),
       QStringLiteral("Notion"),
       false,
       {QStringLiteral("notion.id")},
       {QStringLiteral("notion.exe")},
       {QStringLiteral("notion:")}},
      {QStringLiteral("figma"),
       QStringLiteral("Figma"),
       false,
       {QStringLiteral("com.figma.Desktop")},
       {QStringLiteral("figma.exe")},
       {QStringLiteral("figma:")}},
      {QStringLiteral("photoshop"),
       QStringLiteral("Adobe Photoshop"),
       false,
       {QStringLiteral("com.adobe.Photoshop")},
       {QStringLiteral("photoshop.exe")},
       {QStringLiteral("photoshop.exe")}},
      {QStringLiteral("vlc"),
       QStringLiteral("VLC"),
       false,
       {QStringLiteral("org.videolan.vlc")},
       {QStringLiteral("vlc.exe")},
       {QStringLiteral("vlc.exe")}},
      // communication and media: these open without a document
      {QStringLiteral("spotify"),
       QStringLiteral("Spotify"),
       false,
       {QStringLiteral("com.spotify.client")},
       {QStringLiteral("spotify.exe")},
       {QStringLiteral("spotify:")}},
      {QStringLiteral("slack"),
       QStringLiteral("Slack"),
       false,
       {QStringLiteral("com.tinyspeck.slackmacgap")},
       {QStringLiteral("slack.exe")},
       {QStringLiteral("slack:")}},
      {QStringLiteral("discord"),
       QStringLiteral("Discord"),
       false,
       {QStringLiteral("com.hnc.Discord")},
       {QStringLiteral("discord.exe")},
       {QStringLiteral("discord:")}},
      {QStringLiteral("teams"),
       QStringLiteral("Microsoft Teams"),
       false,
       {QStringLiteral("com.microsoft.teams2"), QStringLiteral("com.microsoft.teams")},
       {QStringLiteral("ms-teams.exe"), QStringLiteral("teams.exe")},
       {QStringLiteral("msteams:")}},
      {QStringLiteral("zoom"),
       QStringLiteral("Zoom"),
       false,
       {QStringLiteral("us.zoom.xos")},
       {QStringLiteral("zoom.exe")},
       {QStringLiteral("zoommtg:")}},
      {QStringLiteral("telegram"),
       QStringLiteral("Telegram"),
       false,
       {QStringLiteral("ru.keepcoder.Telegram"), QStringLiteral("org.telegram.desktop")},
       {QStringLiteral("telegram.exe")},
       {QStringLiteral("tg:")}},
      {QStringLiteral("whatsapp"),
       QStringLiteral("WhatsApp"),
       false,
       {QStringLiteral("net.whatsapp.WhatsApp")},
       {QStringLiteral("whatsapp.exe")},
       {QStringLiteral("whatsapp:")}},
  };
  return apps;
}

const App *findAppById(const QString &id)
{
  const auto &apps = knownApps();
  const auto it = std::find_if(apps.begin(), apps.end(), [&id](const App &app) { return app.id == id; });
  return it == apps.end() ? nullptr : &*it;
}

const App *findAppByMacBundle(const QString &bundleId)
{
  const auto &apps = knownApps();
  const auto it = std::find_if(apps.begin(), apps.end(), [&bundleId](const App &app) {
    return app.macBundleIds.contains(bundleId, Qt::CaseInsensitive);
  });
  return it == apps.end() ? nullptr : &*it;
}

const App *findAppByWindowsExe(const QString &exeFileName)
{
  const auto name = exeFileName.toLower();
  const auto &apps = knownApps();
  const auto it = std::find_if(apps.begin(), apps.end(), [&name](const App &app) {
    return app.windowsExes.contains(name);
  });
  return it == apps.end() ? nullptr : &*it;
}

bool closesWithTabKey(const QString &appId)
{
  static const QStringList editors = {QStringLiteral("vscode"), QStringLiteral("cursor"), QStringLiteral("sublime")};
  const auto *app = findAppById(appId);
  return app != nullptr && (app->isBrowser || editors.contains(appId));
}

bool hasNoDocuments(const QString &appId)
{
  static const QStringList apps = {
      QStringLiteral("spotify"), QStringLiteral("slack"),    QStringLiteral("discord"),  QStringLiteral("teams"),
      QStringLiteral("zoom"),    QStringLiteral("telegram"), QStringLiteral("whatsapp"), QStringLiteral("notion"),
      QStringLiteral("figma"),   QStringLiteral("obsidian"),
  };
  return apps.contains(appId);
}

bool isAllowedUrl(const QString &url)
{
  if (url.isEmpty() || url.size() > 8192) {
    return false;
  }
  for (const auto c : url) {
    if (c.isSpace() || c.category() == QChar::Other_Control || c == QLatin1Char('"')) {
      return false;
    }
  }
  const QUrl parsed(url, QUrl::StrictMode);
  const auto scheme = parsed.scheme().toLower();
  return parsed.isValid() && !parsed.host().isEmpty() &&
         (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"));
}

QString safeFileName(const QString &name)
{
  // a peer could send "..\..\evil": only what follows the last separator counts
  auto result = name;
  result.replace(QLatin1Char('\\'), QLatin1Char('/'));
  result = result.section(QLatin1Char('/'), -1);

  // characters Windows refuses, or that would confuse a quoted command line
  static const QString forbidden = QStringLiteral("<>:\"|?*");
  QString cleaned;
  for (const auto c : std::as_const(result)) {
    if (c.category() != QChar::Other_Control && !forbidden.contains(c)) {
      cleaned.append(c);
    }
  }

  // no hidden files, and no trailing dots or spaces, which Windows silently drops
  while (cleaned.startsWith(QLatin1Char('.')) || cleaned.startsWith(QLatin1Char(' '))) {
    cleaned.remove(0, 1);
  }
  while (cleaned.endsWith(QLatin1Char('.')) || cleaned.endsWith(QLatin1Char(' '))) {
    cleaned.chop(1);
  }

  return cleaned.left(200);
}

bool canOpenWithDefaultApp(const QString &fileName)
{
  return kDocumentExtensions.contains(QFileInfo(fileName).suffix().toLower());
}

QString uniqueFilePath(const QString &folder, const QString &fileName)
{
  const QDir dir(folder);
  auto candidate = dir.filePath(fileName);
  if (!QFileInfo::exists(candidate)) {
    return candidate;
  }

  const QFileInfo info(fileName);
  const auto base = info.completeBaseName();
  const auto suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".%1").arg(info.suffix());
  for (int i = 2;; ++i) {
    candidate = dir.filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
    if (!QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
}

QString receiveFolder()
{
  auto base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  if (base.isEmpty()) {
    base = QDir::tempPath();
  }
  return QDir(base).filePath(QStringLiteral("Deskflow Handoff"));
}

QString documentNameFromWindowTitle(const QString &title)
{
  auto name = title.section(QStringLiteral(" - "), 0, 0).trimmed();

  // unsaved markers that editors put in front
  while (name.startsWith(QLatin1Char('*')) || name.startsWith(QChar(0x25CF)) || name.startsWith(QChar(0x2022))) {
    name = name.mid(1).trimmed();
  }
  if (name.endsWith(QLatin1Char('*'))) {
    name.chop(1);
  }

  // Office adds these after the name
  if (const auto bracket = name.lastIndexOf(QStringLiteral(" [")); bracket > 0 && name.endsWith(QLatin1Char(']'))) {
    name = name.left(bracket);
  }
  return name.trimmed();
}

} // namespace deskflow::handoff
