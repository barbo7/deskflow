/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// The browser starts this program for the Deskflow extension, and talks to it through stdin and
// stdout. It passes messages on to the Deskflow core over the core's local socket. The core is
// restarted by every role swap, so it keeps trying to reconnect for as long as the browser runs.

#include "common/Constants.h"
#include "common/VersionInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMetaObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <cstdint>
#include <cstdio>
#include <thread>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace {

const auto kHostName = QStringLiteral("com.deskflow.handoff");
const uint32_t kMaxMessage = 8 * 1024 * 1024;
const auto kBase64 = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;

void writeToBrowser(const QJsonObject &object)
{
  const auto json = QJsonDocument(object).toJson(QJsonDocument::Compact);
  const auto size = static_cast<uint32_t>(json.size());
  // native byte order, which the browser's protocol asks for
  std::fwrite(&size, sizeof(size), 1, stdout);
  std::fwrite(json.constData(), 1, json.size(), stdout);
  std::fflush(stdout);
}

class Bridge : public QObject
{
public:
  explicit Bridge(const QString &socketName) : m_socketName(socketName)
  {
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
      // before hello: the core gives what it queued for the gui to the first client that says hello
      write(QStringLiteral("browserHost"));
      write(QStringLiteral("hello=%1+%2").arg(kVersion, kVersionGitSha));
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] { readCore(); });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] { tellStatus(false); });
    connect(&m_retry, &QTimer::timeout, this, [this] {
      if (m_socket.state() == QLocalSocket::UnconnectedState) {
        m_ready = false;
        m_buffer.clear();
        m_socket.connectToServer(m_socketName);
      }
    });
    m_retry.start(1000);
    m_socket.connectToServer(m_socketName);
  }

  void fromBrowser(const QByteArray &json)
  {
    const auto object = QJsonDocument::fromJson(json).object();
    const auto type = object.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("hello")) {
      m_browser = object.value(QStringLiteral("browser")).toString();
      announce();
      tellStatus(m_ready);
    } else if (type == QStringLiteral("captured") && m_ready) {
      const auto state = QJsonDocument(object.value(QStringLiteral("state")).toObject()).toJson(QJsonDocument::Compact);
      write(QStringLiteral("browserCaptured=%1;%2")
                .arg(object.value(QStringLiteral("id")).toInt())
                .arg(QString::fromLatin1(state.toBase64(kBase64))));
    }
  }

private:
  void write(const QString &line)
  {
    m_socket.write(line.toUtf8() + '\n');
    m_socket.flush();
  }

  //! whether deskflow can be reached, for the extension to show
  void tellStatus(bool connected)
  {
    writeToBrowser({{QStringLiteral("type"), QStringLiteral("status")}, {QStringLiteral("connected"), connected}});
  }

  void announce()
  {
    if (m_ready && !m_browser.isEmpty()) {
      write(QStringLiteral("browserHello=%1").arg(m_browser));
    }
  }

  void readCore()
  {
    m_buffer += m_socket.readAll();
    while (true) {
      const auto end = m_buffer.indexOf('\n');
      if (end < 0) {
        return;
      }
      handleLine(QString::fromUtf8(m_buffer.left(end)));
      m_buffer.remove(0, end + 1);
    }
  }

  void handleLine(const QString &line)
  {
    const auto command = line.section(QLatin1Char('='), 0, 0);
    const auto args = line.section(QLatin1Char('='), 1);

    if (command == QStringLiteral("hello")) {
      m_ready = true;
      announce();
      tellStatus(true);
    } else if (command == QStringLiteral("browserCapture")) {
      writeToBrowser(
          {{QStringLiteral("type"), QStringLiteral("capture")},
           {QStringLiteral("id"), args.section(QLatin1Char(';'), 0, 0).toInt()},
           {QStringLiteral("mode"), args.section(QLatin1Char(';'), 1, 1)},
           {QStringLiteral("url"),
            QString::fromUtf8(QByteArray::fromBase64(args.section(QLatin1Char(';'), 2, 2).toLatin1(), kBase64))}}
      );
    } else if (command == QStringLiteral("browserClose")) {
      QJsonArray tabIds;
      const auto ids = args.split(QLatin1Char(','), Qt::SkipEmptyParts);
      for (const auto &id : ids) {
        tabIds.append(id.toInt());
      }
      writeToBrowser({{QStringLiteral("type"), QStringLiteral("close")}, {QStringLiteral("tabIds"), tabIds}});
    } else if (command == QStringLiteral("browserRestore")) {
      auto object = QJsonDocument::fromJson(QByteArray::fromBase64(args.toLatin1(), kBase64)).object();
      object.insert(QStringLiteral("type"), QStringLiteral("restore"));
      writeToBrowser(object);
    }
  }

  QString m_socketName;
  QLocalSocket m_socket;
  QTimer m_retry;
  QByteArray m_buffer;
  QString m_browser;
  bool m_ready = false;
};

// blocks, so it has a thread to itself; the browser closing stdin is the signal to leave
void readBrowser(Bridge *bridge)
{
  while (true) {
    uint32_t size = 0;
    if (std::fread(&size, sizeof(size), 1, stdin) != 1 || size > kMaxMessage) {
      break;
    }
    QByteArray json(static_cast<qsizetype>(size), Qt::Uninitialized);
    if (size > 0 && std::fread(json.data(), 1, size, stdin) != size) {
      break;
    }
    QMetaObject::invokeMethod(bridge, [bridge, json] { bridge->fromBrowser(json); }, Qt::QueuedConnection);
  }
  QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}

bool writeManifest(const QString &path, const QString &extensionId)
{
  const QJsonObject manifest = {
      {QStringLiteral("name"), kHostName},
      {QStringLiteral("description"), QStringLiteral("Lets Deskflow move browser tabs between computers")},
      {QStringLiteral("path"), QDir::toNativeSeparators(QCoreApplication::applicationFilePath())},
      {QStringLiteral("type"), QStringLiteral("stdio")},
      {QStringLiteral("allowed_origins"), QJsonArray{QStringLiteral("chrome-extension://%1/").arg(extensionId)}},
  };
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(QJsonDocument(manifest).toJson()) > 0;
}

// tells each installed browser where this program is
int install(const QString &extensionId)
{
  QTextStream out(stdout);
  const auto manifestName = QStringLiteral("%1.json").arg(kHostName);

#if defined(Q_OS_WIN)
  const auto folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Deskflow");
  QDir().mkpath(folder);
  const auto path = QDir(folder).filePath(manifestName);
  if (!writeManifest(path, extensionId)) {
    out << "can't write " << path << "\n";
    return 1;
  }
  const QStringList keys = {
      QStringLiteral("Google\\Chrome"), QStringLiteral("Microsoft\\Edge"), QStringLiteral("BraveSoftware\\Brave-Browser")
  };
  for (const auto &key : keys) {
    QSettings registry(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\%1\\NativeMessagingHosts\\%2").arg(key, kHostName),
        QSettings::NativeFormat
    );
    registry.setValue(QStringLiteral("."), QDir::toNativeSeparators(path));
    out << "registered for " << key << "\n";
  }
#else
#if defined(Q_OS_MACOS)
  const auto base = QDir::homePath() + QStringLiteral("/Library/Application Support/");
  const QStringList browsers = {
      QStringLiteral("Google/Chrome"), QStringLiteral("Microsoft Edge"), QStringLiteral("BraveSoftware/Brave-Browser")
  };
#else
  const auto base = QDir::homePath() + QStringLiteral("/.config/");
  const QStringList browsers = {
      QStringLiteral("google-chrome"), QStringLiteral("microsoft-edge"), QStringLiteral("BraveSoftware/Brave-Browser")
  };
#endif
  for (const auto &browser : browsers) {
    if (!QDir(base + browser).exists()) {
      continue;
    }
    const auto folder = base + browser + QStringLiteral("/NativeMessagingHosts");
    QDir().mkpath(folder);
    if (writeManifest(QDir(folder).filePath(manifestName), extensionId)) {
      out << "registered for " << browser << "\n";
    }
  }
#endif
  return 0;
}

} // namespace

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  const auto arguments = QCoreApplication::arguments();
  if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--install")) {
    return install(arguments.at(2));
  }

#ifdef Q_OS_WIN
  // or windows turns the length bytes that happen to be line feeds into something else
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  // --socket is for testing against something other than the real core
  const bool ownSocket = arguments.size() == 3 && arguments.at(1) == QStringLiteral("--socket");
  Bridge bridge(ownSocket ? arguments.at(2) : QString::fromLatin1(kCoreIpcName));
  std::thread reader(readBrowser, &bridge);
  reader.detach(); // it's stuck in a read until the browser goes away, and we're leaving then anyway
  return QCoreApplication::exec();
}
