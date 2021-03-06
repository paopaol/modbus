#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <bytes/buffer.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
static QList<QString> localIpList();

class TcpConnection : public AbstractConnection {
  Q_OBJECT
public:
  explicit TcpConnection(quintptr fd, QObject *parent = nullptr)
      : AbstractConnection(parent), fd_(fd),
        readBuffer_(new pp::bytes::Buffer()) {
    socket_.setSocketDescriptor(fd);
    connect(&socket_, &QTcpSocket::disconnected, this,
            [&]() { emit disconnected(fd_); });
    connect(&socket_, &QTcpSocket::readyRead, this,
            &TcpConnection::onClientReadyRead);
  }
  ~TcpConnection() override = default;

  quintptr fd() const override { return socket_.socketDescriptor(); }

  void write(const char *data, size_t size) override {
    socket_.write(data, size);
  }
  std::string name() const override {
    return socket_.peerAddress().toString().toStdString();
  }
  std::string fullName() const override {
    QString address;
    auto peerAddress = socket_.peerAddress();
    bool ok = true;
    QHostAddress ipv4(peerAddress.toIPv4Address(&ok));
    if (ok) {
      address = ipv4.toString();
    } else {
      QHostAddress ipv6(peerAddress.toIPv6Address());
      address = ipv6.toString();
    }
    return QString("%1:%2").arg(address).arg(socket_.peerPort()).toStdString();
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
  explicit PrivateTcpServer(QObject *parent = nullptr) : QTcpServer(parent) {}
  ~PrivateTcpServer() override = default;
signals:
  void newConnectionArrived(qintptr _t1);

protected:
  void incomingConnection(qintptr socketDescriptor) override {
    emit newConnectionArrived(socketDescriptor);
  }
};

class TcpServer : public AbstractServer {
  Q_OBJECT
public:
  explicit TcpServer(QObject *parent = nullptr) : AbstractServer(parent) {
    connect(&tcpServer_, &PrivateTcpServer::newConnectionArrived, this,
            &TcpServer::incomingConnection);
  }
  void setListenPort(uint16_t port) { port_ = port; }
  bool listenAndServe() override {
    bool success = tcpServer_.listen(QHostAddress::Any, port_);
    if (!success) {
      log(prefix(), LogLevel::kError, "tcp server listen(:{}) failed. {}",
          port_, tcpServer_.errorString().toStdString());
      return false;
    }

    auto ipList = localIpList();
    log(prefix(), LogLevel::kInfo, "tcp server listened at [{}]:{}",
        ipList.join(",").toStdString(), port_);
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

static QList<QString> localIpList() {
  QList<QString> ipList;
  auto interfaceList = QNetworkInterface::allInterfaces();
  for (auto &networkInterface : interfaceList) {
    auto addressEntrys = networkInterface.addressEntries();
    for (auto &addessEntry : addressEntrys) {
      auto ip = addessEntry.ip();
      if (ip.protocol() !=
          QAbstractSocket::NetworkLayerProtocol::IPv4Protocol) {
        continue;
      }
      ipList.push_back(ip.toString());
    }
  }
  return ipList;
}

} // namespace modbus

#include "modbus_tcp_server.moc"
