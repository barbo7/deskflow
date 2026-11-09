/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/AppHandoff.h"

#include "base/Log.h"

#include <QDir>
#include <QFileInfo>

#include <Windows.h>

#include <UIAutomation.h>
#include <objbase.h>
#include <shellapi.h>

#include <array>

namespace deskflow::handoff {

namespace {

// the file path of the program that owns a window
QString windowProgram(HWND window)
{
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (process == nullptr) {
    return {};
  }
  std::array<wchar_t, 32768> path{};
  auto size = static_cast<DWORD>(path.size());
  QString result;
  if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
    result = QString::fromWCharArray(path.data(), static_cast<int>(size));
  }
  CloseHandle(process);
  return result;
}

QString windowTitle(HWND window)
{
  std::array<wchar_t, 1024> title{};
  const int length = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
  return QString::fromWCharArray(title.data(), length);
}

// keeps COM set up for as long as this worker job runs
class ComScope
{
public:
  ComScope() : m_ok(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
  {
  }
  ~ComScope()
  {
    if (m_ok) {
      CoUninitialize();
    }
  }
  ComScope(const ComScope &) = delete;
  ComScope &operator=(const ComScope &) = delete;

private:
  bool m_ok;
};

// a browser's address bar is the first text box in its window. looking it up by name wouldn't
// work: the name is translated into the browser's language.
QString addressBarText(HWND window)
{
  const ComScope com;

  IUIAutomation *automation = nullptr;
  if (FAILED(CoCreateInstance(
          __uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation),
          reinterpret_cast<void **>(&automation)
      ))) {
    LOG_WARN("app handoff: ui automation isn't available");
    return {};
  }

  QString result;
  IUIAutomationElement *root = nullptr;
  if (SUCCEEDED(automation->ElementFromHandle(window, &root)) && root != nullptr) {
    VARIANT controlType;
    VariantInit(&controlType);
    controlType.vt = VT_I4;
    controlType.lVal = UIA_EditControlTypeId;

    IUIAutomationCondition *condition = nullptr;
    if (SUCCEEDED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, controlType, &condition))) {
      IUIAutomationElement *edit = nullptr;
      if (SUCCEEDED(root->FindFirst(TreeScope_Descendants, condition, &edit)) && edit != nullptr) {
        VARIANT value;
        VariantInit(&value);
        if (SUCCEEDED(edit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value)) && value.vt == VT_BSTR &&
            value.bstrVal != nullptr) {
          result = QString::fromWCharArray(value.bstrVal);
        }
        VariantClear(&value);
        edit->Release();
      }
      condition->Release();
    }
    root->Release();
  }
  automation->Release();
  return result.trimmed();
}

// browsers hide "https://" in the address bar
QString normalizedAddress(const QString &text)
{
  if (text.isEmpty() || text.contains(QStringLiteral("://"))) {
    return text;
  }
  return QStringLiteral("https://%1").arg(text);
}

// windows has no general way to ask a program which file it has open. but programs add the files
// they open to the user's Recent folder, so look for the one the window title names.
QString recentDocument(const QString &title)
{
  const auto name = documentNameFromWindowTitle(title);
  if (name.isEmpty()) {
    return {};
  }

  QDir recent(QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA")));
  if (!recent.cd(QStringLiteral("Microsoft/Windows/Recent"))) {
    LOG_DEBUG("app handoff: no recent folder");
    return {};
  }

  // newest first, so that of two files with one name the one just opened wins
  const auto shortcuts = recent.entryInfoList({QStringLiteral("*.lnk")}, QDir::Files, QDir::Time);
  for (const auto &shortcut : shortcuts) {
    const auto target = QFileInfo(shortcut.symLinkTarget());
    // office leaves the extension out of its titles unless explorer is set to show them
    if ((target.fileName().compare(name, Qt::CaseInsensitive) == 0 ||
         target.completeBaseName().compare(name, Qt::CaseInsensitive) == 0) &&
        target.isFile()) {
      return target.absoluteFilePath();
    }
  }
  return {};
}

bool shellOpen(const QString &program, const QString &arguments)
{
  // let whatever we start take the foreground; we're a background process
  AllowSetForegroundWindow(ASFW_ANY);
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
      nullptr, L"open", reinterpret_cast<LPCWSTR>(program.utf16()),
      arguments.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(arguments.utf16()), nullptr, SW_SHOWNORMAL
  ));
  return result > 32;
}

QString expandedPath(const QString &path)
{
  std::array<wchar_t, 32768> expanded{};
  const auto length = ExpandEnvironmentStringsW(
      reinterpret_cast<LPCWSTR>(path.utf16()), expanded.data(), static_cast<DWORD>(expanded.size())
  );
  if (length == 0 || length > expanded.size()) {
    return path;
  }
  return QString::fromWCharArray(expanded.data());
}

} // namespace

CapturedApp captureFrontApp()
{
  CapturedApp captured;

  HWND window = GetForegroundWindow();
  if (window == nullptr) {
    return captured;
  }
  captured.window = reinterpret_cast<quintptr>(window);

  const QFileInfo program(windowProgram(window));
  const auto *known = findAppByWindowsExe(program.fileName());
  captured.displayName = known != nullptr ? known->displayName : program.completeBaseName();
  if (known != nullptr) {
    captured.appId = known->id;
  }

  if (known != nullptr && known->isBrowser) {
    if (const auto address = normalizedAddress(addressBarText(window)); isAllowedUrl(address)) {
      captured.url = address;
    }
  } else {
    captured.filePath = recentDocument(windowTitle(window));
  }

  return captured;
}

void forgetCapturedApp(const CapturedApp &)
{
  // a window handle needs no releasing
}

void closeCapturedApp(const CapturedApp &app)
{
  if (!closesWithTabKey(app.appId)) {
    // what the close button sends, so an app with unsaved work can ask about it
    PostMessageW(reinterpret_cast<HWND>(app.window), WM_CLOSE, 0, 0);
    return;
  }

  // ctrl+w while the user is still holding the hotkey's modifiers would be a different shortcut
  const auto held = [] {
    for (const int key : {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN}) {
      if ((GetAsyncKeyState(key) & 0x8000) != 0) {
        return true;
      }
    }
    return false;
  };
  for (int i = 0; i < 60 && held(); ++i) {
    Sleep(50);
  }
  if (held()) {
    LOG_DEBUG("app handoff: modifiers still held, leaving the tab open");
    return;
  }

  // never close a tab in some other window
  if (GetForegroundWindow() != reinterpret_cast<HWND>(app.window)) {
    LOG_DEBUG("app handoff: the browser lost focus, leaving the tab open");
    return;
  }

  std::array<INPUT, 4> inputs{};
  for (auto &input : inputs) {
    input.type = INPUT_KEYBOARD;
  }
  inputs[0].ki.wVk = VK_CONTROL;
  inputs[1].ki.wVk = 'W';
  inputs[2].ki.wVk = 'W';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

bool isWindowBeingDragged()
{
  const int dragKey = GetSystemMetrics(SM_SWAPBUTTON) != 0 ? VK_RBUTTON : VK_LBUTTON;
  if ((GetAsyncKeyState(dragKey) & 0x8000) == 0) {
    return false;
  }

  HWND window = GetForegroundWindow();
  if (window == nullptr) {
    return false;
  }

  GUITHREADINFO info{};
  info.cbSize = sizeof(info);
  if (GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &info) && (info.flags & GUI_INMOVESIZE) != 0) {
    return true;
  }

  // browsers drag tabs themselves, so windows doesn't know about those: go by the tab strip
  const auto *known = findAppByWindowsExe(QFileInfo(windowProgram(window)).fileName());
  if (known == nullptr || !known->isBrowser) {
    return false;
  }
  RECT area;
  POINT cursor;
  if (!GetWindowRect(window, &area) || !GetCursorPos(&cursor)) {
    return false;
  }
  return PtInRect(&area, cursor) && cursor.y < area.top + 3 * GetSystemMetrics(SM_CYCAPTION);
}

void cancelWindowDrag()
{
  std::array<INPUT, 2> inputs{};
  for (auto &input : inputs) {
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_ESCAPE;
  }
  inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void releaseLeftButton()
{
  INPUT input{};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = GetSystemMetrics(SM_SWAPBUTTON) != 0 ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
  SendInput(1, &input, sizeof(INPUT));
}

bool launchApp(const App &app, const QString &target)
{
  const auto arguments =
      target.isEmpty() ? QString() : QStringLiteral("\"%1\"").arg(target.contains(QStringLiteral("://")) ? target : QDir::toNativeSeparators(target));

  for (const auto &launch : app.windowsLaunch) {
    // a uri scheme only starts the app; it can't be given a file
    if (launch.endsWith(QLatin1Char(':'))) {
      if (target.isEmpty() && shellOpen(launch, {})) {
        return true;
      }
      continue;
    }
    if (shellOpen(expandedPath(launch), arguments)) {
      return true;
    }
  }
  return false;
}

bool openWithDefaultApp(const QString &target)
{
  if (target.isEmpty()) {
    return false;
  }
  return shellOpen(target.contains(QStringLiteral("://")) ? target : QDir::toNativeSeparators(target), {});
}

} // namespace deskflow::handoff
