#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include "modbus.h"
#include "modbus_qt.h"
#include <QSerialPort>
#include <QTimer>
#include <queue>

namespace modbus {
class ConnectionState {
public:
  enum class State { kOpening, kOpened, kClosing, kClosed, kError };
  void setConnectionState(State state) { state_ = state; }
  State connectionState() { return state_; }
  void setErrorCode(const QString &errorString) { errorString_ = errorString; }
  QString error() { return errorString_; }

private:
  State state_;
  QString errorString_;
};
enum class SessionState {
  kIdle,
  kWaitingResponse,
  kProcessingResponse,
  kWaitingConversionDelay,
  kProcessingError
};

class Client : public QObject {
  Q_OBJECT
protected:
  struct Element {
    Request request;
    Response response;
  };
  using ElementQueue = std::queue<Element>;

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

class QSerialClient : public Client {
  Q_OBJECT
public:
  QSerialClient(QObject *parent = nullptr)
      : Client(parent), sessionState_(SessionState::kIdle) {
    connectionState_.setConnectionState(ConnectionState::State::kClosed);
  }
  void open() override {
    if (!isClosed()) {
      // FIXME:add log
      return;
    }
    connectionState_.setConnectionState(ConnectionState::State::kOpening);
    bool opened = serialPort_.open(QIODevice::ReadWrite);
    if (opened) {
      connectionState_.setConnectionState(ConnectionState::State::kOpened);
      emit streamOpened();
      return;
    } else {
      emit errorOccur(serialPort_.errorString());
      return;
    }
  }
  void close() override {
    auto currentState = connectionState_.connectionState();
    if (currentState != ConnectionState::State::kOpened) {
      return;
    }
    connectionState_.setConnectionState(ConnectionState::State::kClosing);
    serialPort_.close();
    connectionState_.setConnectionState(ConnectionState::State::kClosed);
    emit streamClosed();
  }
  void sendRequest(const Request &request) override {}

  bool isClosed() override {
    return connectionState_.connectionState() ==
           ConnectionState::State::kClosed;
  }
  bool isOpened() override {
    return connectionState_.connectionState() ==
           ConnectionState::State::kOpened;
  }
signals:
  void streamOpened();
  void streamClosed();
  void errorOccur(const QString &errorString);
  void requestFinished(const Request &request, const Response &response);

protected:
  void enqueueElement(const Client::Element &element) override {
    elementQueue_.push(element);

    if (sessionState_ != SessionState::kIdle) {
      return;
    }
    runAfter(t3_5_, [&]() {
      auto ele = elementQueue_.front();
      auto request = ele.request;
      auto array = request.data();
      serialPort_.write(
          QByteArray(reinterpret_cast<const char *>(array.data())),
          array.size());
    });
  }

private:
  Element createElement(const Request &request) {
    Element element;

    element.request = request;
    return element;
  }

  void runAfter(int delay, const std::function<void()> &functor) {
    QTimer::singleShot(delay, functor);
  }

  ElementQueue elementQueue_;
  ConnectionState connectionState_;
  SessionState sessionState_;
  QSerialPort serialPort_;
  int waitResponseDeley_;
  int waitConversionDelay_;
  int t3_5_;
};
} // namespace modbus
#endif // __MODBUS_SERIAL_CLIENT_H_
