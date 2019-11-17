#include "modbus_client_types.h"
#include <modbus/tools/modbus_serial_client.h>
#include <queue>

namespace modbus {
enum class SessionState {
  kIdle,
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

  ElementQueue elementQueue_;
  StateManager<ConnectionState> connectionState_;
  StateManager<SessionState> sessionState_;
  AbstractSerialPort *serialPort_ = nullptr;
  int waitConversionDelay_;
  int t3_5_;
};

} // namespace modbus
