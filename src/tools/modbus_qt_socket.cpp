#ifndef MODBUS_QT_SOCKET_H
#define MODBUS_QT_SOCKET_H

#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>
#include <modbus/tools/modbus_client.h>

namespace modbus {
class QtSocket : public AbstractIoDevice {
  Q_OBJECT
public:
  QtSocket(QAbstractSocket::SocketType socketType, QObject *parent = nullptr)
      : socket_(nullptr), AbstractIoDevice(parent) {
    socket_ = (socketType == QAbstractSocket::UdpSocket)
                  ? static_cast<QAbstractSocket *>(new QUdpSocket(this))
                  : static_cast<QAbstractSocket *>(new QTcpSocket(this));
    setupEnvironment();
  }
  ~QtSocket() {
    if (socket_->isOpen()) {
      socket_->close();
    }
    socket_->deleteLater();
  }

  void setHostName(const QString &hostName) { hostName_ = hostName; }

  void setPort(quint16 port) { port_ = port; }

  std::string name() override {
    return QString("%1:%2").arg(hostName_).arg(port_).toStdString();
  }

  void open() override { socket_->connectToHost(hostName_, port_); }

  void close() override {
    if (socket_->isOpen()) {
      socket_->close();
      return;
    }
    emit closed();
  }

  void write(const char *data, size_t size) override {
    socket_->write(data, size);
  }

  QByteArray readAll() override { return socket_->readAll(); }

  void clear() override {}

private:
  void setupEnvironment() {
    socket_->socketOption(QAbstractSocket::KeepAliveOption);
    connect(socket_, &QAbstractSocket::disconnected, this, &QtSocket::closed);
    connect(socket_, &QAbstractSocket::connected, this, &QtSocket::opened);
#if (QT_VERSION <= QT_VERSION_CHECK(5, 6, 1))
    connect(
        socket_,
        static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(
            &QAbstractSocket::error),
        this, [&](QAbstractSocket::SocketError err) {
#else
    connect(socket_, &QAbstractSocket::errorOccurred, this,
            [&](QAbstractSocket::SocketError err) {
#endif
          emit error(socket_->errorString());
        });
    connect(socket_, &QAbstractSocket::bytesWritten, this,
            &QtSocket::bytesWritten);
    connect(socket_, &QAbstractSocket::readyRead, this, &QtSocket::readyRead);
  }

  QString hostName_;
  quint16 port_;
  QAbstractSocket *socket_;
};

QModbusClient *newSocketClient(QAbstractSocket::SocketType type,
                               const QString &hostName, quint16 port,
                               QObject *parent) {
  QtSocket *socket = new QtSocket(type, parent);
  socket->setHostName(hostName);
  socket->setPort(port);

  QModbusClient *client = new QModbusClient(socket, parent);
  client->setTransferMode(TransferMode::kMbap);
  return client;
}
#include "modbus_qt_socket.moc"
} // namespace modbus

#endif /* MODBUS_QT_SOCKET_H */
