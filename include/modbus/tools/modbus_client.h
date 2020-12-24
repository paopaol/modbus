#ifndef __MODBUS_CLIENT_H_
#define __MODBUS_CLIENT_H_

#include <modbus/base/sixteen_bit_access.h>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>
#include <QVector>
#include <QtNetwork/QAbstractSocket>
#include <QtSerialPort/QSerialPort>
#include <memory>
#include <queue>
#include "modbus/base/modbus.h"

namespace modbus {

class AbstractIoDevice : public QObject {
  Q_OBJECT
 public:
  AbstractIoDevice(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~AbstractIoDevice() {}
  virtual void open() = 0;
  virtual void close() = 0;
  virtual void write(const char *data, size_t size) = 0;
  virtual QByteArray readAll() = 0;
  virtual void clear() = 0;
  virtual std::string name() = 0;
 signals:
  void opened();
  void closed();
  void error(const QString &errorString);
  void bytesWritten(qint64 bytes);
  void readyRead();
};

class ReconnectableIoDevicePrivate;
class ReconnectableIoDevice : public QObject {
  Q_OBJECT
  Q_DECLARE_PRIVATE(ReconnectableIoDevice);

 public:
  static const int kBrokenLineReconnection = -1;
  ReconnectableIoDevice(QObject *parent = nullptr);
  ReconnectableIoDevice(AbstractIoDevice *iodevice, QObject *parent = nullptr);
  ~ReconnectableIoDevice();
  void setOpenRetryTimes(int retryTimes, int delay);
  int openRetryTimes();
  int openRetryDelay();

  void open();
  void close();
  void write(const char *data, size_t size);
  QByteArray readAll();
  void clear();
  std::string name();

  bool isOpened();
  bool isClosed();
 signals:
  void opened();
  void closed();
  void error(const QString &errorString);
  void bytesWritten(qint64 bytes);
  void readyRead();
  void connectionIsLostWillReconnect();

 private:
  void setupEnvironment();
  void onIoDeviceOpened();
  void onIoDeviceClosed();

  void onIoDeviceError(const QString &errorString);
  void closeButNotSetForceCloseFlag();

  QScopedPointer<ReconnectableIoDevicePrivate> d_ptr;
};

class QModbusClientPrivate;
class QModbusClient : public QObject {
  Q_OBJECT
  Q_DECLARE_PRIVATE(QModbusClient);

 public:
  QModbusClient(AbstractIoDevice *iodevice, QObject *parent = nullptr);
  QModbusClient(QObject *parent = nullptr);
  ~QModbusClient();

  void open();
  void close();
  /**
   * if the connection is not opened, the request will dropped
   */
  void sendRequest(std::unique_ptr<Request> &request);

  /**
   *for function code 0x01/0x02
   *will emit readSingleBitsFinished signal
   */
  void readSingleBits(ServerAddress serverAddress, FunctionCode functionCode,
                      Address startAddress, Quantity quantity);

  /**
   *for function code 0x05
   *will emit writeSingleCoilFinished signal
   */
  void writeSingleCoil(ServerAddress serverAddress, Address startAddress,
                       BitValue value);

  /*
   *for function code 0x0f
   *will emit writeMultipleCoilsFinished signal
   */
  void writeMultipleCoils(ServerAddress serverAddress, Address startAddress,
                          const QVector<BitValue> &valueList);

  /**
   * sixteem bit access, for function code 3/4
   * will emit readRegistersFinished signal
   */
  void readRegisters(ServerAddress serverAddress, FunctionCode functionCode,
                     Address startAddress, Quantity quantity);
  /**
   * for function code 0x06
   * will emit writeSingleRegisterFinished signal
   */
  void writeSingleRegister(ServerAddress serverAddress, Address address,
                           const SixteenBitValue &value);
  /**
   *for function code 0x10
   *wiil emit writeMultipleRegistersFinished signal
   */
  void writeMultipleRegisters(ServerAddress serverAddress, Address startAddress,
                              const QVector<SixteenBitValue> &valueList);

  /**
   *for function code 0x17
   *will emit readWriteMultipleRegistersFinished signal
   */
  void readWriteMultipleRegisters(ServerAddress serverAddress,
                                  Address readStartAddress,
                                  Quantity readQuantity,
                                  Address writeStartAddress,
                                  const QVector<SixteenBitValue> &valueList);

  bool isIdle();

  bool isClosed();
  bool isOpened();

  void setTimeout(uint64_t timeout);
  uint64_t timeout();

  void setTransferMode(TransferMode transferMode);
  TransferMode transferMode() const;

  void setRetryTimes(int times);
  int retryTimes();
  void setOpenRetryTimes(int retryTimes, int delay = 1000);
  int openRetryTimes();
  int openRetryDelay();

  void setFrameInterval(int frameInterval);
  /**
   * After the disconnection, all pending requests will be deleted. So. if the
   * short-term reconnection, there should be no pending requests
   */
  size_t pendingRequestSize();

  QString errorString();

  /**
   *enable collection diagnosis
   *default is disabled
   */
  void enableDiagnosis(bool flag);
  void enableDump(bool enable);

  RuntimeDiagnosis runtimeDiagnosis() const;

 signals:
  void clientOpened();
  void clientClosed();
  void errorOccur(const QString &errorString);
  void connectionIsLostWillReconnect();
  void requestFinished(const Request &request, const Response &response);
  void readSingleBitsFinished(ServerAddress serverAddress,
                              FunctionCode functionCode, Address startAddress,
                              Quantity quantity,
                              const QVector<BitValue> &valueList, Error error);
  void writeSingleCoilFinished(ServerAddress serverAddress, Address address,
                               Error error);
  void readRegistersFinished(ServerAddress serverAddress,
                             FunctionCode functionCode, Address startAddress,
                             Quantity quantity, const ByteArray &data,
                             Error error);
  void writeSingleRegisterFinished(ServerAddress serverAddress, Address address,
                                   Error error);

  void writeMultipleRegistersFinished(ServerAddress serverAddress,
                                      Address address, Error error);

  void writeMultipleCoilsFinished(ServerAddress serverAddress, Address address,
                                  Error error);
  void readWriteMultipleRegistersFinished(
      ServerAddress serverAddress, Address readStartAddress,
      const QVector<SixteenBitValue> &valueList, Error error);

 private:
  void runAfter(int delay, const std::function<void()> &functor);
  void setupEnvironment();
  void initMemberValues();
  void closeNotClearOpenRetrys();

  void onIoDeviceError(const QString &errorString);
  void onIoDeviceBytesWritten(qint16 bytes);
  void onIoDeviceReadyRead();
  void onIoDeviceResponseTimeout();
  void clearPendingRequest();
  void processResponseAnyFunctionCode(const Request &request,
                                      const Response &response);
  void processFunctionCode(const Request &request, const Response &response);
  void processDiagnosis(const Request &request, const Response &response);

  QScopedPointer<QModbusClientPrivate> d_ptr;
};

Request createRequest(ServerAddress serverAddress, FunctionCode functionCode,
                      const DataChecker &dataChecker, const any &userData,
                      const ByteArray &data);

QModbusClient *newQtSerialClient(
    const QString &serialName,
    QSerialPort::BaudRate baudRate = QSerialPort::Baud9600,
    QSerialPort::DataBits dataBits = QSerialPort::Data8,
    QSerialPort::Parity parity = QSerialPort::NoParity,
    QSerialPort::StopBits stopBits = QSerialPort::OneStop,
    QObject *parent = nullptr);

QModbusClient *newSocketClient(QAbstractSocket::SocketType type,
                               const QString &hostName, quint16 port,
                               QObject *parent = nullptr);
// url format:
// baud rate:1200/2400/4800/9600/115200
// data bits: 5/6/7/8
// parity:n(NoParity)/e(EvenParity)/o(OddParity)
// stop bits:1/2
//
// modbus.file:///COM1/?9600-8-n-1
// modbus.file:///dev/ttyS0/?9600-8-n-1
// modbus.tcp://192.168.4.66:502/
// modbus.udp://192.168.4.66:502/
QModbusClient *createClient(const QString &url, QObject *parent = nullptr);

}  // namespace modbus
Q_DECLARE_METATYPE(modbus::Response);
Q_DECLARE_METATYPE(modbus::Request);
Q_DECLARE_METATYPE(modbus::SixteenBitAccess);
Q_DECLARE_METATYPE(modbus::Error);
Q_DECLARE_METATYPE(QVector<modbus::SixteenBitValue>);
Q_DECLARE_METATYPE(QVector<modbus::BitValue>);
Q_DECLARE_METATYPE(modbus::ByteArray);

#endif  // __MODBUS_CLIENT_H_
