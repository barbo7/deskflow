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
  post([this, target] {
    const auto captured = captureFrontApp();
    if (captured.isEmpty()) {
      const auto name = captured.displayName.isEmpty() ? QStringLiteral("This app") : captured.displayName;
      LOG_INFO("app handoff: nothing to send from \"%s\"", qPrintable(name));
      tellUser(QStringLiteral("%1 can't be moved: it isn't available on the other computer").arg(name));
      return;
    }

    auto *info = new HandoffCapture;
    info->m_target = target;
    info->m_appId = captured.appId.toStdString();
    info->m_url = captured.url.toStdString();

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
      return;
    }

    LOG_INFO("app handoff: sending \"%s\"", qPrintable(captured.displayName));
    m_events->addEvent(Event(EventTypes::AppHandoffCaptured, m_eventTarget, info));

    // it's only a move for a page: documents may have unsaved work, so they stay open
    if (!captured.url.isEmpty()) {
      closeCapturedTab(captured);
    }
  });
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

  if (size > kMaxFileSize) {
    LOG_WARN("app handoff: refusing a file of %u bytes", size);
    return;
  }

  if (size == 0) {
    post([appId, url, fileName] { open(appId, url, fileName, {}); });
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
    post([appId = m_appId, url = m_url, fileName = m_fileName, contents = std::move(m_buffer)] {
      open(appId, url, fileName, contents);
    });
    reset();
  }
}

void AppHandoffEngine::open(
    const std::string &appId, const std::string &url, const std::string &fileName, const std::string &data
)
{
  const auto *app = findAppById(QString::fromStdString(appId));

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
      return;
    }

    const auto folder = receiveFolder();
    if (!QDir().mkpath(folder)) {
      LOG_ERR("app handoff: can't create \"%s\"", qPrintable(folder));
      return;
    }
    target = uniqueFilePath(folder, name);
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data.data(), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size())) {
      LOG_ERR("app handoff: can't write \"%s\"", qPrintable(target));
      tellUser(QStringLiteral("Couldn't save the received file"));
      return;
    }
    file.close();
  }

  if (app == nullptr && target.isEmpty()) {
    LOG_WARN("app handoff: don't know what to do with app \"%s\"", appId.c_str());
    return;
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
}

} // namespace deskflow
