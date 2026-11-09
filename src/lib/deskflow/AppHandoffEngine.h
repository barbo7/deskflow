/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class IEventQueue;

namespace deskflow {

//! Event data for EventTypes::AppHandoffCaptured.
class HandoffCapture : public EventData
{
public:
  ~HandoffCapture() override = default;

  std::string m_target; //!< computer to send to; empty lets the server pick
  std::string m_appId;
  std::string m_url;
  std::string m_fileName;
  std::string m_data; //!< contents of the file
};

/**
 * The part of moving an app that's the same on a server and on a client: finding what's in front
 * and opening what arrives. Slow work runs on a thread of its own, one job at a time, so that
 * the event thread keeps the mouse and keyboard moving.
 */
class AppHandoffEngine
{
public:
  /**
   * \param events Queue that gets EventTypes::AppHandoffCaptured.
   * \param eventTarget Target of those events.
   */
  AppHandoffEngine(IEventQueue *events, void *eventTarget);
  ~AppHandoffEngine();

  AppHandoffEngine(const AppHandoffEngine &) = delete;
  AppHandoffEngine &operator=(const AppHandoffEngine &) = delete;

  //! Looks at the app in front and posts an AppHandoffCaptured event if there's something to send.
  void capture(const std::string &target);

  //! Starts receiving. With \p size 0 this opens the app straight away.
  void begin(const std::string &appId, const std::string &url, const std::string &fileName, uint32_t size);

  //! The next piece of the file announced by begin().
  void data(const std::string &chunk);

private:
  void post(std::function<void()> job);
  void run();
  void reset();
  static void open(const std::string &appId, const std::string &url, const std::string &fileName, const std::string &data);

  IEventQueue *m_events;
  void *m_eventTarget;

  // incoming transfer; only touched on the event thread
  std::string m_appId;
  std::string m_url;
  std::string m_fileName;
  std::string m_buffer;
  uint32_t m_expected = 0;
  bool m_receiving = false;

  std::mutex m_mutex;
  std::condition_variable m_wake;
  std::deque<std::function<void()>> m_jobs;
  bool m_stopping = false;
  std::thread m_thread;
};

} // namespace deskflow
