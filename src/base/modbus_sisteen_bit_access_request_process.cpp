#include <modbus/base/modbus.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus_logger.h>

namespace modbus {
static bool validateSixteenBitAccessResponse(const modbus::Response &resp);

Request createReadMultipleRegistersRequest(ServerAddress serverAddress,
                                           const SixteenBitAccess &access) {
  static const DataChecker readMultipleRegisters_ = {
      modbus::bytesRequired<4>, modbus::bytesRequiredStoreInArrayIndex0};

  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(FunctionCode::kReadwriteMultipleRegisters);
  request.setUserData(access);
  request.setData(access.marshalMultipleReadRequest());
  request.setDataChecker(readMultipleRegisters_);

  return request;
}

bool processReadMultipleRegisters(const Request &request,
                                  const Response &response,
                                  SixteenBitAccess *access) {
  if (!access) {
    log(LogLevel::kWarning, "SixteenBitAccess access is nullptr");
    return false;
  }

  bool success = validateSixteenBitAccessResponse(response);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SixteenBitAccess>(request.userData());
  success = access->unmarshalReadResponse(response.data());
  if (!success) {
    log(LogLevel::kWarning, "unmarshalMultipleReadRegister: data is invalid");
    return false;
  }
  return true;
}

static bool validateSixteenBitAccessResponse(const modbus::Response &resp) {
  if (resp.error() != modbus::Error::kNoError) {
    log(LogLevel::kWarning, resp.errorString().c_str());
    return false;
  }

  if (resp.isException()) {
    log(LogLevel::kWarning, resp.errorString().c_str());
    return false;
  }
  return true;
}
} // namespace modbus
