#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include "modbus/base/modbus.h"
#include <QSerialPort>
#include <QTimer>
#include <assert.h>
#include <queue>

namespace modbus {

template <typename StateType> class StateManager {
public:
  StateManager() {}
  StateManager(StateType state) : state_(state) {}
  void setState(StateType state) { state_ = state; }
  StateType state() { return state_; }

private:
  StateType state_;
};

enum class ConnectionState { kOpening, kOpened, kClosing, kClosed, kError };

class Client : public QObject {
  Q_OBJECT
protected:
  struct Element {
    Request request;
    Response response;
  };
  using ElementQueue = std::queue<Element>;
  Element createElement(const Request &request) {
    Element element;

    element.request = request;
    return element;
  }

public:
  Client(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~Client() {}

  virtual void open() = 0;
  virtual void close() = 0;
  virtual void sendRequest(const Request &request) = 0;
  virtual void enqueueElement(const Element &element) = 0;

  virtual bool isClosed() = 0;
  virtual bool isOpened() = 0;
signals:
  void clientOpened();
  void clientClosed();
  void requestFinished(const Request &request, const Response &response);

protected:
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
};

class QSerialClient : public Client {
  Q_OBJECT
public:
  QSerialClient(AbstractSerialPort *serialPort, QObject *parent = nullptr)
      : serialPort_(serialPort), Client(parent) {
    initMemberValues();
    setupEnvironment();
  }

  QSerialClient(QObject *parent = nullptr) : Client(parent) {
    initMemberValues();
    setupEnvironment();
  }
  ~QSerialClient() {
    if (isOpened()) {
      close();
    }
    // assert(isClosed() && "the QSerialClient is running, can't destrctor");
    if (serialPort_) {
      serialPort_->deleteLater();
    }
  }

  void open() override {
    if (!isClosed()) {
      // FIXME:add log
      return;
    }
    connectionState_.setState(ConnectionState::kOpening);
    serialPort_->open();
    return;
  }
  void close() override {
    if (!isOpened()) {
      return;
    }
    connectionState_.setState(ConnectionState::kClosing);
    serialPort_->close();
  }
  void sendRequest(const Request &request) override {}

  bool isClosed() override {
    return connectionState_.state() == ConnectionState::kClosed;
  }
  bool isOpened() override {
    return connectionState_.state() == ConnectionState::kOpened;
  }
signals:
  void clientOpened();
  void clientClosed();
  void errorOccur(const QString &errorString);
  void requestFinished(const Request &request, const Response &response);

protected:
  void enqueueElement(const Client::Element &element) override {
    elementQueue_.push(element);

    if (sessionState_.state() != SessionState::kIdle) {
      return;
    }
    runAfter(t3_5_, [&]() {
      auto ele = elementQueue_.front();
      auto request = ele.request;
      auto array = request.data();
      serialPort_->write(
          QByteArray(reinterpret_cast<const char *>(array.data())),
          array.size());
    });
  }

private:
  enum class SessionState {
    kIdle,
    kWaitingResponse,
    kProcessingResponse,
    kWaitingConversionDelay,
    kProcessingError
  };

  void runAfter(int delay, const std::function<void()> &functor) {
    QTimer::singleShot(delay, functor);
  }

  void setupEnvironment() {
    assert(serialPort_ && "the serialport backend is invalid");
    connect(serialPort_, &AbstractSerialPort::opened, [&]() {
      connectionState_.setState(ConnectionState::kOpened);
      emit clientOpened();
    });
    connect(serialPort_, &AbstractSerialPort::closed, [&]() {
      connectionState_.setState(ConnectionState::kClosed);
      emit clientClosed();
    });
    connect(serialPort_, &AbstractSerialPort::error,
            [&](const QString &errorString) {
              connectionState_.setState(ConnectionState::kClosed);
              emit errorOccur(errorString);
              emit clientClosed();
            });
  }
  void initMemberValues() {
    connectionState_.setState(ConnectionState::kClosed);
    sessionState_.setState(SessionState::kIdle);
    waitConversionDelay_ = 200;
    t3_5_ = 100;
  }

  ElementQueue elementQueue_;
  StateManager<ConnectionState> connectionState_;
  StateManager<SessionState> sessionState_;
  AbstractSerialPort *serialPort_ = nullptr;
  int waitConversionDelay_;
  int t3_5_;
};
} // namespace modbus
#endif // __MODBUS_SERIAL_CLIENT_H_
