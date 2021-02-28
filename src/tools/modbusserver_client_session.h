#ifndef MODBUSSERVER_CLIENT_SESSION_H
#define MODBUSSERVER_CLIENT_SESSION_H

#include "base/modbus_frame.h"
#include "modbus/base/modbus.h"
#include "modbus/base/modbus_types.h"
#include <memory>

namespace modbus {

class AbstractConnection;
class QModbusServerPrivate;
class ClientSession {
public:
  ClientSession(QModbusServerPrivate *d, AbstractConnection *connction,
                const CheckSizeFuncTable &table);
  ~ClientSession();

  std::string fullName() const;

  void handleModbusRequest(pp::bytes::Buffer &buffer);

private:
  void processModbusRequest(pp::bytes::Buffer &buffer);
  void ReplyResponse();

  QModbusServerPrivate *d_ = nullptr;
  AbstractConnection *client = nullptr;
  std::unique_ptr<ModbusFrameDecoder> decoder;
  std::unique_ptr<ModbusFrameEncoder> encoder;
  Adu request_;
  Adu response_;
  pp::bytes::Buffer writeBuffer;
};

using ClientSessionPtr = std::shared_ptr<ClientSession>;

} // namespace modbus

#endif
