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

#include <future>
#include <thread>
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

    case WM_TIMER:
      if (self != nullptr && wParam == kDesktopTimer) {
        self->checkInputDesktop();
      }
      return 0;

    case WM_CLOSE: {
      KillTimer(window, kDesktopTimer);
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

  static constexpr UINT_PTR kDesktopTimer = 1;

  uint32_t m_edges;
  EdgePushAccumulator m_accumulator;
  Callback m_onPush;
  Callback m_onInputBlocked;
  bool m_inputBlocked = false;
  HWND m_window = nullptr;
  std::thread m_thread;
};

} // namespace

std::unique_ptr<EdgePushDetector> EdgePushDetector::create(uint32_t edges, Callback onPush, Callback onInputBlocked)
{
  // runs even with no edges to watch, to notice screens deskflow can't control
  auto detector = std::make_unique<MSWindowsEdgePushDetector>(edges, std::move(onPush), std::move(onInputBlocked));
  if (!detector->start()) {
    return nullptr;
  }
  return detector;
}

} // namespace deskflow
