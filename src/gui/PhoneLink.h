#pragma once

// PhoneLink: lets a phone on the same Wi-Fi control this OpenSCAD window
// from its web browser. It runs a tiny HTTP server inside the app that serves
// a touch-friendly page plus a small JSON/HTTP API:
//
//   GET  /                     phone control page
//   GET  /api/status           file name, render state, whether an STL is ready
//   GET  /api/view.png         current 3D viewport as a PNG
//   POST /api/action?name=X    trigger a menu action (preview, render, views...)
//   POST /api/rotate?dx=&dy=   rotate the camera (degrees, relative)
//   POST /api/zoom?d=          zoom the camera (relative)
//   GET  /api/code             current editor text
//   POST /api/code             replace editor text (body = new code)
//   GET  /api/model.stl        last full render (F6) as binary STL
//
// Every /api call must carry ?k=<pin>; the pin is shown on the PC when the
// link is switched on, so other people on the network can't drive the app.

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QAction;
class MainWindow;

class PhoneLink : public QObject
{
  Q_OBJECT

public:
  static constexpr quint16 kDefaultPort = 8765;

  // Adds the "Phone Link..." item to the window's Design menu.
  static void install(MainWindow *window);
  static PhoneLink *instance();

  bool isRunning() const;
  bool start(MainWindow *window, QString *error = nullptr);
  void stop();
  QString url() const;
  QString pin() const { return pin_; }

signals:
  void runningChanged(bool running);

private:
  explicit PhoneLink(QObject *parent = nullptr);

  void onNewConnection();
  void onReadyRead(QTcpSocket *socket);
  void handleRequest(QTcpSocket *socket, const QByteArray& method, const QByteArray& target,
                     const QByteArray& body);
  void reply(QTcpSocket *socket, int status, const QByteArray& contentType, const QByteArray& body,
             const QByteArray& extraHeaders = {});
  void replyJson(QTcpSocket *socket, int status, const QByteArray& json);

  QByteArray statusJson() const;
  static QString localAddress();
  static QByteArray pageHtml();

  QTcpServer *server_ = nullptr;
  MainWindow *window_ = nullptr;
  QHash<QTcpSocket *, QByteArray> buffers_;
  QString pin_;
  quint16 port_ = kDefaultPort;
};
