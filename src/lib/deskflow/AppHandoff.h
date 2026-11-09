/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

/**
 * Moving what you're working on to another computer: the app that's in front is opened on the
 * other computer, with the same web page or a copy of the same document.
 *
 * A running program can't move between operating systems, so what moves is the page or file.
 *
 * Everything a peer sends is treated as untrusted: it can only name an app from knownApps(),
 * never a program to run, and only http(s) addresses and plain document files are accepted.
 */
namespace deskflow::handoff {

//! Files larger than this stay behind; the app still opens on the other computer.
inline constexpr uint32_t kMaxFileSize = 32 * 1024 * 1024;

//! Data is sent in pieces this big, well under the protocol's message limits.
inline constexpr uint32_t kChunkSize = 512 * 1024;

//! An app that exists on both Mac and Windows (or is at least a browser on one of them).
struct App
{
  QString id;
  QString displayName;
  bool isBrowser = false;
  QStringList macBundleIds;
  //! Lower case file names of its Windows executables, to recognise it.
  QStringList windowsExes;
  /**
   * What to start on Windows, tried in order: a program ShellExecute can find, a full path
   * (may contain %VARIABLES%), or a URI scheme such as "spotify:".
   */
  QStringList windowsLaunch;
};

const std::vector<App> &knownApps();
const App *findAppById(const QString &id);
const App *findAppByMacBundle(const QString &bundleId);
const App *findAppByWindowsExe(const QString &exeFileName);

//! Only http(s) addresses, with nothing a command line or shell could misread.
bool isAllowedUrl(const QString &url);

//! A bare file name that is safe to create; empty if nothing usable is left.
QString safeFileName(const QString &name);

//! Whether a file can be opened with whatever app the system picks: documents only.
bool canOpenWithDefaultApp(const QString &fileName);

//! A path in \p folder for \p fileName that doesn't exist yet ("a.txt", "a (2).txt", ...).
QString uniqueFilePath(const QString &folder, const QString &fileName);

//! Where received files go: "Deskflow Handoff" in the Downloads folder.
QString receiveFolder();

//! The name of a document from a window title such as "● report.docx - Word".
QString documentNameFromWindowTitle(const QString &title);

//! What's in front on this computer.
struct CapturedApp
{
  QString appId;       //!< empty if it's not one of knownApps()
  QString displayName; //!< as this computer calls it
  QString url;         //!< a web page, if it has one
  QString filePath;    //!< a local document, if it has one
  quintptr window = 0; //!< platform handle, to act on the same window later

  [[nodiscard]] bool isEmpty() const
  {
    return appId.isEmpty() && url.isEmpty() && filePath.isEmpty();
  }
};

// Implemented per platform. All of these can block for a while: never call them on the event
// thread, where they would freeze the shared mouse and keyboard.

//! Finds the app in front and the page or document it's showing.
CapturedApp captureFrontApp();

//! Closes the browser tab that captureFrontApp() found.
void closeCapturedTab(const CapturedApp &app);

//! Starts \p app, opening \p target (an address or a file path) if it isn't empty.
bool launchApp(const App &app, const QString &target);

//! Opens \p target with the system's default app for it.
bool openWithDefaultApp(const QString &target);

} // namespace deskflow::handoff
