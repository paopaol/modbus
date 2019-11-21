#ifndef MODBUS_SERIAL_CLIENT_P_H
#define MODBUS_SERIAL_CLIENT_P_H

#include "modbus_client_types.h"
#include <QTimer>
#include <modbus/base/modbus_tool.h>
#include <modbus/tools/modbus_serial_client.h>
#include <queue>

namespace modbus {
enum class SessionState {
  kIdle,
  kSendingRequest,
  kWaitingResponse,
  kProcessingResponse,
  kWaitingConversionDelay,
  kProcessingError
};

class QSerialClientPrivate : public QObject {
  Q_OBJECT
public:
  QSerialClientPrivate(AbstractSerialPort *serialPort,
                       QObject *parent = nullptr)
      : serialPort_(serialPort), QObject(parent) {}

  void enqueueElement(const Element &element) {
    elementQueue_.push(element);
    scheduleNextRequest();
  }

  void scheduleNextRequest() {
    /**
     * only in idle state can send request
     */
    if (sessionState_.state() != SessionState::kIdle) {
      return;
    }

    /*after some delay, the request will be sent,so we change the state to
     * sending request*/
    sessionState_.setState(SessionState::kSendingRequest);
    QTimer::singleShot(t3_5_, this, [&]() {
      /**
       * take out the first request,send it out,
       */
      auto &ele = elementQueue_.front();
      auto &request = ele.request;
      auto data = request.marshalData();
      /**
       * we append crc, then write to serialport
       */
      auto modbusSerialData = tool::appendCrc(data);
      serialPort_->write(
          QByteArray(reinterpret_cast<const char *>(modbusSerialData.data())),
          modbusSerialData.size());
    });
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
  StateManager<ConnectionState> connectionState_;
  StateManager<SessionState> sessionState_;
  AbstractSerialPort *serialPort_ = nullptr;
  int waitConversionDelay_;
  int t3_5_;
  int waitResponseTimeout_;
  int retryTimes_;
  QTimer waitResponseTimer_;
};

static inline ByteArray subArray(const ByteArray &array, size_t index,
                                 int n = -1) {
  if (n == -1) {
    return ByteArray(array.begin() + index, array.end());
  } else {
    return ByteArray(array.begin() + index, array.begin() + index + n);
  }
}

} // namespace modbus

#endif /* MODBUS_SERIAL_CLIENT_P_H */
