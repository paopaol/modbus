#ifndef MODBUS_SERIAL_CLIENT_P_H
#define MODBUS_SERIAL_CLIENT_P_H

#include "modbus/base/modbus.h"
#include "modbus_client_types.h"
#include "modbus_frame.h"
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
    initMemberValues();
    device_ = new ReconnectableIoDevice(serialPort, this);
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

      encoder_->Encode(ele->request.get(), writerBuffer_);
      ele->totalBytes = writerBuffer_.Len();
      if (enableDump_) {
        log(LogLevel::kDebug, "{} will send: {}", device_->name(),
            dump(writerBuffer_));
      }

      char *p = nullptr;
      int len = writerBuffer_.Len();
      writerBuffer_.ZeroCopyRead(&p, len);
      device_->write(p, len);
    });
  }

  std::string dump(const ByteArray &byteArray) {
    return transferMode_ == TransferMode::kAscii ? tool::dumpRaw(byteArray)
                                                 : tool::dumpHex(byteArray);
  }

  std::string dump(const QByteArray &array) {
    return transferMode_ == TransferMode::kAscii
               ? tool::dumpRaw((uint8_t *)array.data(), array.size())
               : tool::dumpHex((uint8_t *)array.data(), array.size());
  }

  std::string dump(const pp::bytes::Buffer &buffer) {
    char *p;
    int len = buffer.Len();
    buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());
    return transferMode_ == TransferMode::kAscii
               ? tool::dumpRaw((uint8_t *)p, len)
               : tool::dumpHex((uint8_t *)p, len);
  }

  void initMemberValues() {
    sessionState_.setState(SessionState::kIdle);
    waitConversionDelay_ = 200;
    t3_5_ = 60;
    waitResponseTimeout_ = 1000;
    retryTimes_ = 0; /// default no retry
    transferMode_ = TransferMode::kRtu;

    waitResponseTimer_ = new QTimer(this);
    checkSizeFuncTable_ = creatDefaultCheckSizeFuncTableForClient();
    decoder_ = createModbusFrameDecoder(transferMode_, checkSizeFuncTable_);
    encoder_ = createModbusFrameEncoder(transferMode_);
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

  CheckSizeFuncTable checkSizeFuncTable_;
  std::unique_ptr<ModbusFrameDecoder> decoder_;
  std::unique_ptr<ModbusFrameEncoder> encoder_;

  pp::bytes::Buffer readBuffer_;
  pp::bytes::Buffer writerBuffer_;
};

class ReconnectableIoDevicePrivate : public QObject {
  Q_OBJECT
public:
  ReconnectableIoDevicePrivate(AbstractIoDevice *iodevice,
                               QObject *parent = nullptr)
      : QObject(parent), ioDevice_(iodevice) {
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
