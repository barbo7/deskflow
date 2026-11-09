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

// long enough to answer macOS asking whether Deskflow may control the browser
const int kScriptTimeoutMs = 20000;

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

// who owns the window under the cursor, or else the frontmost ordinary window. the accessibility
// api can't name the focused app while a window is being dragged; the window server always can.
pid_t frontWindowOwner()
{
  CGEventRef now = CGEventCreate(nullptr);
  const CGPoint cursor = CGEventGetLocation(now);
  CFRelease(now);

  CFArrayRef windows = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID
  );
  if (windows == nullptr) {
    return 0;
  }

  pid_t frontmost = 0;
  pid_t underCursor = 0;
  const CFIndex count = CFArrayGetCount(windows);
  for (CFIndex i = 0; i < count && underCursor == 0; ++i) {
    auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));

    int layer = -1;
    auto layerValue = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer));
    if (layerValue == nullptr || !CFNumberGetValue(layerValue, kCFNumberIntType, &layer) || layer != 0) {
      continue;
    }

    pid_t owner = 0;
    auto ownerValue = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID));
    if (ownerValue == nullptr || !CFNumberGetValue(ownerValue, kCFNumberIntType, &owner)) {
      continue;
    }
    if (frontmost == 0) {
      frontmost = owner;
    }

    CGRect bounds;
    auto boundsValue = static_cast<CFDictionaryRef>(CFDictionaryGetValue(info, kCGWindowBounds));
    if (boundsValue != nullptr && CGRectMakeWithDictionaryRepresentation(boundsValue, &bounds) &&
        CGRectContainsPoint(CGRectInset(bounds, -2, -2), cursor)) {
      underCursor = owner;
    }
  }
  CFRelease(windows);
  return underCursor != 0 ? underCursor : frontmost;
}

// the app in front, retained; nullptr if there's none
AXUIElementRef copyFrontApp()
{
  AXUIElementRef systemWide = AXUIElementCreateSystemWide();
  CFTypeRef focused = nullptr;
  const auto error = AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute, &focused);
  CFRelease(systemWide);
  if (error == kAXErrorSuccess && focused != nullptr) {
    return static_cast<AXUIElementRef>(focused);
  }

  const pid_t owner = frontWindowOwner();
  if (owner == 0) {
    LOG_WARN("app handoff: can't find the focused app (accessibility error %d)", static_cast<int>(error));
    return nullptr;
  }
  LOG_DEBUG("app handoff: going by the window server for the app in front");
  return AXUIElementCreateApplication(owner);
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
    AXUIElementRef app = copyFrontApp();
    if (app == nullptr) {
      return captured;
    }

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

    // kept until the other computer has answered, so the right window gets closed
    CFTypeRef window = nullptr;
    if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, &window) == kAXErrorSuccess && window != nullptr) {
      captured.window = reinterpret_cast<quintptr>(window);
    }

    CFRelease(app);
  }

  return captured;
}

void closeCapturedApp(const CapturedApp &app)
{
  const auto *known = findAppById(app.appId);
  if (known != nullptr && known->isBrowser && !known->macBundleIds.isEmpty()) {
    const auto &bundleId = known->macBundleIds.first();
    if (isChromiumBundle(bundleId)) {
      runAppleScript(QStringLiteral("tell application id \"%1\" to close active tab of front window").arg(bundleId));
      return;
    }
    if (bundleId == QStringLiteral("com.apple.Safari")) {
      runAppleScript(QStringLiteral("tell application id \"%1\" to close current tab of front window").arg(bundleId));
      return;
    }
  }

  auto window = reinterpret_cast<AXUIElementRef>(app.window);
  if (window == nullptr) {
    return;
  }

  if (closesWithTabKey(app.appId)) {
    // command+W only goes to the app in front: never close a tab somewhere else
    pid_t captured = 0;
    AXUIElementGetPid(window, &captured);
    pid_t front = 0;
    if (AXUIElementRef focused = copyFrontApp(); focused != nullptr) {
      AXUIElementGetPid(focused, &front);
      CFRelease(focused);
    }
    if (front != captured) {
      LOG_DEBUG("app handoff: the app lost focus, leaving its tab open");
      return;
    }

    const CGKeyCode kKeyW = 13;
    for (const bool down : {true, false}) {
      CGEventRef key = CGEventCreateKeyboardEvent(nullptr, kKeyW, down);
      CGEventSetFlags(key, kCGEventFlagMaskCommand);
      CGEventPost(kCGHIDEventTap, key);
      CFRelease(key);
    }
    return;
  }

  // the window's own close button, so an app with unsaved work can ask about it
  CFTypeRef button = nullptr;
  if (AXUIElementCopyAttributeValue(window, kAXCloseButtonAttribute, &button) == kAXErrorSuccess && button != nullptr) {
    AXUIElementPerformAction(static_cast<AXUIElementRef>(button), kAXPressAction);
    CFRelease(button);
  }
}

void forgetCapturedApp(const CapturedApp &app)
{
  if (app.window != 0) {
    CFRelease(reinterpret_cast<CFTypeRef>(app.window));
  }
}

bool isWindowBeingDragged()
{
  if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft)) {
    LOG_INFO("app handoff: pushed against an edge, but the left button isn't down");
    return false;
  }

  CGEventRef now = CGEventCreate(nullptr);
  const CGPoint cursor = CGEventGetLocation(now);
  CFRelease(now);

  // not the accessibility api: the window server moves a dragged window by itself, and the app
  // only hears where it ended up once the drag is over
  CFArrayRef windows = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID
  );
  if (windows == nullptr) {
    return false;
  }

  // macOS can't say a window is being dragged, but a window follows the cursor by the strip
  // along its top: its title bar, or the tabs of a browser
  const double kTitleStrip = 56.0;
  bool dragged = false;
  const CFIndex count = CFArrayGetCount(windows);
  for (CFIndex i = 0; i < count; ++i) {
    auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));

    int layer = -1;
    auto layerValue = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer));
    if (layerValue == nullptr || !CFNumberGetValue(layerValue, kCFNumberIntType, &layer) || layer != 0) {
      continue; // menu bar, dock, pop-ups
    }

    CGRect bounds;
    auto boundsValue = static_cast<CFDictionaryRef>(CFDictionaryGetValue(info, kCGWindowBounds));
    if (boundsValue == nullptr || !CGRectMakeWithDictionaryRepresentation(boundsValue, &bounds)) {
      continue;
    }

    // the list runs front to back, so the first window under the cursor is the one
    if (CGRectContainsPoint(CGRectInset(bounds, -2, -2), cursor)) {
      dragged = cursor.y <= CGRectGetMinY(bounds) + kTitleStrip;
      LOG_INFO(
          "app handoff: window at %.0f,%.0f size %.0fx%.0f, cursor at %.0f,%.0f: %s", bounds.origin.x, bounds.origin.y,
          bounds.size.width, bounds.size.height, cursor.x, cursor.y, dragged ? "held by its title" : "held elsewhere"
      );
      break;
    }
  }
  CFRelease(windows);
  return dragged;
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
