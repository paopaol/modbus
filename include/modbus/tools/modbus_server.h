#ifndef __MODBUS_SERVER_H_
#define __MODBUS_SERVER_H_

#include <QBuffer>
#include <QMap>
#include <QObject>
#include <assert.h>
#include <bytes/buffer.h>
#include <functional>
#include <memory>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_types.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/sixteen_bit_access.h>

namespace modbus {

class AbstractConnection : public QObject {
  Q_OBJECT
public:
  AbstractConnection(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~AbstractConnection() {}
  virtual quintptr fd() const = 0;
  virtual void write(const char *data, size_t size) = 0;
  virtual std::string name() const = 0;
  virtual std::string fullName() const = 0;

signals:
  void disconnected(quintptr fd);
  void messageArrived(quintptr fd,
                      const std::shared_ptr<pp::bytes::Buffer> &message);
};

class AbstractServer : public QObject {
  Q_OBJECT
public:
  using HandleNewConnFunc = std::function<void(AbstractConnection *)>;

  AbstractServer(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~AbstractServer() {}

  void handleNewConnFunc(const HandleNewConnFunc &functor) {
    handleNewConnFunc_ = functor;
  }

protected:
  HandleNewConnFunc handleNewConnFunc_;
};

class QModbusServerPrivate;
class QModbusServer : public QObject {
  Q_OBJECT
  Q_DECLARE_PRIVATE(QModbusServer)
public:
  QModbusServer(AbstractServer *server, QObject *parent = nullptr);
  virtual ~QModbusServer();

  int maxClients() const;
  TransferMode transferMode() const;
  QList<QString> blacklist() const;
  ServerAddress serverAddress() const;

  void setMaxClients(int maxClients);
  void setTransferMode(TransferMode transferMode);
  void addBlacklist(const QString &clientIp);
  void setServerAddress(ServerAddress serverAddress);

  void handleFunc(FunctionCode functionCode, const SingleBitAccess &access,
                  DataChecker *requestDataChecker = nullptr);
signals:

  /**
   *private signal
   */
  void responseCreated(const Response &response);

protected:
  virtual Response processRequest(const Request &request);
  virtual void processBrocastRequest(const Request &request);

private:
  QScopedPointer<QModbusServerPrivate> d_ptr;
};

} // namespace modbus

#endif // __MODBUS_SERVER_H_