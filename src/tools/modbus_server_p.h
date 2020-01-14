#ifndef __MODBUS_SERVER_P_H_
#define __MODBUS_SERVER_P_H_

#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <fmt/core.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {

static DataChecker defaultRequestDataChecker(FunctionCode functionCode);
static void appendByteArray(ByteArray &array, const std::vector<char> &carray);

static std::string dump(TransferMode transferMode, const ByteArray &byteArray) {
  return transferMode == TransferMode::kAscii ? tool::dumpRaw(byteArray)
                                              : tool::dumpHex(byteArray);
}

static std::string dump(TransferMode transferMode,
                        const pp::bytes::Buffer &buffer) {
  ByteArray array;
  std::vector<char> carray;
  appendByteArray(array, carray);
  return transferMode == TransferMode::kAscii ? tool::dumpRaw(array)
                                              : tool::dumpHex(array);
}

#define sessionIteratorOrReturn(it, fd)                                        \
  auto it = sessionList_.find(fd);                                             \
  if (it == sessionList_.end()) {                                              \
    return;                                                                    \
  }

struct ClientSession {
  AbstractConnection *client = nullptr;
};

struct HandleFuncEntry {
  FunctionCode functionCode;
  DataChecker requestDataChecker;
  SingleBitAccess singleBitAccess;
  SixteenBitAccess sixteenBitAccess;
};

class QModbusServerPrivate : public QObject {
  Q_OBJECT
public:
  enum class ProcessResult {
    kSuccess,
    kNeedMoreData,
    kBadServerAddress,
    kBadFunctionCode,
    kBroadcast,
    kStorageParityError
  };

  QModbusServerPrivate(QObject *parent = nullptr) : QObject(parent) {}

  int maxClients() const { return maxClient_; }
  TransferMode transferMode() const { return transferMode_; }
  QList<QString> blacklist() const {
    QList<QString> ipList;
    for (const auto &ip : blacklist_) {
      ipList.push_back(ip);
    }
    return ipList;
  }

  ServerAddress serverAddress() const { return serverAddress_; }

  void setMaxClients(int maxClients) {
    maxClients = std::max(1, maxClients);
    maxClient_ = maxClients;
  }
  void setTransferMode(TransferMode transferMode) {
    transferMode_ = transferMode;
  }
  void addBlacklist(const QString &clientIp) {
    blacklist_[clientIp] = clientIp;
  }
  void setServerAddress(ServerAddress serverAddress) {
    serverAddress_ = serverAddress;
  }

  void handleFunc(FunctionCode functionCode, const SingleBitAccess &access,
                  DataChecker *requestDataChecker = nullptr) {
    HandleFuncEntry entry;

    entry.functionCode = functionCode;
    entry.singleBitAccess = access;
    entry.requestDataChecker = requestDataChecker
                                   ? *requestDataChecker
                                   : defaultRequestDataChecker(functionCode);
    handleFuncRouter_[functionCode] = entry;

    log(LogLevel::kInfo, "route add Function[{}]", functionCode);
    if (!entry.requestDataChecker.calculateSize) {
      log(LogLevel::kInfo, "Function[{}] invalud request data size checker",
          functionCode);
    }
  }

  void setServer(AbstractServer *server) { server_ = server; }

  void setEnv() {
    assert(server_ && "not set ConnectionServer");

    server_->handleNewConnFunc(
        std::bind(&QModbusServerPrivate::incomingConnection, this,
                  std::placeholders::_1));
  }

  void incomingConnection(AbstractConnection *connection) {
    ClientSession session;
    session.client = connection;

    connect(session.client, &AbstractConnection::disconnected, this,
            &QModbusServerPrivate::removeClient);
    connect(session.client, &AbstractConnection::messageArrived, this,
            &QModbusServerPrivate::onMessageArrived);
    sessionList_[connection->fd()] = session;
  }

  void removeClient(qintptr fd) {
    sessionIteratorOrReturn(it, fd);
    it.value().client->deleteLater();
    sessionList_.erase(it);
  }

  void onMessageArrived(quintptr fd,
                        const std::shared_ptr<pp::bytes::Buffer> &buffer) {
    sessionIteratorOrReturn(sessionIt, fd);
    auto &session = sessionIt.value();

    std::shared_ptr<Frame> requestFrame;
    std::shared_ptr<Frame> responseFrame;
    auto result = processModbusRequest(buffer, requestFrame, responseFrame);
    switch (result) {
    case ProcessResult::kNeedMoreData: {
      log(LogLevel::kDebug, "{} need more data R[{}]",
          session.client->fullName(), dump(transferMode_, *buffer));
      break;
    }
    case ProcessResult::kBadServerAddress: {
      log(LogLevel::kWarning,
          "{} unexpected server address,my "
          "address[{}] R[{}]",
          session.client->fullName(), serverAddress_,
          dump(transferMode_, *buffer));
      break;
    }
    case ProcessResult::kBadFunctionCode: {
      log(LogLevel::kWarning, "{} unsupported function code R[{}]",
          session.client->fullName(), dump(transferMode_, *buffer));
      break;
    }
    case ProcessResult::kBroadcast: {
    }
    case ProcessResult::kStorageParityError: {
      log(LogLevel::kWarning, "{} invalid request R[{}]",
          session.client->fullName(), dump(transferMode_, *buffer));
      break;
    }
    case ProcessResult::kSuccess: {
    }
    }
    if (requestFrame && responseFrame) {
      writeFrame(session, responseFrame, requestFrame->frameId());
    }
  }

  ProcessResult
  processModbusRequest(const std::shared_ptr<pp::bytes::Buffer> &buffer,
                       std::shared_ptr<Frame> &requestFrame,
                       std::shared_ptr<Frame> &responseFrame) {
    requestFrame = createModebusFrame(transferMode_);

    ByteArray data;
    std::vector<char> d;

    buffer->PeekAt(d, 0, buffer->Len());
    appendByteArray(data, d);

    /**
     *first, try decode server address, function code
     */
    ServerAddress serverAddress;
    FunctionCode functionCode;
    auto result = requestFrame->unmarshalServerAddressFunctionCode(
        data, &serverAddress, &functionCode);
    if (result != DataChecker::Result::kSizeOk) {
      return ProcessResult::kNeedMoreData;
    }

    /**
     *if the requested server address is not self server address, and is
     *not brocast too, discard the recived buffer.
     */
    if (serverAddress != serverAddress_ &&
        serverAddress != Adu::kBrocastAddress) {
      buffer->Reset();
      return ProcessResult::kBadServerAddress;
    }

    /**
     *if the function code is not supported,
     *discard the recive buffer,
     */
    if (!handleFuncRouter_.contains(functionCode)) {
      buffer->Reset();
      return ProcessResult::kBadFunctionCode;
    }

    /**
     *now, handle the supported function code
     *call user defined handlers
     */
    auto &entry = handleFuncRouter_[functionCode];
    auto adu = requestFrame->adu();
    adu.setDataChecker(entry.requestDataChecker);
    requestFrame->setAdu(adu);

    Error error = Error::kNoError;
    result = requestFrame->unmarshal(data, &error);
    if (result == DataChecker::Result::kNeedMoreData) {
      return ProcessResult::kNeedMoreData;
    }

    if (result == DataChecker::Result::kFailed) {
      buffer->Reset();
      responseFrame = createModebusFrame(transferMode_);
      responseFrame->setAdu(
          createErrorReponse(functionCode, Error::kStorageParityError));
      return ProcessResult::kStorageParityError;
    }

    char *unused;
    buffer->ZeroCopyRead(unused, requestFrame->marshalSize());

    Request request(requestFrame->adu());
    if (serverAddress == Adu::kBrocastAddress) {
      processBrocastRequest(request);
      return ProcessResult::kBroadcast;
    }

    responseFrame = createModebusFrame(transferMode_);
    responseFrame->setAdu(processRequest(request));
    return ProcessResult::kSuccess;
  }

  Response processRequest(const Request &request) {
    switch (request.functionCode()) {
    case FunctionCode::kReadCoils:
      return processReadCoilsRequest(request);
      break;
    default:
      break;
    }
  }

  void writeFrame(ClientSession &session, const std::shared_ptr<Frame> &frame,
                  uint16_t id) {
    auto array = frame->marshal(&id);
    session.client->write((const char *)array.data(), array.size());

    log(LogLevel::kDebug, "{} will send:{}", session.client->fullName(),
        dump(transferMode_, array));
  }

  void processBrocastRequest(const Request &request) {}

  Response createErrorReponse(FunctionCode functionCode, Error errorCode) {
    Response response;

    response.setServerAddress(serverAddress_);
    response.setFunctionCode(FunctionCode(functionCode | Pdu::kExceptionByte));
    response.setData(ByteArray({uint8_t(errorCode)}));
    response.setDataChecker(expectionResponseDataChecker);

    return response;
  }

  Response processReadCoilsRequest(const Request &request) {
    using modbus::FunctionCode;
    Response response;

    response.setFunctionCode(kReadCoils);
    response.setServerAddress(serverAddress_);

    SingleBitAccess access;
    bool ok = access.unmarshalReadRequest(request.data());
    if (!ok) {
      response.setError(Error::kStorageParityError);
      response.setData(ByteArray({uint8_t(response.error())}));
      return response;
    }

    auto requestStartAddress = access.startAddress();
    auto requestQuantity = access.quantity();
    auto &entry = handleFuncRouter_[kReadCoils];
    auto myStartAddress = entry.singleBitAccess.startAddress();
    auto myQuantity = entry.singleBitAccess.quantity();

    if (requestStartAddress < myStartAddress ||
        requestStartAddress > myStartAddress + myQuantity) {
      response.setError(Error::kIllegalDataAddress);
      response.setData(ByteArray({uint8_t(response.error())}));
      log(LogLevel::kDebug,
          "requested function "
          "code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          kReadCoils, myStartAddress, myQuantity, requestStartAddress,
          requestQuantity);
      return response;
    }

    if (requestStartAddress + requestQuantity > myStartAddress + myQuantity) {
      response.setError(Error::kIllegalDataAddress);
      response.setData(ByteArray({uint8_t(response.error())}));
      log(LogLevel::kDebug,
          "requested function "
          "code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          kReadCoils, myStartAddress, myQuantity, requestStartAddress,
          requestQuantity);
      return response;
    }

    SingleBitAccess responseAccess;

    responseAccess.setStartAddress(requestStartAddress);
    responseAccess.setQuantity(requestQuantity);
    for (size_t i = 0; i < responseAccess.quantity(); i++) {
      Address address = responseAccess.startAddress() + i;
      responseAccess.setValue(address, entry.singleBitAccess.value(address));
    }
    response.setError(Error::kNoError);
    response.setData(responseAccess.marshalReadResponse());
    return response;
  }

  int maxClient_ = 1;
  QMap<QString, QString> blacklist_;
  TransferMode transferMode_ = TransferMode::kMbap;
  QMap<FunctionCode, HandleFuncEntry> handleFuncRouter_;
  QMap<qintptr, ClientSession> sessionList_;
  AbstractServer *server_ = nullptr;
  ServerAddress serverAddress_ = 1;
};

static DataChecker defaultRequestDataChecker(FunctionCode functionCode) {
  using modbus::FunctionCode;
  static QMap<FunctionCode, DataChecker> requestDataCheckerMap = {
      {kReadCoils, {bytesRequired<4>}},
      {kReadInputDiscrete, {bytesRequired<4>}},
      {kReadHoldingRegisters, {bytesRequired<4>}},
      {kReadInputRegister, {bytesRequired<4>}},
      {kWriteSingleCoil, {bytesRequired<4>}},
      {kWriteSingleRegister, {bytesRequired<4>}},
      {kWriteMultipleCoils, {bytesRequiredStoreInArrayIndex<4>}},
      {kWriteMultipleRegisters, {bytesRequiredStoreInArrayIndex<4>}},
      {kReadWriteMultipleRegisters, {bytesRequiredStoreInArrayIndex<9>}}};

  return requestDataCheckerMap.contains(functionCode)
             ? requestDataCheckerMap[functionCode]
             : DataChecker();
}

static void appendByteArray(ByteArray &array, const std::vector<char> &carray) {
  array.insert(array.end(), carray.begin(), carray.end());
}

} // namespace modbus

#endif // __MODBUS_SERVER_P_H_
