#ifndef __MODBUS_CLIENT_H_
#define __MODBUS_CLIENT_H_

#include "modbus/base/modbus.h"
#include <QObject>
#include <QScopedPointer>
#include <QTimer>
#include <QtNetwork/QAbstractSocket>
#include <QtSerialPort/QSerialPort>
#include <modbus/base/sixteen_bit_access.h>
#include <queue>

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
  void sendRequest(const Request &request);

  /**
   * sixteem bit access, for function code 3/4
   * will emit readRegistersFinished signal
   */
  void readRegisters(ServerAddress serverAddress, FunctionCode functionCode,
                     const SixteenBitAccess &access);
  /**
   * for function code 0x06
   * will emit writeSingleRegisterFinished signal
   */
  void writeSingleRegister(ServerAddress serverAddress, Address address,
                           const SixteenBitValue &value);

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
signals:
  void clientOpened();
  void clientClosed();
  void errorOccur(const QString &errorString);
  void requestFinished(const Request &request, const Response &response);
  void readRegistersFinished(const Request &request, const Response &response,
                             const SixteenBitAccess &access);
  void writeSingleRegisterFinished(ServerAddress serverAddress, Address address,
                                   bool isSuccess);

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

  QScopedPointer<QModbusClientPrivate> d_ptr;
};

QModbusClient *
newQtSerialClient(const QString &serialName,
                  QSerialPort::BaudRate baudRate = QSerialPort::Baud9600,
                  QSerialPort::DataBits dataBits = QSerialPort::Data8,
                  QSerialPort::Parity parity = QSerialPort::NoParity,
                  QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                  QObject *parent = nullptr);

QModbusClient *newSocketClient(QAbstractSocket::SocketType type,
                               const QString &hostName, quint16 port,
                               QObject *parent = nullptr);

} // namespace modbus
Q_DECLARE_METATYPE(modbus::Response);
Q_DECLARE_METATYPE(modbus::Request);
Q_DECLARE_METATYPE(modbus::SixteenBitAccess);

#endif // __MODBUS_CLIENT_H_
