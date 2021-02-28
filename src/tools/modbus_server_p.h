#ifndef __MODBUS_SERVER_P_H_
#define __MODBUS_SERVER_P_H_

#include "modbusserver_client_session.h"
#include <algorithm>
#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <bits/stdint-uintn.h>
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

#define sessionIteratorOrReturn(it, fd)                                        \
  auto it = sessionList_.find(fd);                                             \
  if (it == sessionList_.end()) {                                              \
    return;                                                                    \
  }

#define DeferRun(functor)                                                      \
  std::shared_ptr<void> _##__LINE__(nullptr, std::bind(functor))

struct HandleFuncEntry {
  FunctionCode functionCode;
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

  void enableDump(bool enable) { enableDump_ = enable; }

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

  void handleFunc(FunctionCode functionCode, SingleBitAccess *access) {
    HandleFuncEntry entry;

    entry.functionCode = functionCode;
    smart_assert(access && "invalid access")(functionCode);
    entry.singleBitAccess = access;

    handleFuncRouter_[functionCode] = entry;

    log(LogLevel::kInfo, "route add Function[{}] StartAddress[{}] Quantity[{}]",
        functionCode, access->startAddress(), access->quantity());
  }

  void handleFunc(FunctionCode functionCode, SixteenBitAccess *access) {
    HandleFuncEntry entry;

    entry.functionCode = functionCode;
    smart_assert(access && "invalid access")(functionCode);
    entry.sixteenBitAccess = access;

    handleFuncRouter_[functionCode] = entry;

    log(LogLevel::kInfo, "route add Function[{}] StartAddress[{}] Quantity[{}]",
        functionCode, access->startAddress(), access->quantity());
  }

  bool updateValue(FunctionCode functionCode, Address address,
                   const SixteenBitValue &newValue) {
    smart_assert((functionCode == FunctionCode::kWriteSingleRegister ||
                  functionCode == FunctionCode::kWriteMultipleRegisters ||
                  functionCode == FunctionCode::kReadInputRegister ||
                  functionCode == FunctionCode::kReadHoldingRegisters) &&
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

  bool coilsValue(const SingleBitAccess &access, Address address) {
    return access.value(address);
  }

  bool holdingRegisterValue(Address address, SixteenBitValue *value) {
    return registerValue(holdingRegister_, address, value);
  }

  bool inputRegisterValue(Address address, SixteenBitValue *value) {
    return registerValue(inputRegister_, address, value);
  }

  bool coilsValue(Address address) { return coilsValue(coils_, address); }

  bool inputDiscreteValue(Address address) {
    return coilsValue(inputDiscrete_, address);
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
    connect(connection, &AbstractConnection::messageArrived, this,
            &QModbusServerPrivate::onMessageArrived);

    auto session = std::make_shared<ClientSession>(
        this, connection, creatDefaultCheckSizeFuncTableForServer());
    sessionList_[connection->fd()] = session;
  }

  void removeClient(qintptr fd) {
    sessionIteratorOrReturn(it, fd);
    log(LogLevel::kInfo, "{} closed", it.value()->fullName());
    sessionList_.erase(it);
  }

  void onMessageArrived(quintptr fd, const BytesBufferPtr &buffer) {
    sessionIteratorOrReturn(sessionIt, fd);
    auto &session = sessionIt.value();
    if (enableDump_) {
      log(LogLevel::kDebug, "R[{}]:[{}]", session->fullName(),
          dump(transferMode_, *buffer));
    }
    session->handleModbusRequest(*buffer);
  }

  void processRequest(const Adu *request, Adu *response) {
    using modbus::FunctionCode;
    switch (request->functionCode()) {
    case kReadCoils:
    case kReadInputDiscrete: {
      processReadSingleBitRequest(request, response);
    } break;
    case kWriteSingleCoil: {
      processWriteCoilRequest(request, response);
    } break;
    case kWriteMultipleCoils: {
      processWriteCoilsRequest(request, response);
    } break;
    case kReadHoldingRegisters:
    case kReadInputRegister: {
      processReadMultipleRegisters(request, response);
    } break;
    case kWriteSingleRegister: {
      processWriteHoldingRegisterRequest(request, response);
    } break;
    case kWriteMultipleRegisters: {
      processWriteHoldingRegistersRequest(request, response);
    } break;
    default:
      smart_assert(0 && "unsuported function")(request->functionCode());
      break;
    }
  }

  void processBrocastRequest(const Adu *request) {}

  void createErrorReponse(FunctionCode functionCode, Error errorCode,
                          Adu *response) {
    response->setServerAddress(serverAddress_);
    response->setFunctionCode(FunctionCode(functionCode | Adu::kExceptionByte));
    response->setData(ByteArray({uint8_t(errorCode)}));
  }

  void processWriteCoilsRequest(const Adu *request, Adu *response) {
    FunctionCode functionCode = request->functionCode();

    SingleBitAccess access;
    bool ok = access.unmarshalMultipleWriteRequest(request->data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(functionCode, Error::kStorageParityError, response);
      return;
    }

    auto error = handleClientwriteCoils(functionCode, request, coils_, access);
    if (error != Error::kNoError) {
      createErrorReponse(functionCode, error, response);
      return;
    }

    response->setFunctionCode(functionCode);
    response->setServerAddress(serverAddress_);
    response->setData(access.marshalAddressQuantity());
  }

  Error writeCoilsInternal(StorageKind kind, SingleBitAccess *my,
                           const SingleBitAccess *you) {
    Q_Q(QModbusServer);
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

  Error handleClientwriteCoils(FunctionCode functionCode, const Adu *request,
                               SingleBitAccess &my,
                               const SingleBitAccess &you) {
    Q_Q(QModbusServer);
    auto error = validateSingleBitAccess(you, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          functionCode, my.startAddress(), my.quantity(), you.startAddress(),
          my.quantity());
      return error;
    }

    Address reqStartAddress = you.startAddress();
    for (size_t i = 0; i < you.quantity(); i++) {
      Address address = reqStartAddress + i;
      auto value = you.value(address);
      auto error = canWriteSingleBitValue(address, value);
      if (error != Error::kNoError) {
        return error;
      }
    }
    for (size_t i = 0; i < you.quantity(); i++) {
      Address address = reqStartAddress + i;
      auto value = you.value(address);
      emit q->writeCoilsRequested(address, value);
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
  void processWriteCoilRequest(const Adu *request, Adu *response) {
    FunctionCode functionCode = FunctionCode::kWriteSingleCoil;
    SingleBitAccess access;

    bool ok = access.unmarshalSingleWriteRequest(request->data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(functionCode, Error::kStorageParityError, response);
      return;
    }
    auto error = handleClientwriteCoils(functionCode, request, coils_, access);
    if (error != Error::kNoError) {
      createErrorReponse(functionCode, error, response);
      return;
    }

    response->setFunctionCode(functionCode);
    response->setServerAddress(serverAddress_);
    response->setData(access.marshalSingleWriteRequest());
  }

  void setCanWriteSingleBitValueFunc(const canWriteSingleBitValueFunc &func) {
    canWriteSingleBitValue_ = func;
  }

  void setCanWriteSixteenBitValueFunc(const canWriteSixteenBitValueFunc &func) {
    canWriteSixteenBitValue_ = func;
  }

  Error canWriteSingleBitValue(Address startAddress, bool value) {
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

  void processReadSingleBitRequest(const Adu *request, Adu *response) {
    SingleBitAccess access;
    bool ok = access.unmarshalReadRequest(request->data());
    if (!ok) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(request->functionCode(), Error::kStorageParityError,
                         response);
      return;
    }

    auto &entry = handleFuncRouter_[request->functionCode()];
    const auto &my = *entry.singleBitAccess;
    auto error = validateSingleBitAccess(access, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request code({}):myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          request->functionCode(), my.startAddress(), my.quantity(),
          access.startAddress(), my.quantity());
      createErrorReponse(request->functionCode(), error, response);
      return;
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

    response->setFunctionCode(request->functionCode());
    response->setServerAddress(serverAddress_);
    response->setData(responseAccess.marshalReadResponse());
  }

  void processReadMultipleRegisters(const Adu *request, Adu *response) {
    SixteenBitAccess access;

    bool ok = access.unmarshalAddressQuantity(request->data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(request->functionCode(), Error::kStorageParityError,
                         response);
      return;
    }
    auto entry = handleFuncRouter_[request->functionCode()];
    const auto &my = *entry.sixteenBitAccess;
    auto error = validateSixteenAccess(access, my);
    if (error != Error::kNoError) {
      log(LogLevel::kError,
          "invalid request ({}) :myStartAddress({}),myMaxQuantity({}),"
          "requestStartAddress({}),requestQuantity({})",
          request->functionCode(), my.startAddress(), my.quantity(),
          access.startAddress(), access.quantity());
      createErrorReponse(request->functionCode(), error, response);
      return;
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
    response->setFunctionCode(request->functionCode());
    response->setServerAddress(serverAddress_);
    response->setData(responseAccess.marshalMultipleReadResponse());
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
      } else if (kind == StorageKind::kInputRegisters) {
        emit q->inputRegisterValueChanged(access.startAddress(), new_values);
      }
    }
    return Error::kNoError;
  }

  Error handleClientWriteHodingRegisters(const SixteenBitAccess &access,
                                         const SixteenBitAccess &my) {
    auto error = validateSixteenAccess(access, my);
    if (error != Error::kNoError) {
      return error;
    }
    Quantity quantity = access.quantity();
    for (size_t i = 0; i < quantity; i++) {
      Address reqStartAddress = access.startAddress() + i;
      auto value = access.value(reqStartAddress);
      auto error = canWriteSixteenBitValue(reqStartAddress, value);
      if (error != Error::kNoError) {
        return error;
      }
    }
    Q_Q(QModbusServer);
    emit q->writeHodingRegistersRequested(access.startAddress(),
                                          access.value());
    return Error::kNoError;
  }

  Error writeHodingRegisters(Address address,
                             const QVector<SixteenBitValue> &setValues) {
    SixteenBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(setValues.size());
    auto addr = address;
    for (const auto &value : setValues) {
      access.setValue(addr++, value.toUint16());
    }
    auto error = writeRegisterValuesInternal(StorageKind::kHoldingRegisters,
                                             &holdingRegister_, access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation write holding register {}",
          error);
      return error;
    }
    return Error::kNoError;
  }

  Error writeInputRegisters(Address address,
                            const QVector<SixteenBitValue> &setValues) {
    SixteenBitAccess access;
    access.setStartAddress(address);
    access.setQuantity(setValues.size());
    auto addr = address;
    for (const auto &value : setValues) {
      access.setValue(addr++, value.toUint16());
    }
    auto error = writeRegisterValuesInternal(StorageKind::kInputRegisters,
                                             &inputRegister_, access);
    if (error != Error::kNoError) {
      log(LogLevel::kError, "invalid operation(set input register): {}", error);
      return error;
    }
    return Error::kNoError;
  }

  Error writeInputDiscrete(Address address, bool setValue) {
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

  Error writeCoils(Address address, bool setValue) {
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

  void processWriteHoldingRegisterRequest(const Adu *request, Adu *response) {
    SixteenBitAccess access;

    bool ok = access.unmarshalSingleWriteRequest(request->data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(request->functionCode(), Error::kStorageParityError,
                         response);
      return;
    }

    auto error = handleClientWriteHodingRegisters(access, holdingRegister_);
    if (error != Error::kNoError) {
      createErrorReponse(request->functionCode(), error, response);
      return;
    }

    response->setFunctionCode(request->functionCode());
    response->setServerAddress(serverAddress_);
    response->setData(request->data());
  }

  void processWriteHoldingRegistersRequest(const Adu *request, Adu *response) {
    SixteenBitAccess access;

    bool ok = access.unmarshalMulitpleWriteRequest(request->data());
    if (ok == false) {
      log(LogLevel::kError, "invalid request");
      createErrorReponse(request->functionCode(), Error::kStorageParityError,
                         response);
      return;
    }

    auto error = handleClientWriteHodingRegisters(access, holdingRegister_);
    if (error != Error::kNoError) {
      createErrorReponse(request->functionCode(), error, response);
      return;
    }

    response->setFunctionCode(request->functionCode());
    response->setServerAddress(serverAddress_);
    response->setData(access.marshalMultipleReadRequest());
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
  QMap<qintptr, ClientSessionPtr> sessionList_;
  AbstractServer *server_ = nullptr;
  ServerAddress serverAddress_ = 1;
  canWriteSingleBitValueFunc canWriteSingleBitValue_;
  canWriteSixteenBitValueFunc canWriteSixteenBitValue_;
  QModbusServer *q_ptr;

  SingleBitAccess inputDiscrete_;
  SingleBitAccess coils_;
  SixteenBitAccess inputRegister_;
  SixteenBitAccess holdingRegister_;
  bool enableDump_ = true;
};
} // namespace modbus

#endif // __MODBUS_SERVER_P_H_
