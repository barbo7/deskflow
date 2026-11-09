/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/AppHandoff.h"

#include "base/Log.h"

#include <QProcess>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace deskflow::handoff {

namespace {

const int kScriptTimeoutMs = 5000;

// browsers that answer to the Chromium AppleScript dictionary
bool isChromiumBundle(const QString &bundleId)
{
  return bundleId == QStringLiteral("com.google.Chrome") || bundleId == QStringLiteral("com.microsoft.edgemac") ||
         bundleId == QStringLiteral("com.brave.Browser");
}

// through osascript, not NSAppleScript: that's only safe on the main thread, and this runs on a
// worker. the first use makes macOS ask whether Deskflow may control the browser.
QString runAppleScript(const QString &script)
{
  QProcess process;
  process.start(QStringLiteral("/usr/bin/osascript"), {QStringLiteral("-e"), script});
  if (!process.waitForFinished(kScriptTimeoutMs)) {
    process.kill();
    process.waitForFinished(1000);
    LOG_WARN("app handoff: applescript timed out");
    return {};
  }
  if (process.exitCode() != 0) {
    LOG_WARN("app handoff: applescript failed: %s", process.readAllStandardError().constData());
    return {};
  }
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString browserUrl(const QString &bundleId)
{
  if (isChromiumBundle(bundleId)) {
    return runAppleScript(QStringLiteral("tell application id \"%1\" to return URL of active tab of front window")
                              .arg(bundleId));
  }
  if (bundleId == QStringLiteral("com.apple.Safari")) {
    return runAppleScript(QStringLiteral("tell application id \"%1\" to return URL of current tab of front window")
                              .arg(bundleId));
  }
  // firefox has no scripting interface: it opens on the other computer without the page
  return {};
}

// the document of the focused window, as a file:// or web address
QString focusedDocument(AXUIElementRef app)
{
  CFTypeRef window = nullptr;
  if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, &window) != kAXErrorSuccess || window == nullptr) {
    return {};
  }

  QString result;
  CFTypeRef document = nullptr;
  if (AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(window), kAXDocumentAttribute, &document) ==
          kAXErrorSuccess &&
      document != nullptr) {
    if (CFGetTypeID(document) == CFStringGetTypeID()) {
      result = QString::fromNSString(static_cast<NSString *>(document));
    }
    CFRelease(document);
  }
  CFRelease(window);
  return result;
}

bool runOpen(const QStringList &arguments)
{
  QProcess process;
  process.start(QStringLiteral("/usr/bin/open"), arguments);
  return process.waitForFinished(kScriptTimeoutMs) && process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
}

} // namespace

CapturedApp captureFrontApp()
{
  CapturedApp captured;

  @autoreleasepool {
    // not NSWorkspace.frontmostApplication: it goes stale without an AppKit run loop
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    CFTypeRef focused = nullptr;
    const auto error = AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute, &focused);
    CFRelease(systemWide);
    if (error != kAXErrorSuccess || focused == nullptr) {
      LOG_WARN("app handoff: can't find the focused app (accessibility error %d)", static_cast<int>(error));
      return captured;
    }
    auto app = static_cast<AXUIElementRef>(focused);

    pid_t pid = 0;
    AXUIElementGetPid(app, &pid);
    NSRunningApplication *running = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    const auto bundleId = QString::fromNSString(running.bundleIdentifier);
    captured.displayName = QString::fromNSString(running.localizedName);

    const auto *known = findAppByMacBundle(bundleId);
    if (known != nullptr) {
      captured.appId = known->id;
      captured.displayName = known->displayName;
    }

    if (known != nullptr && known->isBrowser) {
      if (const auto address = browserUrl(bundleId); isAllowedUrl(address)) {
        captured.url = address;
      }
    } else {
      const auto document = focusedDocument(app);
      NSURL *url = document.isEmpty() ? nil : [NSURL URLWithString:document.toNSString()];
      if (url.isFileURL) {
        captured.filePath = QString::fromNSString(url.path);
      } else if (isAllowedUrl(document)) {
        // a document kept in the cloud, such as on OneDrive
        captured.url = document;
      }
    }

    CFRelease(focused);
  }

  return captured;
}

void closeCapturedTab(const CapturedApp &app)
{
  const auto *known = findAppById(app.appId);
  if (known == nullptr || !known->isBrowser || known->macBundleIds.isEmpty()) {
    return;
  }

  const auto &bundleId = known->macBundleIds.first();
  if (isChromiumBundle(bundleId)) {
    runAppleScript(QStringLiteral("tell application id \"%1\" to close active tab of front window").arg(bundleId));
  } else if (bundleId == QStringLiteral("com.apple.Safari")) {
    runAppleScript(QStringLiteral("tell application id \"%1\" to close current tab of front window").arg(bundleId));
  }
}

bool isWindowBeingDragged()
{
  if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft)) {
    return false;
  }

  AXUIElementRef systemWide = AXUIElementCreateSystemWide();
  CFTypeRef app = nullptr;
  const auto error = AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute, &app);
  CFRelease(systemWide);
  if (error != kAXErrorSuccess || app == nullptr) {
    return false;
  }

  CFTypeRef window = nullptr;
  AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(app), kAXFocusedWindowAttribute, &window);
  CFRelease(app);
  if (window == nullptr) {
    return false;
  }

  CFTypeRef positionValue = nullptr;
  CFTypeRef sizeValue = nullptr;
  AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(window), kAXPositionAttribute, &positionValue);
  AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(window), kAXSizeAttribute, &sizeValue);
  CFRelease(window);

  CGPoint position = CGPointZero;
  CGSize size = CGSizeZero;
  bool known = false;
  if (positionValue != nullptr && sizeValue != nullptr) {
    known = AXValueGetValue(static_cast<AXValueRef>(positionValue), kAXValueTypeCGPoint, &position) &&
            AXValueGetValue(static_cast<AXValueRef>(sizeValue), kAXValueTypeCGSize, &size);
  }
  if (positionValue != nullptr) {
    CFRelease(positionValue);
  }
  if (sizeValue != nullptr) {
    CFRelease(sizeValue);
  }
  if (!known) {
    return false;
  }

  CGEventRef now = CGEventCreate(nullptr);
  const CGPoint cursor = CGEventGetLocation(now);
  CFRelease(now);

  // macOS can't say a window is being dragged, but a window follows the cursor by the strip
  // along its top: its title bar, or the tabs of a browser
  const double kTitleStrip = 56.0;
  return cursor.x >= position.x - 2 && cursor.x <= position.x + size.width + 2 && cursor.y >= position.y - 2 &&
         cursor.y <= position.y + kTitleStrip;
}

void cancelWindowDrag()
{
  // macOS has no way to take back a window drag
}

void releaseLeftButton()
{
  CGEventRef now = CGEventCreate(nullptr);
  const CGPoint cursor = CGEventGetLocation(now);
  CFRelease(now);

  CGEventRef up = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseUp, cursor, kCGMouseButtonLeft);
  CGEventPost(kCGHIDEventTap, up);
  CFRelease(up);
}

bool launchApp(const App &app, const QString &target)
{
  for (const auto &bundleId : app.macBundleIds) {
    QStringList arguments = {QStringLiteral("-b"), bundleId};
    if (!target.isEmpty()) {
      arguments.append(target);
    }
    if (runOpen(arguments)) {
      return true;
    }
  }
  return false;
}

bool openWithDefaultApp(const QString &target)
{
  return !target.isEmpty() && runOpen({target});
}

} // namespace deskflow::handoff
