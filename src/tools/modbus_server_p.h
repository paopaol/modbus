#ifndef __MODBUS_SERVER_P_H_
#define __MODBUS_SERVER_P_H_

#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <fmt/core.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {

static DataChecker defaultRequestDataChecker(FunctionCode functionCode);
static void appendByteArray(ByteArray &array, const std::vector<char> &carray);
static ByteArray byteArrayFromBuffer(pp::bytes::Buffer &buffer);

static std::string dump(TransferMode transferMode, const ByteArray &byteArray) {
  return transferMode == TransferMode::kAscii ? tool::dumpRaw(byteArray)
                                              : tool::dumpHex(byteArray);
}

static std::string dump(TransferMode transferMode,
                        const pp::bytes::Buffer &buffer) {
  ByteArray array =
      byteArrayFromBuffer(const_cast<pp::bytes::Buffer &>(buffer));
  return transferMode == TransferMode::kAscii ? tool::dumpRaw(array)
                                              : tool::dumpHex(array);
}

#define sessionIteratorOrReturn(it, fd)                                        \
  auto it = sessionList_.find(fd);                                             \
  if (it == sessionList_.end()) {                                              \
    return;                                                                    \
  }

#define DeferRun(functor)                                                      \
  std::shared_ptr<void> _##__LINE__(nullptr, std::bind(functor))

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
  bool listenAndServe() { return server_->listenAndServe(); }

  void setEnv() {
    assert(server_ && "not set ConnectionServer");

    server_->handleNewConnFunc(
        std::bind(&QModbusServerPrivate::incomingConnection, this,
                  std::placeholders::_1));
  }

  void incomingConnection(AbstractConnection *connection) {
    connect(connection, &AbstractConnection::disconnected, this,
            &QModbusServerPrivate::removeClient);
    bool ok = connect(connection, &AbstractConnection::messageArrived, this,
                      &QModbusServerPrivate::onMessageArrived);

    ClientSession session;
    session.client = connection;
    sessionList_[connection->fd()] = session;
  }

  void removeClient(qintptr fd) {
    sessionIteratorOrReturn(it, fd);
    log(LogLevel::kInfo, "{} closed", it.value().client->fullName());
    it.value().client->deleteLater();
    sessionList_.erase(it);
  }

  void checkProcessRequestResult(const ClientSession &session,
                                 ProcessResult result,
                                 const std::shared_ptr<Frame> &requestFrame,
                                 const pp::bytes::Buffer &buffer) {
    switch (result) {
    case ProcessResult::kNeedMoreData: {
      log(LogLevel::kDebug, "{} need more data R[{}]",
          session.client->fullName(), dump(transferMode_, buffer));
      break;
    }
    case ProcessResult::kBadServerAddress: {
      log(LogLevel::kError,
          "{} unexpected server address,my "
          "address[{}]",
          session.client->fullName(), serverAddress_);
      break;
    }
    case ProcessResult::kBadFunctionCode: {
      log(LogLevel::kError, "{} unsupported function code",
          session.client->fullName());
      break;
    }
    case ProcessResult::kBroadcast: {
    }
    case ProcessResult::kStorageParityError: {
      log(LogLevel::kError, "{} invalid request", session.client->fullName());
      break;
    }
    case ProcessResult::kSuccess: {
    }
    }
  }

  void onMessageArrived(quintptr fd, const BytesBufferPtr &buffer) {
    sessionIteratorOrReturn(sessionIt, fd);
    auto &session = sessionIt.value();
    log(LogLevel::kDebug, "R[{}]:[{}]", session.client->fullName(),
        dump(transferMode_, *buffer));

    std::shared_ptr<Frame> requestFrame;
    std::shared_ptr<Frame> responseFrame;
    auto result = processModbusRequest(buffer, requestFrame, responseFrame);
    checkProcessRequestResult(session, result, requestFrame, *buffer);
    // if requestFrame and responseFrame is not null
    // that is need reply somthing to client
    if (requestFrame && responseFrame) {
      writeFrame(session, responseFrame, requestFrame->frameId());
    }
  }

  ProcessResult processModbusRequest(const BytesBufferPtr &buffer,
                                     std::shared_ptr<Frame> &requestFrame,
                                     std::shared_ptr<Frame> &responseFrame) {
    requestFrame = createModebusFrame(transferMode_);
    auto data = byteArrayFromBuffer(*buffer);

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

    auto adu = requestFrame->adu();
    adu.setServerAddress(serverAddress);
    adu.setFunctionCode(functionCode);
    requestFrame->setAdu(adu);
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
      responseFrame = createModebusFrame(transferMode_);
      responseFrame->setAdu(
          createErrorReponse(functionCode, Error::kIllegalFunctionCode));
      return ProcessResult::kBadFunctionCode;
    }

    /**
     *now, handle the supported function code
     *call user defined handlers
     */
    auto &entry = handleFuncRouter_[functionCode];
    adu = requestFrame->adu();
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
    using modbus::FunctionCode;
    switch (request.functionCode()) {
    case kReadCoils: {
      return processReadSingleBitRequest(request, kReadCoils);
    } break;
    case kReadInputDiscrete: {
      return processReadSingleBitRequest(request, kReadInputDiscrete);
    } break;
    default:
      break;
    }
  }

  void writeFrame(ClientSession &session, const std::shared_ptr<Frame> &frame,
                  uint16_t id) {
    auto array = frame->marshal(&id);
    session.client->write((const char *)array.data(), array.size());

    log(LogLevel::kDebug, "S[{}]:[{}]", session.client->fullName(),
        dump(transferMode_, array));
  }

  void processBrocastRequest(const Request &request) {}

  Response createErrorReponse(FunctionCode functionCode, Error errorCode) {
    Response response;

    response.setError(errorCode);
    response.setServerAddress(serverAddress_);
    response.setFunctionCode(FunctionCode(functionCode | Pdu::kExceptionByte));
    response.setData(ByteArray({uint8_t(errorCode)}));
    response.setDataChecker(expectionResponseDataChecker);

    return response;
  }


    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);

  Response processReadSingleBitRequest(const Request &request,
                                       FunctionCode functionCode) {
    SingleBitAccess access;
    bool ok = access.unmarshalReadRequest(request.data());
    if (!ok) {
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }

    auto requestStartAddress = access.startAddress();
    auto requestQuantity = access.quantity();
    auto &entry = handleFuncRouter_[functionCode];
    auto myStartAddress = entry.singleBitAccess.startAddress();
    auto myQuantity = entry.singleBitAccess.quantity();

    if (requestStartAddress < myStartAddress ||
        requestStartAddress > myStartAddress + myQuantity) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, myStartAddress, myQuantity, requestStartAddress,
          requestQuantity);
      return createErrorReponse(functionCode, Error::kIllegalDataAddress);
    }

    if (requestStartAddress + requestQuantity > myStartAddress + myQuantity) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, myStartAddress, myQuantity, requestStartAddress,
          requestQuantity);
      return createErrorReponse(functionCode, Error::kIllegalDataAddress);
    }

    SingleBitAccess responseAccess;

    responseAccess.setStartAddress(requestStartAddress);
    responseAccess.setQuantity(requestQuantity);
    for (size_t i = 0; i < responseAccess.quantity(); i++) {
      Address address = responseAccess.startAddress() + i;
      responseAccess.setValue(address, entry.singleBitAccess.value(address));
    }

    Response response;
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
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

static ByteArray byteArrayFromBuffer(pp::bytes::Buffer &buffer) {
  ByteArray data;
  std::vector<char> d;

  buffer.PeekAt(d, 0, buffer.Len());
  appendByteArray(data, d);
  return data;
}

} // namespace modbus

#endif // __MODBUS_SERVER_P_H_
