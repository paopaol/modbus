#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <bytes/buffer.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {

class TcpConnection : public AbstractConnection {
  Q_OBJECT
public:
  TcpConnection(quintptr fd, QObject *parent = nullptr)
      : AbstractConnection(parent), fd_(fd),
        readBuffer_(new pp::bytes::Buffer()) {
    socket_.setSocketDescriptor(fd);
    connect(&socket_, &QTcpSocket::disconnected, this,
            [&]() { emit disconnected(fd_); });
    connect(&socket_, &QTcpSocket::readyRead, this,
            &TcpConnection::onClientReadyRead);
  }
  virtual ~TcpConnection() {}

  quintptr fd() const override { return socket_.socketDescriptor(); }

  void write(const char *data, size_t size) override {
    socket_.write(data, size);
  }
  std::string name() const override {
    return socket_.localAddress().toString().toStdString();
  }
  std::string fullName() const override {
    QString address;
    auto localAddress = socket_.localAddress();
    bool ok = true;
    QHostAddress ipv4(localAddress.toIPv4Address(&ok));
    if (ok) {
      address = ipv4.toString();
    } else {
      QHostAddress ipv6(localAddress.toIPv6Address());
      address = ipv6.toString();
    }
    return QString("%1:%2").arg(address).arg(socket_.localPort()).toStdString();
  }

private:
  void onClientReadyRead() {
    while (socket_.bytesAvailable() > 0) {
      char buf[1024] = {0};
      int size = socket_.read(buf, sizeof(buf));
      readBuffer_->Write(buf, size);
    }
    emit messageArrived(socket_.socketDescriptor(), readBuffer_);
  }

  QTcpSocket socket_;
  quintptr fd_ = -1;
  BytesBufferPtr readBuffer_;
};

class PrivateTcpServer : public QTcpServer {
  Q_OBJECT
public:
  PrivateTcpServer(QObject *parent = nullptr) : QTcpServer(parent) {}
  ~PrivateTcpServer() {}
signals:
  void newConnectionArrived(qintptr socketDescriptor);

protected:
  void incomingConnection(qintptr socketDescriptor) {
    emit newConnectionArrived(socketDescriptor);
  }
};

class TcpServer : public AbstractServer {
  Q_OBJECT
public:
  TcpServer(QObject *parent = nullptr) : AbstractServer(parent) {
    connect(&tcpServer_, &PrivateTcpServer::newConnectionArrived, this,
            &TcpServer::incomingConnection);
  }
  void setListenPort(uint16_t port) { port_ = port; }
  bool listenAndServe() override {
    bool success = tcpServer_.listen(QHostAddress::Any, port_);
    if (!success) {
      log(LogLevel::kError, "tcp server listen(:{}) failed. {}", port_,
          tcpServer_.errorString().toStdString());
      return false;
    }
    log(LogLevel::kInfo, "tcp server listened at 0.0.0.0:{}", port_);
    return true;
  }

protected:
  void incomingConnection(qintptr socketDescriptor) {
    if (!handleNewConnFunc_) {
      QTcpSocket unused;
      unused.setSocketDescriptor(socketDescriptor);
      unused.close();
      return;
    }

    // log
    auto conn = new TcpConnection(socketDescriptor, this);
    handleNewConnFunc_(conn);
  }

private:
  PrivateTcpServer tcpServer_;
  uint16_t port_ = 502;
};

QModbusServer *createQModbusTcpServer(uint16_t port, QObject *parent) {
  auto tcpServer = new TcpServer(parent);
  tcpServer->setListenPort(port);
  auto modbusServer = new QModbusServer(tcpServer, parent);
  modbusServer->setTransferMode(TransferMode::kMbap);
  return modbusServer;
}

} // namespace modbus

#include "modbus_tcp_server.moc"
