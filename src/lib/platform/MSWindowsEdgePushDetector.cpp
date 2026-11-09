/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/EdgePushDetector.h"

#include "base/Log.h"
#include "deskflow/EdgePushAccumulator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <vector>

namespace deskflow {

namespace {

// raw mouse counts: a deliberate push of a few millimetres on a typical mouse
const double kPushThreshold = 200.0;

const wchar_t *const kWindowClass = L"DeskflowEdgePushDetector";

class MSWindowsEdgePushDetector : public EdgePushDetector
{
public:
  MSWindowsEdgePushDetector(uint32_t edges, Callback onPush, Callback onInputBlocked)
      : m_edges(edges),
        m_accumulator(edges, kPushThreshold),
        m_onPush(std::move(onPush)),
        m_onInputBlocked(std::move(onInputBlocked))
  {
  }

  void setHandoffCallback(Callback onHandoff)
  {
    m_onHandoff = std::move(onHandoff);
  }

  ~MSWindowsEdgePushDetector() override
  {
    if (m_window != nullptr) {
      PostMessageW(m_window, WM_CLOSE, 0, 0);
    }
    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

  MSWindowsEdgePushDetector(const MSWindowsEdgePushDetector &) = delete;
  MSWindowsEdgePushDetector &operator=(const MSWindowsEdgePushDetector &) = delete;

  bool start()
  {
    std::promise<HWND> started;
    auto window = started.get_future();
    m_thread = std::thread([this, &started] { run(started); });
    m_window = window.get();
    if (m_window == nullptr) {
      m_thread.join();
      LOG_WARN("can't watch the mouse for role swaps, raw input registration failed");
      return false;
    }
    LOG_DEBUG("watching the mouse for role swaps");
    return true;
  }

private:
  void run(std::promise<HWND> &started)
  {
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &MSWindowsEdgePushDetector::windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass); // fails harmlessly if already registered

    // a message-only window: never shown, only receives raw input
    HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (window == nullptr) {
      started.set_value(nullptr);
      return;
    }
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // generic desktop page, mouse usage; input sink: receive input even when not in the foreground
    RAWINPUTDEVICE device = {};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = window;
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
      DestroyWindow(window);
      started.set_value(nullptr);
      return;
    }

    // check now and then whether we can still control the input desktop
    SetTimer(window, kDesktopTimer, 500, nullptr);

    // ctrl+alt+win+arrow toward the server: the keyboard way to take over as server
    for (const auto &[mask, key] : kArrowKeys) {
      if ((m_edges & mask) != 0 &&
          !RegisterHotKey(window, static_cast<int>(key), MOD_CONTROL | MOD_ALT | MOD_WIN | MOD_NOREPEAT, key)) {
        LOG_WARN("can't register the ctrl+alt+win+arrow hotkey, another app may be using it");
      }
    }

    if (!RegisterHotKey(window, kHandoffHotKey, MOD_CONTROL | MOD_ALT | MOD_WIN | MOD_NOREPEAT, 'H')) {
      LOG_WARN("can't register the ctrl+alt+win+H hotkey, another app may be using it");
    }

    started.set_value(window);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }

  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    auto *self = reinterpret_cast<MSWindowsEdgePushDetector *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_INPUT:
      if (self != nullptr) {
        self->handleRawInput(reinterpret_cast<HRAWINPUT>(lParam));
      }
      break;

    case WM_HOTKEY:
      if (self != nullptr && wParam == static_cast<WPARAM>(kHandoffHotKey)) {
        LOG_INFO("handoff hotkey pressed");
        self->m_onHandoff();
      } else if (self != nullptr) {
        self->handleHotKey();
      }
      return 0;

    case WM_TIMER:
      if (self != nullptr && wParam == kDesktopTimer) {
        self->checkInputDesktop();
      }
      return 0;

    case WM_CLOSE: {
      KillTimer(window, kDesktopTimer);
      UnregisterHotKey(window, kHandoffHotKey);
      for (const auto &arrow : kArrowKeys) {
        UnregisterHotKey(window, static_cast<int>(arrow.second));
      }
      RAWINPUTDEVICE device = {};
      device.usUsagePage = 0x01;
      device.usUsage = 0x02;
      device.dwFlags = RIDEV_REMOVE;
      device.hwndTarget = nullptr;
      RegisterRawInputDevices(&device, 1, sizeof(device));
      DestroyWindow(window);
      return 0;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }

  void handleHotKey()
  {
    // one swap per press; the swap restarts deskflow, so ignore presses while it does
    const auto now = EdgePushAccumulator::Clock::now();
    if (now - m_lastHotKey < std::chrono::seconds(5)) {
      return;
    }
    m_lastHotKey = now;
    LOG_INFO("hotkey pressed toward the server, asking to swap roles");
    m_onPush();
  }

  void checkInputDesktop()
  {
    // an elevation prompt (UAC), the lock screen and the like run on a secure desktop that a
    // desktop-mode process can't open or inject into. if the server's cursor is here then, the
    // user is stuck: hand the cursor back to the server.
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    const bool blocked = desktop == nullptr;
    if (desktop != nullptr) {
      CloseDesktop(desktop);
    }

    if (blocked && !m_inputBlocked) {
      LOG_INFO("can't control the current screen (e.g. an elevation prompt), returning the cursor to the server");
      m_onInputBlocked();
    }
    m_inputBlocked = blocked;
  }

  void handleRawInput(HRAWINPUT handle)
  {
    UINT size = 0;
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
      return;
    }
    std::vector<BYTE> buffer(size);
    if (GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
      return;
    }
    const auto *input = reinterpret_cast<const RAWINPUT *>(buffer.data());

    // injected input (e.g. from deskflow itself) has no device; only the user's hand counts
    if (m_edges == 0 || input->header.dwType != RIM_TYPEMOUSE || input->header.hDevice == nullptr) {
      return;
    }
    const RAWMOUSE &mouse = input->data.mouse;
    if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
      return;
    }

    POINT cursor;
    if (!GetCursorPos(&cursor)) {
      return;
    }
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1;
    const int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

    // a game (or any app) that confines the cursor keeps it: never swap roles mid-game
    RECT clip;
    if (GetClipCursor(&clip) && (clip.left > left || clip.top > top || clip.right <= right || clip.bottom <= bottom)) {
      return;
    }

    EdgePushAccumulator::Edges at;
    at.left = cursor.x <= left;
    at.right = cursor.x >= right;
    at.top = cursor.y <= top;
    at.bottom = cursor.y >= bottom;

    const auto dx = static_cast<double>(mouse.lLastX);
    const auto dy = static_cast<double>(mouse.lLastY);
    if (m_accumulator.feed(dx, dy, at, EdgePushAccumulator::Clock::now())) {
      LOG_INFO("mouse pushed toward the server, asking to swap roles");
      m_onPush();
    }
  }

  // arrow hotkeys use their virtual key as their id, so the letter can't clash with them
  static constexpr int kHandoffHotKey = 'H';
  static constexpr UINT_PTR kDesktopTimer = 1;
  // DirectionMask bit and virtual key of each arrow
  static constexpr std::pair<uint32_t, UINT> kArrowKeys[] = {
      {static_cast<uint32_t>(DirectionMask::LeftMask), VK_LEFT},
      {static_cast<uint32_t>(DirectionMask::RightMask), VK_RIGHT},
      {static_cast<uint32_t>(DirectionMask::TopMask), VK_UP},
      {static_cast<uint32_t>(DirectionMask::BottomMask), VK_DOWN},
  };

  uint32_t m_edges;
  EdgePushAccumulator m_accumulator;
  Callback m_onPush;
  Callback m_onHandoff;
  Callback m_onInputBlocked;
  bool m_inputBlocked = false;
  EdgePushAccumulator::Clock::time_point m_lastHotKey;
  HWND m_window = nullptr;
  std::thread m_thread;
};

} // namespace

std::unique_ptr<EdgePushDetector>
EdgePushDetector::create(uint32_t edges, Callback onPush, Callback onInputBlocked, Callback onHandoff)
{
  // runs even with no edges to watch, to notice screens deskflow can't control
  auto detector = std::make_unique<MSWindowsEdgePushDetector>(edges, std::move(onPush), std::move(onInputBlocked));
  detector->setHandoffCallback(std::move(onHandoff));
  if (!detector->start()) {
    return nullptr;
  }
  return detector;
}

} // namespace deskflow
