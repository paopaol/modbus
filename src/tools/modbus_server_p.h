#ifndef __MODBUS_SERVER_P_H_
#define __MODBUS_SERVER_P_H_

#include <algorithm>
#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <modbus/base/smart_assert.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
enum class StorageKind {
  kCoils,
  kInputDiscrete,
  kHoldingRegisters,
  kInputRegisters
};

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
  SingleBitAccess *singleBitAccess;
  SixteenBitAccess *sixteenBitAccess;
};

class QModbusServerPrivate : public QObject {
  Q_OBJECT
  Q_DECLARE_PUBLIC(QModbusServer)
public:
  enum class ProcessResult {
    kSuccess,
    kNeedMoreData,
    kBadServerAddress,
    kBadFunctionCode,
    kBroadcast,
    kStorageParityError
  };

  QModbusServerPrivate(QModbusServer *q) : q_ptr(q) {}

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

  // read write
  void handleCoils(Address startAddress, Quantity quantity) {
    coils_.setStartAddress(startAddress);
    coils_.setQuantity(quantity);
    handleFunc(kReadCoils, &coils_);
    handleFunc(kWriteSingleCoil, &coils_);
    handleFunc(kWriteMultipleCoils, &coils_);
  }

  // read only
  void handleDiscreteInputs(Address startAddress, Quantity quantity) {
    inputDiscrete_.setStartAddress(startAddress);
    inputDiscrete_.setQuantity(quantity);
    handleFunc(kReadInputDiscrete, &coils_);
  }

  // read only
  void handleInputRegisters(Address startAddress, Quantity quantity) {
    inputRegister_.setStartAddress(startAddress);
    inputRegister_.setQuantity(quantity);
    handleFunc(kReadInputRegister, &inputRegister_);
  }

  // read write
  void handleHoldingRegisters(Address startAddress, Quantity quantity) {
    holdingRegister_.setStartAddress(startAddress);
    holdingRegister_.setQuantity(quantity);
    handleFunc(kReadHoldingRegisters, &holdingRegister_);
    handleFunc(kWriteSingleRegister, &holdingRegister_);
    handleFunc(kWriteMultipleRegisters, &holdingRegister_);
    handleFunc(kReadWriteMultipleRegisters, &holdingRegister_);
  }

  void handleFunc(FunctionCode functionCode, SingleBitAccess *access,
                  DataChecker *requestDataChecker = nullptr) {
    HandleFuncEntry entry;

    entry.functionCode = functionCode;
    smart_assert(access && "invalid access")(functionCode);
    entry.singleBitAccess = access;
    entry.requestDataChecker = requestDataChecker
                                   ? *requestDataChecker
                                   : defaultRequestDataChecker(functionCode);
    handleFuncRouter_[functionCode] = entry;

    log(LogLevel::kInfo, "route add Function[{}] StartAddress[{}] Quantity[{}]",
        functionCode, access->startAddress(), access->quantity());
    if (!entry.requestDataChecker.calculateSize) {
      log(LogLevel::kInfo, "Function[{}] invalud request data size checker",
          functionCode);
    }
  }

  void handleFunc(FunctionCode functionCode, SixteenBitAccess *access,
                  DataChecker *requestDataChecker = nullptr) {
    HandleFuncEntry entry;

    entry.functionCode = functionCode;
    smart_assert(access && "invalid access")(functionCode);
    entry.sixteenBitAccess = access;
    entry.requestDataChecker = requestDataChecker
                                   ? *requestDataChecker
                                   : defaultRequestDataChecker(functionCode);
    handleFuncRouter_[functionCode] = entry;

    log(LogLevel::kInfo, "route add Function[{}] StartAddress[{}] Quantity[{}]",
        functionCode, access->startAddress(), access->quantity());
    if (!entry.requestDataChecker.calculateSize) {
      log(LogLevel::kInfo, "Function[{}] invalud request data size checker",
          functionCode);
    }
  }

  bool updateValue(FunctionCode functionCode, Address address,
                   const SixteenBitValue &newValue) {
    smart_assert(functionCode == FunctionCode::kWriteSingleRegister ||
                 functionCode == FunctionCode::kWriteMultipleRegisters ||
                 functionCode == FunctionCode::kReadInputRegister ||
                 functionCode == FunctionCode::kReadHoldingRegisters &&
                     "invalud function code")(functionCode);

    auto entry = handleFuncRouter_.find(functionCode);
    if (entry == handleFuncRouter_.end()) {
      log(LogLevel::kWarning,
          fmt::format("function code[{}] not supported", functionCode));
      return false;
    }

    bool ok = true;
    entry->sixteenBitAccess->value(address, &ok);
    if (!ok) {
      log(LogLevel::kWarning,
          "address out of range.function "
          "code:{} address:{} [start {} quantity {}]",
          functionCode, address, entry->sixteenBitAccess->startAddress(),
          entry->sixteenBitAccess->quantity());
      return false;
    }
    entry->sixteenBitAccess->setValue(address, newValue.toUint16());
    return true;
  }

  bool registerValue(const SixteenBitAccess &access, Address address,
                     SixteenBitValue *value) {
    if (!value) {
      return false;
    }
    bool ok = true;
    auto v = access.value(address, &ok);
    if (!ok) {
      log(LogLevel::kWarning,
          "address out of range.function "
          "address:{} [start {} quantity {}]",
          address, access.startAddress(), access.quantity());
      return false;
    }
    *value = v;
    return true;
  }

  bool coilsValue(const SingleBitAccess &access, Address address,
                  BitValue *value) {
    if (!value) {
      return false;
    }
    auto v = access.value(address);
    *value = v;
    return true;
  }

  bool holdingRegisterValue(Address address, SixteenBitValue *value) {
    return registerValue(holdingRegister_, address, value);
  }

  bool inputRegisterValue(Address address, SixteenBitValue *value) {
    return registerValue(inputRegister_, address, value);
  }

  bool coilsValue(Address address, BitValue *value) {
    return coilsValue(coils_, address, value);
  }

  bool inputDiscreteValue(Address address, BitValue *value) {
    return coilsValue(inputDiscrete_, address, value);
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
    requestFrame = createModbusFrame(transferMode_);
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
      responseFrame = createModbusFrame(transferMode_);
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
      responseFrame = createModbusFrame(transferMode_);
      responseFrame->setAdu(
          createErrorReponse(functionCode, Error::kStorageParityError));
      return ProcessResult::kStorageParityError;
    }

    /*
     *discard the already parsed data
     */
    char *unused;
    buffer->ZeroCopyRead(unused, requestFrame->marshalSize());

    Request request(requestFrame->adu());
    if (serverAddress == Adu::kBrocastAddress) {
      processBrocastRequest(request);
      return ProcessResult::kBroadcast;
    }

    responseFrame = createModbusFrame(transferMode_);
    responseFrame->setAdu(processRequest(request));
    return ProcessResult::kSuccess;
  }

  Response processRequest(const Request &request) {
    using modbus::FunctionCode;
    switch (request.functionCode()) {
    case kReadCoils:
    case kReadInputDiscrete: {
      return processReadSingleBitRequest(request, request.functionCode());
    }
    case kWriteSingleCoil: {
      return processWriteCoilRequest(request);
    }
    case kWriteMultipleCoils: {
      return processWriteCoilsRequest(request);
    }
    case kReadHoldingRegisters:
    case kReadInputRegister: {
      return processReadMultipleRegisters(request, request.functionCode());
    }
    case kWriteSingleRegister: {
      return processWriteHoldingRegisterRequest(request);
    }
    case kWriteMultipleRegisters: {
      return processWriteHoldingRegistersRequest(request);
    }
    default:
      smart_assert(0 && "unsuported function")(request.functionCode());
      break;
    }
    return Response();
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

  Response processWriteCoilsRequest(const Request &request) {
    FunctionCode functionCode = FunctionCode::kWriteMultipleCoils;

    SingleBitAccess access;
    bool ok = access.unmarshalMultipleWriteRequest(request.data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }

    auto error = writeCoils(functionCode, request, coils_, access);
    if (error != Error::kNoError) {
      return createErrorReponse(functionCode, error);
    }

    Response response;
    response.setError(Error::kNoError);
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setData(access.marshalAddressQuantity());
    return response;
  }

  Error writeCoilsInternal(StorageKind kind, SingleBitAccess *my,
                           const SingleBitAccess *you) {
    Q_Q(QModbusServer);
    Address startAddress = you->startAddress();
    auto value = you->value(startAddress);
    if (value == BitValue::kBadValue) {
      return Error::kIllegalDataValue;
    }

    Address reqStartAddress = you->startAddress();
    for (size_t i = 0; i < you->quantity(); i++) {
      Address address = reqStartAddress + i;
      auto value = you->value(address);
      auto error = canWriteSingleBitValue(address, value);
      if (error != Error::kNoError) {
        return error;
      }
    }

    for (size_t i = 0; i < you->quantity(); i++) {
      Address address = reqStartAddress + i;
      auto value = you->value(address);
      auto oldValue = my->value(address);
      if (value != oldValue) {
        my->setValue(address, value);
        if (kind == StorageKind::kCoils) {
          emit q->coilsValueChanged(address, value);
        } else if (kind == StorageKind::kInputDiscrete) {
          emit q->inputDiscreteValueChanged(address, value);
        }
      }
    }
    return Error::kNoError;
  }

  Error writeCoils(FunctionCode functionCode, const Request &request,
                   SingleBitAccess &my, const SingleBitAccess &you) {
    auto error = validateSingleBitAccess(you, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, my.startAddress(), my.quantity(), you.startAddress(),
          my.quantity());
      return error;
    }
    error = writeCoilsInternal(StorageKind::kCoils, &coils_, &you);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid request ({}): bad data {}", functionCode,
          dump(transferMode_, request.data()));
      return error;
    }
    return Error::kNoError;
  }

  // coils
  Response processWriteCoilRequest(const Request &request) {
    FunctionCode functionCode = FunctionCode::kWriteSingleCoil;
    SingleBitAccess access;

    bool ok = access.unmarshalSingleWriteRequest(request.data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }
    auto error = writeCoils(functionCode, request, coils_, access);
    if (error != Error::kNoError) {
      return createErrorReponse(functionCode, error);
    }

    Response response;
    response.setError(Error::kNoError);
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setData(access.marshalSingleWriteRequest());
    return response;
  }

  void setCanWriteSingleBitValueFunc(const canWriteSingleBitValueFunc &func) {
    canWriteSingleBitValue_ = func;
  }

  void setCanWriteSixteenBitValueFunc(const canWriteSixteenBitValueFunc &func) {
    canWriteSixteenBitValue_ = func;
  }

  Error canWriteSingleBitValue(Address startAddress, BitValue value) {
    if (canWriteSingleBitValue_) {
      return canWriteSingleBitValue_(startAddress, value);
    }
    return Error::kNoError;
  }

  // TODO(jinzhao):how to use it?it's useable?
  Error canWriteSixteenBitValue(Address startAddress,
                                const SixteenBitValue &value) {
    if (canWriteSixteenBitValue_) {
      return canWriteSixteenBitValue_(startAddress, value);
    }
    return Error::kNoError;
  }

  Response processReadSingleBitRequest(const Request &request,
                                       FunctionCode functionCode) {
    SingleBitAccess access;
    bool ok = access.unmarshalReadRequest(request.data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }

    auto &entry = handleFuncRouter_[functionCode];
    const auto &my = *entry.singleBitAccess;
    auto error = validateSingleBitAccess(access, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, my.startAddress(), my.quantity(), access.startAddress(),
          my.quantity());
      return createErrorReponse(functionCode, error);
    }

    auto requestStartAddress = access.startAddress();
    auto requestQuantity = access.quantity();
    SingleBitAccess responseAccess;

    responseAccess.setStartAddress(requestStartAddress);
    responseAccess.setQuantity(requestQuantity);
    for (size_t i = 0; i < responseAccess.quantity(); i++) {
      Address address = responseAccess.startAddress() + i;
      responseAccess.setValue(address, entry.singleBitAccess->value(address));
    }

    Response response;
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setError(Error::kNoError);
    response.setData(responseAccess.marshalReadResponse());
    return response;
  }

  Response processReadMultipleRegisters(const Request &request,
                                        FunctionCode functionCode) {
    SixteenBitAccess access;

    bool ok = access.unmarshalAddressQuantity(request.data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }
    auto entry = handleFuncRouter_[functionCode];
    const auto &my = *entry.sixteenBitAccess;
    auto error = validateSixteenAccess(access, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request ({}) :myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, my.startAddress(), my.quantity(), access.startAddress(),
          access.quantity());
      return createErrorReponse(functionCode, error);
    }

    Address reqStartAddress = access.startAddress();
    Quantity reqQuantity = access.quantity();
    SixteenBitAccess responseAccess;
    responseAccess.setStartAddress(reqStartAddress);
    responseAccess.setQuantity(reqQuantity);
    for (size_t i = 0; i < reqQuantity; i++) {
      Address address = reqStartAddress + i;
      auto value = entry.sixteenBitAccess->value(address);
      responseAccess.setValue(address, value.toUint16());
    }

    Response response;
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setError(Error::kNoError);
    response.setData(responseAccess.marshalMultipleReadResponse());
    return response;
  }

  Error writeRegisterValuesInternal(StorageKind kind, SixteenBitAccess *set,
                                    const SixteenBitAccess &access) {
    Q_Q(QModbusServer);

    auto error = validateSixteenAccess(access, *set);
    if (error != Error::kNoError) {
      return error;
    }

    Quantity quantity = access.quantity();

    for (size_t i = 0; i < quantity; i++) {
      Address reqStartAddress = access.startAddress() + i;
      auto value = access.value(reqStartAddress);
      error = canWriteSixteenBitValue(reqStartAddress, value);
      if (error != Error::kNoError) {
        return error;
      }
    }

	QVector<SixteenBitValue> new_values;
	QVector<SixteenBitValue> old_values;
    for (size_t i = 0; i < quantity; i++) {
      Address reqStartAddress = access.startAddress() + i;
      auto value = access.value(reqStartAddress);
      auto oldValue = set->value(reqStartAddress);
	  new_values.push_back(value);
	  old_values.push_back(oldValue);
      set->setValue(reqStartAddress, value.toUint16());
    }

	if (new_values != old_values) {
		if (kind == StorageKind::kHoldingRegisters) {
			emit q->holdingRegisterValueChanged(access.startAddress(), new_values);
		}
		else if (kind == StorageKind::kInputRegisters) {
			emit q->inputRegisterValueChanged(access.startAddress(), new_values);
		}
	}
    return Error::kNoError;
  }

  Error writeHodingRegister(Address address, const SixteenBitValue &setValue) {
    SixteenBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(1);
    access.setValue(address, setValue.toUint16());
    auto error = writeRegisterValuesInternal(StorageKind::kHoldingRegisters,
                                             &holdingRegister_, access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation write holding register {}",
          error);
      return error;
    }
    return Error::kNoError;
  }

  Error writeInputRegister(Address address, const SixteenBitValue &setValue) {
    SixteenBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(1);
    access.setValue(address, setValue.toUint16());
    auto error = writeRegisterValuesInternal(StorageKind::kInputRegisters,
                                             &inputRegister_, access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation(set input register): {}", error);
      return error;
    }
    return Error::kNoError;
  }

  Error writeInputDiscrete(Address address, BitValue setValue) {
    SingleBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(1);
    access.setValue(address, setValue);
    auto error = writeCoilsInternal(StorageKind::kInputDiscrete,
                                    &inputDiscrete_, &access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation(set coils): {}", error);
      return error;
    }
    return Error::kNoError;
  }

  Error writeCoils(Address address, BitValue setValue) {
    SingleBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(1);
    access.setValue(address, setValue);
    auto error = writeCoilsInternal(StorageKind::kCoils, &coils_, &access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation(set coils): {}", error);
      return error;
    }
    return Error::kNoError;
  }

  Response processWriteHoldingRegisterRequest(const Request &request) {
    Q_Q(QModbusServer);
    SixteenBitAccess access;
    auto functionCode = kWriteSingleRegister;

    bool ok = access.unmarshalSingleWriteRequest(request.data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }

    auto error = writeHodingRegister(access.startAddress(),
                                     access.value(access.startAddress()));
    if (error != Error::kNoError) {
      return createErrorReponse(functionCode, error);
    }

    Response response;
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setError(Error::kNoError);
    response.setData(request.data());
    return response;
  }

  Response processWriteHoldingRegistersRequest(const Request &request) {
    SixteenBitAccess access;
    auto functionCode = kWriteMultipleRegisters;

    bool ok = access.unmarshalMulitpleWriteRequest(request.data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      return createErrorReponse(functionCode, Error::kStorageParityError);
    }

    for (int i = 0; i < access.quantity(); i++) {
      Address address = access.startAddress() + i;
      auto value = access.value(address);

      auto error = writeHodingRegister(address, value);
      if (error != Error::kNoError) {
        return createErrorReponse(functionCode, error);
      }
    }

    Response response;
    response.setFunctionCode(functionCode);
    response.setServerAddress(serverAddress_);
    response.setError(Error::kNoError);
    response.setData(access.marshalMultipleReadRequest());
    return response;
  }

  Error validateSixteenAccess(const SixteenBitAccess &access,
                              const SixteenBitAccess &myAccess) {
    Address myStartAddress = myAccess.startAddress();
    Quantity myMaxQuantity = myAccess.quantity();
    Address reqStartAddress = access.startAddress();
    Quantity reqQuantity = access.quantity();

    if (reqStartAddress < myStartAddress ||
        reqStartAddress + reqQuantity > myStartAddress + myMaxQuantity) {
      return Error::kIllegalDataAddress;
    }
    return Error::kNoError;
  }

  Error validateSingleBitAccess(const SingleBitAccess &access,
                                const SingleBitAccess &myAccess) {
    auto requestStartAddress = access.startAddress();
    auto requestQuantity = access.quantity();
    auto myStartAddress = myAccess.startAddress();
    auto myQuantity = myAccess.quantity();

    if (requestStartAddress < myStartAddress ||
        requestStartAddress > myStartAddress + myQuantity) {
      return Error::kIllegalDataAddress;
    }

    if (requestStartAddress + requestQuantity > myStartAddress + myQuantity) {
      return Error::kIllegalDataAddress;
    }
    return Error::kNoError;
  }

  int maxClient_ = 1;
  QMap<QString, QString> blacklist_;
  TransferMode transferMode_ = TransferMode::kMbap;
  QMap<FunctionCode, HandleFuncEntry> handleFuncRouter_;
  QMap<qintptr, ClientSession> sessionList_;
  AbstractServer *server_ = nullptr;
  ServerAddress serverAddress_ = 1;
  canWriteSingleBitValueFunc canWriteSingleBitValue_;
  canWriteSixteenBitValueFunc canWriteSixteenBitValue_;
  QModbusServer *q_ptr;

  SingleBitAccess inputDiscrete_;
  SingleBitAccess coils_;
  SixteenBitAccess inputRegister_;
  SixteenBitAccess holdingRegister_;
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
