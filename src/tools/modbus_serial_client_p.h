#include "modbus_client_types.h"
#include <QTimer>
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

class QSerialClientPrivate {
public:
  QSerialClientPrivate(AbstractSerialPort *serialPort,
                       QObject *parent = nullptr)
      : serialPort_(serialPort) {}

  void enqueueElement(const Element &element) {
    elementQueue_.push(element);

    if (sessionState_.state() != SessionState::kIdle) {
      return;
    }
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
  uint32_t retryTimes_;
  QTimer waitResponseTimer_;
};

} // namespace modbus
