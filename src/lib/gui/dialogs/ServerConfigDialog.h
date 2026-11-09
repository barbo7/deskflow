/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ComputerSetupModel.h"
#include "common/NetworkProtocol.h"
#include "config/ServerConfig.h"

#include <QDialog>
#include <QSet>

class SettingsDialogButtonBox;
class MonitorLayoutView;
class QCheckBox;
class QLabel;
class QItemSelection;

namespace Ui {
class ServerConfigDialog;
}

class ServerConfigDialog : public QDialog
{
  Q_OBJECT

public:
  ServerConfigDialog(QWidget *parent, ServerConfig &config);
  ~ServerConfigDialog() override;
  bool addClient(const QString &clientName);
  void message(const QString &message)
  {
    m_message = message;
  }

  /**
   * What's connected right now, for the Monitors tab. Can be called while the dialog is open.
   * \param monitors The monitors of each connected computer.
   * \param connected Names of the connected computers.
   * \param active Name of the computer that has the cursor.
   */
  void setMonitorState(const deskflow::MonitorMap &monitors, const QSet<QString> &connected, const QString &active);

  //! After saving: whether the running server can take the change without dropping connections.
  [[nodiscard]] bool appliesLive() const
  {
    return m_appliesLive;
  }

protected:
  void onComputerRemoved();
  void addClient();
  bool addComputer(const QString &clientName, bool doSilent);

  void addHotkey();
  void editHotkey();
  void removeHotkey();
  void listHotkeysSelectionChanged(const QItemSelection &selected, [[maybe_unused]] const QItemSelection &deselected);

  void addAction();
  void editAction();
  void removeAction();
  void listActionsSelectionChanged(const QItemSelection &selected, [[maybe_unused]] const QItemSelection &deselected);

  void toggleSwitchDoubleTap(bool enable);
  void setSwitchDoubleTap(int within);

  void toggleSwitchDelay(bool enable);
  void setSwitchDelay(int delay);

  void toggleDefaultLockToComputerState(bool state);
  void toggleLockToComputer(bool disabled);
  void toggleWin32Foreground(bool enabled);

  void toggleClipboard(bool enabled);
  void setClipboardLimit(int limit);

  void toggleHeartbeat(bool enabled);
  void setHeartbeat(int rate);

  void toggleRelativeMouseMoves(bool enabled);
  void toggleProtocol();

  void toggleExternalConfig(bool enable = false);
  bool browseConfigFile();

  ServerConfig &serverConfig()
  {
    return m_serverConfig;
  }
  void setOriginalServerConfig(const ServerConfig &s)
  {
    m_originalServerConfig = s;
  }
  ComputerSetupModel &model()
  {
    return m_computerSetupModel;
  }

private:
  void save();
  void cancel();
  void loadFromConfig();
  void resetFromSettings();
  void refreshControls();
  void initConnections() const;
  void updateControls() const;
  void restoreFromDefaults();
  void setServerConfig();
  bool isGeneralConfigModified() const;
  bool isGeneralConfigDefault() const;
  void setButtonBoxEnabledButtons() const;
  void setupMonitorTab();
  void syncPlacements();
  void updateMonitorWarning();
  [[nodiscard]] bool onlyLayoutChanged() const;
  std::unique_ptr<Ui::ServerConfigDialog> ui;
  QString m_message = "";
  int m_columns;
  int m_rows;
  ServerConfig &m_originalServerConfig;
  NetworkProtocol m_protocol;
  bool m_enableClipboard;
  bool m_enableHeartbeat;
  int m_heartbeatRate;
  int m_switchDelay;
  int m_switchDoubleTap;
  uint m_clipboardSize;
  bool m_relativeMouseMoves;
  bool m_enableSwitchDelay;
  bool m_enableSwitchDoubleTap;
  bool m_originalServerConfigIsExternal;
  bool m_win32keepForeground;
  bool m_disableLockToComputer;
  bool m_defaultLockToComputerState;
  QString m_originalServerConfigUsesExternalFile;
  ServerConfig m_serverConfig;
  ComputerSetupModel m_computerSetupModel;
  SettingsDialogButtonBox *m_buttonBox = nullptr;
  QWidget *m_monitorTab = nullptr;
  QCheckBox *m_useMonitorLayout = nullptr;
  MonitorLayoutView *m_monitorView = nullptr;
  QLabel *m_monitorWarning = nullptr;
  deskflow::MonitorMap m_monitors;
  QSet<QString> m_connected;
  QString m_active;
  bool m_appliesLive = false;
};
