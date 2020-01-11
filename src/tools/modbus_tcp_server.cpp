#include <QTcpSocket>
#include <base/modbus_frame.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {

class TcpConnection : public ClientConnection {
  Q_OBJECT
public:
  TcpConnection(quintptr fd, QObject *parent = nullptr)
      : ClientConnection(parent), readBuffer_(new QBuffer()) {
    socket_.setSocketDescriptor(fd);
    connect(&socket_, &QTcpSocket::disconnected, this,
            [&]() { emit disconnected(socket_.socketDescriptor()); });
    connect(&socket_, &QTcpSocket::readyRead, this,
            &TcpConnection::onClientReadyRead);
  }
  virtual ~TcpConnection() {}

  quintptr fd() const override { return socket_.socketDescriptor(); }

signals:
  void disconnected(quintptr fd);
  void messageArrived(quintptr fd, const std::shared_ptr<QBuffer> &message);

private:
  void onClientReadyRead() {
    while (socket_.bytesAvailable() > 0) {
      char buf[1024] = {0};
      int size = socket_.read(buf, sizeof(buf));
      readBuffer_->write(buf, size);
    }
    emit messageArrived(socket_.socketDescriptor(), readBuffer_);
  }

  QTcpSocket socket_;
  std::shared_ptr<QBuffer> readBuffer_;
};

class TcpServer : public ConnectionServer {
  Q_OBJECT
public:
  TcpServer(QObject *parent = nullptr) : ConnectionServer(parent) {}

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
};

} // namespace modbus
