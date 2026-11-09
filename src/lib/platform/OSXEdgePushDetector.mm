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
#include <chrono>
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
      : m_edges(edges),
        m_accumulator(edges, kPushThreshold),
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
    if (m_keyTap != nullptr) {
      CFRelease(m_keyTap);
    }
  }

  OSXEdgePushDetector(const OSXEdgePushDetector &) = delete;
  OSXEdgePushDetector &operator=(const OSXEdgePushDetector &) = delete;

  void setDragCallback(uint32_t dragEdges, DragCallback onDragPush)
  {
    m_dragAccumulator = EdgePushAccumulator(dragEdges, kPushThreshold);
    m_onDragPush = std::move(onDragPush);
  }

  void setHandoffCallback(Callback onHandoff)
  {
    m_onHandoff = std::move(onHandoff);
  }

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

    // a separate tap for the keyboard: if it isn't allowed, mouse pushes still work
    m_keyTap = CGEventTapCreate(
        kCGSessionEventTap, kCGTailAppendEventTap, kCGEventTapOptionListenOnly, CGEventMaskBit(kCGEventKeyDown),
        &OSXEdgePushDetector::onEvent, this
    );
    if (m_keyTap == nullptr) {
      LOG_WARN("can't watch the keyboard for the role swap hotkey");
    }

    std::promise<CFRunLoopRef> started;
    auto runLoop = started.get_future();
    m_thread = std::thread([this, &started] {
      CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_tap, 0);
      CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      CGEventTapEnable(m_tap, true);
      CFRunLoopSourceRef keySource = nullptr;
      if (m_keyTap != nullptr) {
        keySource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_keyTap, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), keySource, kCFRunLoopCommonModes);
        CGEventTapEnable(m_keyTap, true);
      }
      started.set_value(CFRunLoopGetCurrent());
      CFRunLoopRun();
      CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      CFRelease(source);
      if (keySource != nullptr) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), keySource, kCFRunLoopCommonModes);
        CFRelease(keySource);
      }
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
      if (self->m_keyTap != nullptr) {
        CGEventTapEnable(self->m_keyTap, true);
      }
      return event;
    }

    // deskflow's own injected events come from this process; only the user's hand counts
    if (CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID) == getpid()) {
      return event;
    }

    if (type == kCGEventKeyDown) {
      self->handleKey(event);
      return event;
    }

    self->handleMove(
        CGEventGetLocation(event), static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX)),
        static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY)),
        type == kCGEventLeftMouseDragged
    );
    return event;
  }

  void handleKey(CGEventRef event)
  {
    // control+option+command+arrow toward the server, without shift
    const CGEventFlags wanted = kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | kCGEventFlagMaskCommand;
    const CGEventFlags flags = CGEventGetFlags(event);
    if ((flags & wanted) != wanted || (flags & kCGEventFlagMaskShift) != 0) {
      return;
    }
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) {
      return;
    }

    if (CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode) == 4) { // H
      if (m_onHandoff) {
        LOG_INFO("handoff hotkey pressed");
        m_onHandoff();
      }
      return;
    }

    uint32_t edge = 0;
    switch (CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode)) {
    case 123: // left arrow
      edge = static_cast<uint32_t>(DirectionMask::LeftMask);
      break;
    case 124: // right arrow
      edge = static_cast<uint32_t>(DirectionMask::RightMask);
      break;
    case 125: // down arrow
      edge = static_cast<uint32_t>(DirectionMask::BottomMask);
      break;
    case 126: // up arrow
      edge = static_cast<uint32_t>(DirectionMask::TopMask);
      break;
    default:
      return;
    }
    if ((m_edges & edge) == 0) {
      return;
    }

    // one swap per press; the swap restarts deskflow, so ignore presses while it does
    const auto now = EdgePushAccumulator::Clock::now();
    if (now - m_lastHotKey < std::chrono::seconds(5)) {
      return;
    }
    m_lastHotKey = now;
    LOG_INFO("hotkey pressed toward the server, asking to swap roles");
    m_onPush();
  }

  void handleMove(CGPoint location, double dx, double dy, bool leftButtonHeld)
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

    if (leftButtonHeld) {
      if (m_onDragPush && m_dragAccumulator.feed(dx, dy, at, EdgePushAccumulator::Clock::now())) {
        LOG_INFO("pushed against an edge with the left button held");
        m_onDragPush(EdgePushAccumulator::pushDirection(dx, dy, at));
      }
      return;
    }

    if (m_accumulator.feed(dx, dy, at, EdgePushAccumulator::Clock::now())) {
      LOG_INFO("mouse pushed toward the server, asking to swap roles");
      m_onPush();
    }
  }

  uint32_t m_edges;
  EdgePushAccumulator m_accumulator;
  Callback m_onPush;
  EdgePushAccumulator::Clock::time_point m_lastHotKey;
  EdgePushAccumulator m_dragAccumulator{0, kPushThreshold};
  DragCallback m_onDragPush;
  Callback m_onHandoff;
  CFMachPortRef m_tap = nullptr;
  CFMachPortRef m_keyTap = nullptr;
  CFRunLoopRef m_runLoop = nullptr;
  std::thread m_thread;
};

} // namespace

std::unique_ptr<EdgePushDetector> EdgePushDetector::create(
    uint32_t edges, Callback onPush, Callback, Callback onHandoff, uint32_t dragEdges, DragCallback onDragPush
)
{
  if (edges == 0 && dragEdges == 0) {
    return nullptr;
  }
  auto detector = std::make_unique<OSXEdgePushDetector>(edges, std::move(onPush));
  detector->setHandoffCallback(std::move(onHandoff));
  detector->setDragCallback(dragEdges, std::move(onDragPush));
  if (!detector->start()) {
    return nullptr;
  }
  return detector;
}

} // namespace deskflow
