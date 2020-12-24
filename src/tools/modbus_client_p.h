#ifndef MODBUS_SERIAL_CLIENT_P_H
#define MODBUS_SERIAL_CLIENT_P_H

#include "modbus_client_types.h"
#include <QTimer>
#include <base/modbus_logger.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>
#include <modbus/tools/modbus_client.h>
#include <queue>

namespace modbus {
enum class SessionState { kIdle, kSendingRequest, kWaitingResponse };

inline std::ostream &operator<<(std::ostream &output,
                                const SessionState &state) {
  switch (state) {
  case SessionState::kIdle:
    output << "idle";
    break;
  case SessionState::kSendingRequest:
    output << "sending-request";
    break;
  case SessionState::kWaitingResponse:
    output << "waiting-response";
    break;
  default:
    output.setstate(std::ios_base::failbit);
  }

  return output;
}

class QModbusClientPrivate : public QObject {
  Q_OBJECT
public:
  QModbusClientPrivate(AbstractIoDevice *serialPort, QObject *parent = nullptr)
      : QObject(parent) {
    device_ = new ReconnectableIoDevice(serialPort, this);
    waitResponseTimer_ = new QTimer(this);
  }
  ~QModbusClientPrivate() {}

  Element *enqueueAndPeekLastElement() {
    elementQueue_.push_back(new Element());
    return elementQueue_.back();
  }

  void scheduleNextRequest(int delay) {
    /**
     * only in idle state can send request
     */
    if (sessionState_.state() != SessionState::kIdle) {
      return;
    }

    if (elementQueue_.empty()) {
      return;
    }

    /*after some delay, the request will be sent,so we change the state to
     * sending request*/
    sessionState_.setState(SessionState::kSendingRequest);
    QTimer::singleShot(delay, this, [&]() {
      if (elementQueue_.empty()) {
        return;
      }
      smart_assert(sessionState_.state() ==
                   SessionState::kSendingRequest)(sessionState_.state());
      /**
       * take out the first request,send it out,
       */
      auto &ele = elementQueue_.front();
      auto data = ele->requestFrame->marshal();
      if (enableDump_) {
        log(LogLevel::kDebug, "{} will send: {}", device_->name(), dump(data));
      }
      device_->write((const char *)data.data(), data.size());
    });
  }

  std::string dump(const ByteArray &byteArray) {
    return transferMode_ == TransferMode::kAscii ? tool::dumpRaw(byteArray)
                                                 : tool::dumpHex(byteArray);
  }

  /**
   * In rtu mode, only one request can be sent at the same time and then
   * processed. If multiple requests are sent consecutively, subsequent requests
   * are not ignored and are placed in the queue. Each time a request is taken
   * from the queue is processed, and when a request is completely processed,
   * the next element in the queue is processed. For the current code
   * implementation, the first element in the queue is the request that is
   * currently being processed. So, after the request is processed, it will be
   * removed.
   */
  ElementQueue elementQueue_;
  StateManager<SessionState> sessionState_;
  ReconnectableIoDevice *device_ = nullptr;
  int waitConversionDelay_;
  int t3_5_;
  int waitResponseTimeout_;
  int retryTimes_;
  QTimer *waitResponseTimer_ = nullptr;
  QString errorString_;

  /// the default transfer mode must be rtu mode
  TransferMode transferMode_;

  /// defualt is disabled
  bool enableDiagnosis_ = false;
  RuntimeDiagnosis runtimeDiagnosis_;
  bool enableDump_ = true;
};

class ReconnectableIoDevicePrivate : public QObject {
  Q_OBJECT
public:
  ReconnectableIoDevicePrivate(AbstractIoDevice *iodevice,
                               QObject *parent = nullptr)
      : ioDevice_(iodevice), QObject(parent) {
    ioDevice_->setParent(this);
  }
  ~ReconnectableIoDevicePrivate() {}

  int openRetryTimes_ = 0;
  int openRetryTimesBack_ = 0;
  int reopenDelay_ = 1000;
  AbstractIoDevice *ioDevice_;
  /**
   * if user call ReconnectableIoDevice::close(), this is force close
   * if the connection broken,the device is closed, this is not force close
   */
  bool forceClose_ = false;
  StateManager<ConnectionState> connectionState_;
  QString errorString_;
};

} // namespace modbus

#endif /* MODBUS_SERIAL_CLIENT_P_H */
