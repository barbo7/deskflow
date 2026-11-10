/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/AppHandoffEngine.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/AppHandoff.h"
#include "deskflow/ipc/CoreIpc.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <utility>

namespace deskflow {

using namespace handoff;

namespace {

// shown by the gui; "=" would be cut off by the ipc channel
void tellUser(QString message)
{
  message.replace(QLatin1Char('='), QLatin1Char(' '));
  ipcSendToClient(QStringLiteral("handoffStatus"), message);
}

} // namespace

AppHandoffEngine::AppHandoffEngine(IEventQueue *events, void *eventTarget)
    : m_events(events),
      m_eventTarget(eventTarget),
      m_thread([this] { run(); })
{
  // do nothing
}

AppHandoffEngine::~AppHandoffEngine()
{
  {
    const std::scoped_lock lock(m_mutex);
    m_stopping = true;
    m_jobs.clear();
  }
  m_wake.notify_all();
  m_thread.join();

  if (m_sent.has_value()) {
    forgetCapturedApp(*m_sent);
  }
}

void AppHandoffEngine::post(std::function<void()> job)
{
  {
    const std::scoped_lock lock(m_mutex);
    m_jobs.push_back(std::move(job));
  }
  m_wake.notify_one();
}

void AppHandoffEngine::run()
{
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock lock(m_mutex);
      m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
      if (m_stopping) {
        return;
      }
      job = std::move(m_jobs.front());
      m_jobs.pop_front();
    }
    job();
  }
}

void AppHandoffEngine::capture(const std::string &target)
{
  post([this, target] { captureAndPost(target, false, false); });
}

void AppHandoffEngine::captureDraggedWindow(const std::string &target, bool releaseButton)
{
  post([this, target, releaseButton] { captureAndPost(target, true, releaseButton); });
}

void AppHandoffEngine::captureAndPost(const std::string &target, bool dragged, bool releaseButton)
{
  // a button held at the edge is usually something else, like selecting text
  if (dragged && !isWindowBeingDragged()) {
    if (const auto files = draggedFiles(); !files.isEmpty()) {
      sendFiles(target, files, releaseButton);
      return;
    }
    LOG_INFO("app handoff: pushed against an edge, but no window or file is being dragged");
    return;
  }

  const auto captured = captureFrontApp();
  if (captured.isEmpty()) {
    const auto name = captured.displayName.isEmpty() ? QStringLiteral("This app") : captured.displayName;
    LOG_INFO("app handoff: nothing to send from \"%s\"", qPrintable(name));
    tellUser(QStringLiteral("%1 can't be moved: it isn't available on the other computer").arg(name));
    forgetCapturedApp(captured);
    return;
  }

  auto *info = new HandoffCapture;
  info->m_target = target;
  info->m_appId = captured.appId.toStdString();
  info->m_url = captured.url.toStdString();

  // with the browser's extension there's more to send than the address
  QList<int> extensionTabs;
  const auto *known = findAppById(captured.appId);
  if (known != nullptr && known->isBrowser && ipcHasBrowserHost(captured.appId)) {
    const auto mode = dragged ? QStringLiteral("drag") : QStringLiteral("hotkey");
    const auto answer = ipcCaptureBrowserState(captured.appId, mode, captured.url, 1500);
    if (const auto state = parsePageState(answer); state.has_value()) {
      LOG_INFO("app handoff: the extension gave %d tab(s)", static_cast<int>(state->urls.size()));
      info->m_fileName = kPageStateFileName.toStdString();
      info->m_data.assign(state->json.constData(), static_cast<size_t>(state->json.size()));
      info->m_url = state->urls.first().toStdString();
      extensionTabs = state->tabIds;
    } else if (answer.isEmpty()) {
      LOG_WARN("app handoff: the browser extension didn't answer, sending only the address");
    } else {
      // what kind of answer it was, without what's on the pages
      const auto object = QJsonDocument::fromJson(answer).object();
      QStringList kinds;
      const auto tabs = object.value(QStringLiteral("tabs")).toArray();
      for (const auto &tab : tabs) {
        const auto url = tab.toObject().value(QStringLiteral("url")).toString();
        kinds.append(QUrl(url).scheme() + (isAllowedUrl(url) ? QString() : QStringLiteral("(refused)")));
      }
      const auto error = object.value(QStringLiteral("error")).toString().left(200);
      LOG_WARN(
          "app handoff: the browser extension's answer (%d bytes, %d tab(s): %s%s%s) has nothing to send, "
          "sending only the address",
          static_cast<int>(answer.size()), static_cast<int>(tabs.size()), qPrintable(kinds.join(QLatin1Char(','))),
          error.isEmpty() ? "" : ", error: ", qPrintable(error)
      );
    }
  }

  if (!captured.filePath.isEmpty()) {
    QFile file(captured.filePath);
    if (file.size() > kMaxFileSize) {
      LOG_WARN("app handoff: \"%s\" is too large to send", qPrintable(captured.filePath));
      tellUser(QStringLiteral("%1 is too large to send, opening the app only").arg(QFileInfo(file).fileName()));
    } else if (file.open(QIODevice::ReadOnly)) {
      const auto bytes = file.readAll();
      info->m_data.assign(bytes.constData(), static_cast<size_t>(bytes.size()));
      info->m_fileName = QFileInfo(file).fileName().toStdString();
    } else {
      LOG_WARN("app handoff: can't read \"%s\"", qPrintable(captured.filePath));
    }
  }

  if (info->m_appId.empty() && info->m_url.empty() && info->m_fileName.empty()) {
    delete info;
    forgetCapturedApp(captured);
    return;
  }

  // close it afterwards only if what's in it really went along
  const bool sentAll = !info->m_url.empty() || !info->m_fileName.empty() || hasNoDocuments(captured.appId);
  if (m_sent.has_value()) {
    forgetCapturedApp(*m_sent);
  }
  m_sent = captured;
  m_sentAll = sentAll;
  m_sentTabs = extensionTabs;
  m_sentFiles = false;

  if (dragged) {
    cancelWindowDrag();
    if (releaseButton) {
      releaseLeftButton();
    }
  }

  LOG_INFO("app handoff: sending \"%s\"", qPrintable(captured.displayName));
  m_events->addEvent(Event(EventTypes::AppHandoffCaptured, m_eventTarget, info));
}

void AppHandoffEngine::sendFiles(const std::string &target, const QStringList &paths, bool releaseButton)
{
  // take the drag back before anything else: packing can take a moment, and letting go in the
  // meantime would drop the files somewhere on this computer
  cancelFileDrag();
  if (releaseButton) {
    releaseLeftButton();
  }

  LOG_INFO("app handoff: sending %d dragged item(s)", static_cast<int>(paths.size()));
  const auto bundle = packFiles(paths);
  if (bundle.data.isEmpty()) {
    LOG_WARN("app handoff: can't send the dragged files: %s", qPrintable(bundle.error));
    if (bundle.error == QStringLiteral("too large") || bundle.error == QStringLiteral("too many files")) {
      tellUser(
          QStringLiteral("Too much to send at once: at most %1 MB and %2 files")
              .arg(kMaxBundleSize / (1024 * 1024))
              .arg(kMaxBundleEntries)
      );
    } else {
      tellUser(QStringLiteral("Couldn't send those files: %1").arg(bundle.error));
    }
    return;
  }

  auto *info = new HandoffCapture;
  info->m_target = target;
  info->m_fileName = kFilesBundleName.toStdString();
  info->m_data.assign(bundle.data.constData(), static_cast<size_t>(bundle.data.size()));

  if (m_sent.has_value()) {
    forgetCapturedApp(*m_sent);
  }
  CapturedApp files;
  files.displayName = QStringLiteral("%1 file(s)").arg(bundle.files);
  m_sent = files;
  m_sentAll = true;
  m_sentTabs.clear();
  m_sentFiles = true;

  LOG_INFO(
      "app handoff: sending %d file(s), %d bytes", bundle.files, static_cast<int>(bundle.data.size())
  );
  m_events->addEvent(Event(EventTypes::AppHandoffCaptured, m_eventTarget, info));
}

bool AppHandoffEngine::openFiles(const std::string &data)
{
  const auto folder = receiveFolder();
  const auto written = unpackFiles(QByteArray::fromStdString(data), folder);
  if (written.isEmpty()) {
    LOG_WARN("app handoff: couldn't save the received files");
    tellUser(QStringLiteral("Couldn't save the received files"));
    return false;
  }
  LOG_INFO("app handoff: saved %d item(s) in \"%s\"", static_cast<int>(written.size()), qPrintable(folder));
  revealFiles(written);
  tellUser(QStringLiteral("Received %1 item(s), in Downloads, Deskflow Handoff").arg(written.size()));
  return true;
}

void AppHandoffEngine::delivered(bool opened)
{
  post([this, opened] { finishPending(opened); });
}

void AppHandoffEngine::finishPending(bool opened)
{
  if (!m_sent.has_value()) {
    return;
  }
  const auto sent = *m_sent;
  m_sent.reset();

  // copies went: the originals stay where they are either way
  if (m_sentFiles) {
    m_sentFiles = false;
    if (opened) {
      LOG_INFO("app handoff: %s arrived on the other computer", qPrintable(sent.displayName));
    } else {
      LOG_WARN("app handoff: the other computer couldn't save %s", qPrintable(sent.displayName));
      tellUser(QStringLiteral("The other computer couldn't save %1").arg(sent.displayName));
    }
    return;
  }

  if (!opened) {
    LOG_WARN("app handoff: the other computer couldn't open \"%s\", it stays here", qPrintable(sent.displayName));
    tellUser(QStringLiteral("%1 couldn't be opened on the other computer, it stays here").arg(sent.displayName));
  } else if (!m_sentAll) {
    LOG_INFO("app handoff: \"%s\" opened over there without its page, so it stays here", qPrintable(sent.displayName));
    tellUser(QStringLiteral("%1 opened, but its page or document couldn't be sent, so it stays here").arg(sent.displayName));
  } else if (!m_sentTabs.isEmpty()) {
    LOG_INFO("app handoff: opened over there, closing %d tab(s) here", static_cast<int>(m_sentTabs.size()));
    ipcCloseBrowserTabs(sent.appId, m_sentTabs);
  } else {
    LOG_INFO("app handoff: opened over there, closing \"%s\" here", qPrintable(sent.displayName));
    closeCapturedApp(sent);
  }
  forgetCapturedApp(sent);
}

void AppHandoffEngine::reset()
{
  m_receiving = false;
  m_expected = 0;
  m_appId.clear();
  m_url.clear();
  m_fileName.clear();
  m_buffer.clear();
}

void AppHandoffEngine::begin(
    const std::string &appId, const std::string &url, const std::string &fileName, uint32_t size
)
{
  reset();

  // a bundle of dragged files may be bigger than a single document
  const uint32_t limit =
      QString::fromStdString(fileName) == kFilesBundleName ? kMaxBundleSize + 1024 * 1024 : kMaxFileSize;
  if (size > limit) {
    LOG_WARN("app handoff: refusing a file of %u bytes", size);
    return;
  }

  if (size == 0) {
    post([this, appId, url, fileName] { openAndReport(appId, url, fileName, {}); });
    return;
  }

  m_appId = appId;
  m_url = url;
  m_fileName = fileName;
  m_expected = size;
  m_buffer.reserve(size);
  m_receiving = true;
}

void AppHandoffEngine::data(const std::string &chunk)
{
  if (!m_receiving) {
    return;
  }

  if (m_buffer.size() + chunk.size() > m_expected) {
    LOG_WARN("app handoff: received more data than announced, dropping the transfer");
    reset();
    return;
  }

  m_buffer += chunk;
  if (m_buffer.size() == m_expected) {
    post([this, appId = m_appId, url = m_url, fileName = m_fileName, contents = std::move(m_buffer)] {
      openAndReport(appId, url, fileName, contents);
    });
    reset();
  }
}

void AppHandoffEngine::openAndReport(
    const std::string &appId, const std::string &url, const std::string &fileName, const std::string &data
)
{
  m_events->addEvent(
      Event(EventTypes::AppHandoffOpened, m_eventTarget, new HandoffResult(open(appId, url, fileName, data)))
  );
}

bool AppHandoffEngine::openPageState(const App *app, const std::string &data)
{
  const auto state = parsePageState(QByteArray::fromStdString(data));
  if (!state.has_value()) {
    LOG_WARN("app handoff: refusing page state that isn't usable");
    return false;
  }

  if (app != nullptr && ipcHasBrowserHost(app->id)) {
    LOG_INFO(
        "app handoff: opening %d tab(s) in %s with their page state", static_cast<int>(state->urls.size()),
        qPrintable(app->displayName)
    );
    ipcRestoreBrowserTabs(app->id, state->json);
    tellUser(QStringLiteral("Opened in %1").arg(app->displayName));
    return true;
  }

  // no extension here: the addresses are all that can be used
  LOG_INFO("app handoff: no browser extension here, opening only the addresses");
  bool opened = false;
  for (const auto &url : state->urls) {
    opened = ((app != nullptr && launchApp(*app, url)) || openWithDefaultApp(url)) || opened;
  }
  return opened;
}

bool AppHandoffEngine::open(
    const std::string &appId, const std::string &url, const std::string &fileName, const std::string &data
)
{
  if (QString::fromStdString(fileName) == kFilesBundleName) {
    return openFiles(data);
  }

  const auto *app = findAppById(QString::fromStdString(appId));

  if (QString::fromStdString(fileName) == kPageStateFileName) {
    return openPageState(app, data);
  }

  QString target;
  bool defaultAppAllowed = false;

  if (const auto address = QString::fromStdString(url); isAllowedUrl(address)) {
    target = address;
    defaultAppAllowed = true;
  } else if (!fileName.empty()) {
    const auto name = safeFileName(QString::fromStdString(fileName));
    defaultAppAllowed = canOpenWithDefaultApp(name);

    // an app we know opens the file as a document; the system's choice might run it instead
    if (name.isEmpty() || (app == nullptr && !defaultAppAllowed)) {
      LOG_WARN("app handoff: refusing file \"%s\"", fileName.c_str());
      tellUser(QStringLiteral("Refused a file that isn't a document"));
      return false;
    }

    const auto folder = receiveFolder();
    if (!QDir().mkpath(folder)) {
      LOG_ERR("app handoff: can't create \"%s\"", qPrintable(folder));
      return false;
    }
    target = uniqueFilePath(folder, name);
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data.data(), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size())) {
      LOG_ERR("app handoff: can't write \"%s\"", qPrintable(target));
      tellUser(QStringLiteral("Couldn't save the received file"));
      return false;
    }
    file.close();
  }

  if (app == nullptr && target.isEmpty()) {
    LOG_WARN("app handoff: don't know what to do with app \"%s\"", appId.c_str());
    return false;
  }

  bool opened = app != nullptr && launchApp(*app, target);
  if (!opened && defaultAppAllowed) {
    opened = openWithDefaultApp(target);
  }

  const auto label = app != nullptr ? app->displayName : QFileInfo(target).fileName();
  if (opened) {
    LOG_INFO("app handoff: opened \"%s\"", qPrintable(label));
    tellUser(QStringLiteral("Opened %1").arg(label));
  } else {
    LOG_WARN("app handoff: couldn't open \"%s\"", qPrintable(label));
    tellUser(QStringLiteral("%1 isn't installed on this computer").arg(label));
  }
  return opened;
}

} // namespace deskflow
