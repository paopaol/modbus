#ifndef MODBUS_QT_SOCKET_H
#define MODBUS_QT_SOCKET_H

#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>
#include <modbus/tools/modbus_client.h>

namespace modbus {
class QtTcpSocket : public AbstractIoDevice {
  Q_OBJECT
public:
  QtTcpSocket(QObject *parent = nullptr)
      : socket_(new QTcpSocket(this)), AbstractIoDevice(parent) {
    setupEnvironment();
  }
  ~QtTcpSocket() {
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
    connect(socket_, &QAbstractSocket::disconnected, this,
            &QtTcpSocket::closed);
    connect(socket_, &QAbstractSocket::connected, this, &QtTcpSocket::opened);
#if (QT_VERSION <= QT_VERSION_CHECK(5, 6, 1) || QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
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
            &QtTcpSocket::bytesWritten);
    connect(socket_, &QAbstractSocket::readyRead, this,
            &QtTcpSocket::readyRead);
  }

  QString hostName_;
  quint16 port_;
  QTcpSocket *socket_;
};

class QtUdpSocket : public AbstractIoDevice {
  Q_OBJECT
public:
  QtUdpSocket(QObject *parent = nullptr)
      : socket_(new QUdpSocket(this)), AbstractIoDevice(parent) {
    setupEnvironment();
  }
  ~QtUdpSocket() { socket_->deleteLater(); }

  void setHostName(const QString &hostName) { hostName_ = hostName; }

  void setPort(quint16 port) { port_ = port; }

  std::string name() override {
    return QString("%1:%2").arg(hostName_).arg(port_).toStdString();
  }

  void open() override {
    emit opened();
    return;
  }

  void close() override {
    emit closed();
    return;
  }

  void write(const char *data, size_t size) override {
    socket_->writeDatagram(data, size, QHostAddress(hostName_), port_);
  }

  QByteArray readAll() override {
    QByteArray datagram;

    do {
      datagram.resize(socket_->pendingDatagramSize());
      socket_->readDatagram(datagram.data(), datagram.size());
    } while (socket_->hasPendingDatagrams());
    return datagram;
  }

  void clear() override {}

private:
  void setupEnvironment() {
    socket_->bind(port_);

#if (QT_VERSION <= QT_VERSION_CHECK(5, 6, 1) || QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
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
            &QtUdpSocket::bytesWritten);
    connect(socket_, &QAbstractSocket::readyRead, this,
            &QtUdpSocket::readyRead);
  }

  QString hostName_;
  quint16 port_;
  QUdpSocket *socket_;
};

QModbusClient *newSocketClient(QAbstractSocket::SocketType type,
                               const QString &hostName, quint16 port,
                               QObject *parent) {

  AbstractIoDevice *ioDevice = nullptr;
  if (type == QAbstractSocket::TcpSocket) {
    QtTcpSocket *socket = new QtTcpSocket(parent);
    socket->setHostName(hostName);
    socket->setPort(port);
    ioDevice = socket;
  } else {
    QtUdpSocket *socket = new QtUdpSocket(parent);
    socket->setHostName(hostName);
    socket->setPort(port);
    ioDevice = socket;
  }

  QModbusClient *client = new QModbusClient(ioDevice, parent);
  client->setTransferMode(TransferMode::kMbap);
  return client;
}
#include "modbus_qt_socket.moc"
} // namespace modbus

#endif /* MODBUS_QT_SOCKET_H */
