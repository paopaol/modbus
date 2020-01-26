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

using BytesBufferPtr = std::shared_ptr<pp::bytes::Buffer>;
class AbstractConnection : public QObject {
  Q_OBJECT
public:
  AbstractConnection(QObject *parent = nullptr) : QObject(parent) {
    qRegisterMetaType<modbus::BytesBufferPtr>("modbus::BytesBufferPtr");
    qRegisterMetaType<quintptr>("quintptr");
  }
  virtual ~AbstractConnection() {}
  virtual quintptr fd() const = 0;
  virtual void write(const char *data, size_t size) = 0;
  virtual std::string name() const = 0;
  virtual std::string fullName() const = 0;

signals:
  void disconnected(quintptr fd);
  void messageArrived(quintptr fd, const BytesBufferPtr &message);
};

class AbstractServer : public QObject {
  Q_OBJECT
public:
  using HandleNewConnFunc = std::function<void(AbstractConnection *)>;

  AbstractServer(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~AbstractServer() {}
  virtual bool listenAndServe() = 0;

  void handleNewConnFunc(const HandleNewConnFunc &functor) {
    handleNewConnFunc_ = functor;
  }

protected:
  HandleNewConnFunc handleNewConnFunc_;
};

using canWriteSingleBitValueFunc = std::function<Error(
    FunctionCode functionCode, Address startAddress, BitValue value)>;
using canWriteSixteenBitValueFunc =
    std::function<Error(FunctionCode functionCode, Address startAddress,
                        const SixteenBitValue &value)>;

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

  /**
   *for write request, 0x05, 0x0f, 0x06,0x16,0x23
   *Before writing, check whether it can be written. If writing is allowed, the
   *mosbus server will update the value. Otherwise, it will not write, so the
   *caller can re-implement these two functions to check whether the client
   *request is valid.
   *
   */
  void setCanWriteSingleBitValueFunc(const canWriteSingleBitValueFunc &func);
  void setCanWriteSixteenBitValueFunc(const canWriteSixteenBitValueFunc &func);

  void handleFunc(FunctionCode functionCode,
                  const std::shared_ptr<SingleBitAccess> &access,
                  DataChecker *requestDataChecker = nullptr);
  void handleFunc(FunctionCode functionCode,
                  const std::shared_ptr<SixteenBitAccess> &access,
                  DataChecker *requestDataChecker = nullptr);
  bool listenAndServe();

protected:
  virtual Response processRequest(const Request &request);

  virtual void processBrocastRequest(const Request &request);

private:
  QScopedPointer<QModbusServerPrivate> d_ptr;
};

QModbusServer *createQModbusTcpServer(uint16_t port = 502,
                                      QObject *parent = nullptr);
} // namespace modbus

#endif // __MODBUS_SERVER_H_
