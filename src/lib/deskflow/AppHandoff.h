/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

/**
 * Moving what you're working on to another computer: the app that's in front is opened on the
 * other computer, with the same web page or a copy of the same document.
 *
 * A running program can't move between operating systems, so what moves is the page or file.
 * Once the other computer has opened it, it's closed here; if that fails it stays where it was.
 *
 * Everything a peer sends is treated as untrusted: it can only name an app from knownApps(),
 * never a program to run, and only http(s) addresses and plain document files are accepted.
 */
namespace deskflow::handoff {

//! Files larger than this stay behind; the app still opens on the other computer.
inline constexpr uint32_t kMaxFileSize = 32 * 1024 * 1024;

//! Data is sent in pieces this big, well under the protocol's message limits.
inline constexpr uint32_t kChunkSize = 512 * 1024;

//! A browser's page state travels like a file, under this name.
inline const QString kPageStateFileName = QStringLiteral(".deskflow-page-state.json");

//! The browser's limit on a message to its extension, less some room.
inline constexpr int kMaxPageState = 900 * 1024;

//! The name a bundle of dragged files travels under, instead of a document's.
inline const QString kFilesBundleName = QStringLiteral(".deskflow-files.bundle");
//! The most a bundle of dragged files may hold, in bytes and in entries.
inline constexpr uint32_t kMaxBundleSize = 100 * 1024 * 1024;
inline constexpr int kMaxBundleEntries = 5000;

//! What the browser extension says about the pages being moved.
struct PageState
{
  QByteArray json;     //!< cleaned up: what's safe to send and to hand to the extension
  QStringList urls;    //!< the address of each page
  QList<int> tabIds;   //!< this browser's ids for the tabs; meaningless on another computer
};

//! Checks an extension's answer or a peer's page state; nothing left means nothing is usable.
std::optional<PageState> parsePageState(const QByteArray &json);

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

//! Whether closing one thing in this app means closing a tab rather than its window.
bool closesWithTabKey(const QString &appId);

//! Whether this app has no document of its own to carry over, like a chat or music app.
bool hasNoDocuments(const QString &appId);

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

//! Files and folders packed to send at once.
struct FileBundle
{
  QByteArray data;
  int files = 0; //!< files in it, not counting folders
  QString error; //!< why there's no data, if there isn't
};

/**
 * Packs \p paths, files or folders (with everything in them), keeping their names and the
 * layout inside folders. Links aren't followed. Fails, with error set, when it would hold more
 * than kMaxBundleSize bytes or kMaxBundleEntries entries, or something can't be read.
 */
FileBundle packFiles(const QStringList &paths);

/**
 * Unpacks a bundle from packFiles() into \p folder. Each top level item gets a name not taken
 * there yet, and nothing is written outside \p folder whatever the names in it say.
 * \return the top level items it wrote; empty if the bundle isn't usable.
 */
QStringList unpackFiles(const QByteArray &bundle, const QString &folder);

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

/**
 * Closes what captureFrontApp() found: the tab in a browser or code editor, otherwise the window.
 * It asks the way the close button does, so an app with unsaved work still gets to say so.
 */
void closeCapturedApp(const CapturedApp &app);

//! Lets go of the platform handle in \p app. Call once for everything captureFrontApp() returned.
void forgetCapturedApp(const CapturedApp &app);

//! Whether the left button is holding a window by its title bar, or a browser tab.
bool isWindowBeingDragged();

//! Takes back a window drag, if the platform can: the window returns to where it started.
void cancelWindowDrag();

//! Lets go of the left button, for a drag made with this computer's own mouse.
void releaseLeftButton();

/**
 * The files being dragged with the left button right now (in Finder or Explorer, say), if
 * that's what's being dragged; empty otherwise.
 */
QStringList draggedFiles();

//! Takes back a drag of files, so they don't land anywhere on this computer.
void cancelFileDrag();

//! For the platform to tell a drag that started with this press from an old one. Any thread.
void noteLeftButtonPressed();

//! Shows \p paths, all in the same folder, selected in Finder or Explorer.
void revealFiles(const QStringList &paths);

//! Starts \p app, opening \p target (an address or a file path) if it isn't empty.
bool launchApp(const App &app, const QString &target);

//! Opens \p target with the system's default app for it.
bool openWithDefaultApp(const QString &target);

} // namespace deskflow::handoff
