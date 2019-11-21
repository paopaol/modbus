#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include "modbus/base/modbus.h"
#include <QObject>
#include <QScopedPointer>
#include <queue>

namespace modbus {

class Client : public QObject {
  Q_OBJECT
public:
  Client(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~Client() {}

  virtual void open() = 0;
  virtual void close() = 0;
  virtual void sendRequest(const Request &request) = 0;

  virtual bool isClosed() = 0;
  virtual bool isOpened() = 0;
signals:
  void clientOpened();
  void clientClosed();
  void errorOccur(const QString &errorString);
  void requestFinished(const Request &request, const Response &response);
};

class AbstractSerialPort : public QObject {
  Q_OBJECT
public:
  AbstractSerialPort(QObject *parent = nullptr) {}
  ~AbstractSerialPort() {}
  virtual void open() = 0;
  virtual void close() = 0;
  virtual void write(const char *data, size_t size) = 0;
signals:
  void opened();
  void closed();
  void error(const QString &errorString);
  void bytesWritten(qint64 bytes);
};

class QSerialClientPrivate;
class QSerialClient : public Client {
  Q_OBJECT
  Q_DECLARE_PRIVATE(QSerialClient);

public:
  QSerialClient(AbstractSerialPort *serialPort, QObject *parent = nullptr);
  QSerialClient(QObject *parent = nullptr);
  ~QSerialClient();

  void open() override;
  void close() override;
  void sendRequest(const Request &request) override;

  bool isClosed() override;
  bool isOpened() override;

  void setTimeout(uint64_t timeout);
  uint64_t timeout();

  void setRetryTimes(int times);
  int retryTimes();

private:
  void runAfter(int delay, const std::function<void()> &functor);
  void setupEnvironment();
  void initMemberValues();

  QScopedPointer<QSerialClientPrivate> d_ptr;
};
} // namespace modbus
#endif // __MODBUS_SERIAL_CLIENT_H_
