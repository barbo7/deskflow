/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/EdgePushDetector.h"

#include "base/Log.h"
#include "deskflow/EdgePushAccumulator.h"

#include <ApplicationServices/ApplicationServices.h>

#include <algorithm>
#include <future>
#include <thread>
#include <unistd.h>

namespace deskflow {

namespace {

// trackpad and mouse deltas are in points
const double kPushThreshold = 80.0;

class OSXEdgePushDetector : public EdgePushDetector
{
public:
  OSXEdgePushDetector(uint32_t edges, Callback onPush)
      : m_accumulator(edges, kPushThreshold),
        m_onPush(std::move(onPush))
  {
  }

  ~OSXEdgePushDetector() override
  {
    if (m_runLoop != nullptr) {
      CFRunLoopStop(m_runLoop);
    }
    if (m_thread.joinable()) {
      m_thread.join();
    }
    if (m_tap != nullptr) {
      CFRelease(m_tap);
    }
  }

  OSXEdgePushDetector(const OSXEdgePushDetector &) = delete;
  OSXEdgePushDetector &operator=(const OSXEdgePushDetector &) = delete;

  bool start()
  {
    // listen only: the tap never holds up or changes an event
    const CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDragged) |
                             CGEventMaskBit(kCGEventRightMouseDragged) | CGEventMaskBit(kCGEventOtherMouseDragged);
    m_tap = CGEventTapCreate(
        kCGSessionEventTap, kCGTailAppendEventTap, kCGEventTapOptionListenOnly, mask, &OSXEdgePushDetector::onEvent,
        this
    );
    if (m_tap == nullptr) {
      LOG_WARN("can't watch the mouse for role swaps, event tap failed (check accessibility permission)");
      return false;
    }

    std::promise<CFRunLoopRef> started;
    auto runLoop = started.get_future();
    m_thread = std::thread([this, &started] {
      CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_tap, 0);
      CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      CGEventTapEnable(m_tap, true);
      started.set_value(CFRunLoopGetCurrent());
      CFRunLoopRun();
      CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      CFRelease(source);
    });
    m_runLoop = runLoop.get();
    LOG_DEBUG("watching the mouse for role swaps");
    return true;
  }

private:
  static CGEventRef onEvent(CGEventTapProxy, CGEventType type, CGEventRef event, void *refcon)
  {
    auto *self = static_cast<OSXEdgePushDetector *>(refcon);
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
      CGEventTapEnable(self->m_tap, true);
      return event;
    }

    // deskflow's own injected events come from this process; only the user's hand counts
    if (CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID) == getpid()) {
      return event;
    }

    self->handleMove(
        CGEventGetLocation(event), static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX)),
        static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY))
    );
    return event;
  }

  void handleMove(CGPoint location, double dx, double dy)
  {
    // the desktop's outer edges, across all displays
    CGDirectDisplayID displays[16];
    uint32_t count = 0;
    if (CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess || count == 0) {
      return;
    }
    CGRect desktop = CGDisplayBounds(displays[0]);
    for (uint32_t i = 1; i < count; ++i) {
      desktop = CGRectUnion(desktop, CGDisplayBounds(displays[i]));
    }

    EdgePushAccumulator::Edges at;
    at.left = location.x <= CGRectGetMinX(desktop) + 1;
    at.right = location.x >= CGRectGetMaxX(desktop) - 1;
    at.top = location.y <= CGRectGetMinY(desktop) + 1;
    at.bottom = location.y >= CGRectGetMaxY(desktop) - 1;

    if (m_accumulator.feed(dx, dy, at, EdgePushAccumulator::Clock::now())) {
      LOG_INFO("mouse pushed toward the server, asking to swap roles");
      m_onPush();
    }
  }

  EdgePushAccumulator m_accumulator;
  Callback m_onPush;
  CFMachPortRef m_tap = nullptr;
  CFRunLoopRef m_runLoop = nullptr;
  std::thread m_thread;
};

} // namespace

std::unique_ptr<EdgePushDetector> EdgePushDetector::create(uint32_t edges, Callback onPush)
{
  if (edges == 0) {
    return nullptr;
  }
  auto detector = std::make_unique<OSXEdgePushDetector>(edges, std::move(onPush));
  if (!detector->start()) {
    return nullptr;
  }
  return detector;
}

} // namespace deskflow
