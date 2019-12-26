#include <modbus/base/modbus.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus_logger.h>

namespace modbus {
static bool validateSixteenBitAccessResponse(const Response &resp);

Request createReadRegistersRequest(ServerAddress serverAddress,
                                   FunctionCode functionCode,
                                   const SixteenBitAccess &access) {
  static const DataChecker readMultipleRegisters_ = {
      modbus::bytesRequired<4>, modbus::bytesRequiredStoreInArrayIndex0};

  Request request;

  request.setServerAddress(serverAddress);
  if (functionCode != FunctionCode::kReadHoldingRegisters &&
      functionCode != FunctionCode::kReadInputRegister) {
    log(LogLevel::kWarning, "invalid function code for read registers" +
                                std::to_string(functionCode));
  }
  request.setFunctionCode(functionCode);
  request.setUserData(access);
  request.setData(access.marshalMultipleReadRequest());
  request.setDataChecker(readMultipleRegisters_);

  return request;
}

bool processReadRegisters(const Request &request, const Response &response,
                          SixteenBitAccess *access) {
  if (!access) {
    log(LogLevel::kError, "SixteenBitAccess access is nullptr");
    return false;
  }

  bool success = validateSixteenBitAccessResponse(response);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SixteenBitAccess>(request.userData());
  success = access->unmarshalReadResponse(response.data());
  if (!success) {
    log(LogLevel::kWarning, "unmarshalReadRegister: data is invalid");
    return false;
  }
  return true;
}

Request createWriteSingleRegisterRequest(ServerAddress serverAddress,
                                         const SixteenBitAccess &access) {
  static const DataChecker dataChecker = {bytesRequired<4>, bytesRequired<4>};
  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(FunctionCode::kWriteSingleRegister);
  request.setDataChecker(dataChecker);
  request.setData(access.marshalSingleWriteRequest());
  request.setUserData(access);
  return request;
}

static bool validateSixteenBitAccessResponse(const Response &resp) {
  if (resp.error() != modbus::Error::kNoError) {
    log(LogLevel::kError, resp.errorString().c_str());
    return false;
  }

  if (resp.isException()) {
    log(LogLevel::kError, resp.errorString().c_str());
    return false;
  }
  return true;
}

Request createWriteMultipleRegisterRequest(ServerAddress serverAddress,
                                           const SixteenBitAccess &access) {
  static const DataChecker dataChecker = {bytesRequiredStoreInArrayIndex4,
                                          bytesRequired<4>};
  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(FunctionCode::kWriteMultipleRegisters);
  request.setDataChecker(dataChecker);
  request.setData(access.marshalMultipleWriteRequest());
  request.setUserData(access);
  return request;
}

} // namespace modbus
