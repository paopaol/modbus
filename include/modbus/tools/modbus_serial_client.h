#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include "modbus/base/modbus.h"
#include <QObject>
#include <QScopedPointer>
#include <QtSerialPort/QSerialPort>
#include <queue>

namespace modbus {

class AbstractSerialPort : public QObject {
  Q_OBJECT
public:
  AbstractSerialPort(QObject *parent = nullptr) {}
  virtual ~AbstractSerialPort() {}
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

class QModbusClientPrivate;
class QModbusClient:public QObject {
  Q_OBJECT
  Q_DECLARE_PRIVATE(QModbusClient);

public:
  static const int kBrokenLineReconnection = -1;
  QModbusClient(AbstractSerialPort *serialPort, QObject *parent = nullptr);
  QModbusClient(QObject *parent = nullptr);
  ~QModbusClient();

  void open();
  void close();
  /**
   * if the connection is not opened, the request will dropped
   */
  void sendRequest(const Request &request);
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

private:
  void runAfter(int delay, const std::function<void()> &functor);
  void setupEnvironment();
  void initMemberValues();
  void closeNotClearOpenRetrys();

  void onSerialPortOpened();
  void onSerialPortClosed();
  void onSerialPortError(const QString &errorString);
  void onSerialPortBytesWritten(qint16 bytes);
  void onSerialPortReadyRead();
  void onSerialPortResponseTimeout();
  void clearPendingRequest();

  QScopedPointer<QModbusClientPrivate> d_ptr;
};

QModbusClient *
newQtSerialClient(const QString &serialName,
                  QSerialPort::BaudRate baudRate = QSerialPort::Baud9600,
                  QSerialPort::DataBits dataBits = QSerialPort::Data8,
                  QSerialPort::Parity parity = QSerialPort::NoParity,
                  QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                  QObject *parent = nullptr);
} // namespace modbus
Q_DECLARE_METATYPE(modbus::Response);
Q_DECLARE_METATYPE(modbus::Request);

#endif // __MODBUS_SERIAL_CLIENT_H_
