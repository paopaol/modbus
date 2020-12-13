#ifndef __MODBUS_SERVER_H_
#define __MODBUS_SERVER_H_

#include <QBuffer>
#include <QMap>
#include <QObject>
#include <QSerialPort>
#include <QVector>
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

using canWriteSingleBitValueFunc =
    std::function<Error(Address startAddress, BitValue value)>;
using canWriteSixteenBitValueFunc =
    std::function<Error(Address startAddress, const SixteenBitValue &value)>;

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

  // read write
  void handleHoldingRegisters(Address startAddress, Quantity quantity);
  // read only
  void handleInputRegisters(Address startAddress, Quantity quantity);
  // read only
  void handleDiscreteInputs(Address startAddress, Quantity quantity);
  // read write
  void handleCoils(Address startAddress, Quantity quantity);

  // read
  bool holdingRegisterValue(Address address, SixteenBitValue *value);
  bool inputRegisterValue(Address address, SixteenBitValue *value);
  bool coilsValue(Address address, BitValue *value);
  bool inputDiscreteValue(Address address, BitValue *value);

  // write
  Error writeCoils(Address address, BitValue setValue);
  Error writeInputDiscrete(Address address, BitValue setValue);
  Error writeInputRegister(Address address, const SixteenBitValue &setValue);
  Error writeHodingRegister(Address address, const SixteenBitValue &setValue);

  bool listenAndServe();

signals:
  // if register of value changed, these signals will emited
  void holdingRegisterValueChanged(Address address,
                                   const QVector<SixteenBitValue> &values);
  void inputRegisterValueChanged(Address address,
                                 const QVector<SixteenBitValue> &values);

  // if coils/input discrete value changed,, these signal will emited
  void coilsValueChanged(Address address, const BitValue &value);
  void inputDiscreteValueChanged(Address, const BitValue &value);

protected:
  virtual Response processRequest(const Request &request);
  virtual void processBrocastRequest(const Request &request);

private:
  QScopedPointer<QModbusServerPrivate> d_ptr;
};

QModbusServer *createQModbusSerialServer(
    const QString &serialName,
    QSerialPort::BaudRate baudRate = QSerialPort::Baud9600,
    QSerialPort::DataBits dataBits = QSerialPort::Data8,
    QSerialPort::Parity parity = QSerialPort::NoParity,
    QSerialPort::StopBits stopBits = QSerialPort::OneStop,
    QObject *parent = nullptr);

QModbusServer *createQModbusTcpServer(uint16_t port = 502,
                                      QObject *parent = nullptr);

// url format:
// baud rate:1200/2400/4800/9600/115200
// data bits: 5/6/7/8
// parity:n(NoParity)/e(EvenParity)/o(OddParity)
// stop bits:1/2
//
// modbus.file:///COM1/?9600-8-n-1
// modbus.file:///dev/ttyS0/?9600-8-n-1
// modbus.tcp://:502/
QModbusServer *createServer(const QString &url, QObject *parent = nullptr);

} // namespace modbus

#endif // __MODBUS_SERVER_H_
