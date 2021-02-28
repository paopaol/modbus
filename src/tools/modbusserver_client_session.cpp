#include "modbusserver_client_session.h"
#include "modbus/base/modbus.h"
#include "modbus_frame.h"
#include "modbus_server_p.h"

namespace modbus {

ClientSession::ClientSession(QModbusServerPrivate *d,
                             AbstractConnection *connction,
                             const CheckSizeFuncTable &table)
    : d_(d), client(connction) {
  decoder = createModbusFrameDecoder(d_->transferMode_, table);
  encoder = createModbusFrameEncoder(d_->transferMode_);
}

ClientSession::~ClientSession() { client->deleteLater(); }

std::string ClientSession::fullName() const { return client->fullName(); }

void ClientSession::handleModbusRequest(pp::bytes::Buffer &buffer) {
  processModbusRequest(buffer);
  ReplyResponse();
  // fixme:use move
  response_ = Adu();
  request_ = Adu();
}

void ClientSession::processModbusRequest(pp::bytes::Buffer &buffer) {
  decoder->Decode(buffer, &request_);
  if (!decoder->IsDone()) {
    log(LogLevel::kDebug, "{} need more data", client->fullName());
    return;
  }
  const auto lastError = decoder->LasError();
  decoder->Clear();
  buffer.Reset();

  /**
   *if the requested server address is not self server address, and is
   *not brocast too, discard the recived buffer.
   */
  if (request_.serverAddress() != d_->serverAddress_ &&
      request_.serverAddress() != Adu::kBrocastAddress) {
    log(LogLevel::kError,
        "{} unexpected server address,my "
        "address[{}]",
        client->fullName(), d_->serverAddress_);
    return;
  }

  if (lastError != Error::kNoError) {
    log(LogLevel::kError, "{} invalid request", client->fullName(), lastError);
    d_->createErrorReponse(request_.functionCode(), lastError, &response_);
    return;
  }

  /**
   *if the function code is not supported,
   *discard the recive buffer,
   */
  if (!d_->handleFuncRouter_.contains(request_.functionCode())) {
    log(LogLevel::kError, "{} unsupported function code", client->fullName(),
        request_.functionCode());

    d_->createErrorReponse(request_.functionCode(), Error::kIllegalFunctionCode,
                           &response_);
    return;
  }
  if (request_.serverAddress() == Adu::kBrocastAddress) {
    d_->processBrocastRequest(&request_);
    return;
  }
  d_->processRequest(&request_, &response_);
}

void ClientSession::ReplyResponse() {
  if (!response_.isValid()) {
    return;
  }

  response_.setTransactionId(request_.transactionId());
  encoder->Encode(&response_, writeBuffer);

  uint8_t *p = nullptr;
  const int len = writeBuffer.Len();
  writeBuffer.ZeroCopyRead(&p, len);

  client->write((const char *)p, len);

  if (d_->enableDump_) {
    log(LogLevel::kDebug, "S[{}]:[{}]", client->fullName(),
        dump(d_->transferMode_, (const char *)p, len));
  }
}

} // namespace modbus
