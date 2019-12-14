#include <modbus/base/single_bit_access.h>
#include <modbus_logger.h>

namespace modbus {
static bool validateSingleBitAccessResponse(const modbus::Response &resp);

Request createReadSingleBitRequest(ServerAddress serverAddress,
                                   const SingleBitAccess &access,
                                   FunctionCode functionCode) {
  static const DataChecker dataChecker = {bytesRequired<4>,
                                          bytesRequiredStoreInArrayIndex0};

  if (functionCode != FunctionCode::kReadCoils &&
      functionCode != FunctionCode::kReadInputDiscrete) {
    log(LogLevel::kWarning, "single bit access:[read] invalid function code(" +
                                std::to_string(functionCode) + ")");
  }

  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(functionCode);
  request.setUserData(access);
  request.setData(access.marshalReadRequest());
  request.setDataChecker(dataChecker);

  return request;
}

bool processReadSingleBit(const Request &request, const Response &response,
                          SingleBitAccess *access) {
  if (!access) {
    log(LogLevel::kError, "SingleBitAccess access is nullptr");
    return false;
  }

  bool success = validateSingleBitAccessResponse(response);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SingleBitAccess>(request.userData());
  success = access->unmarshalReadResponse(response.data());
  if (!success) {
    log(LogLevel::kWarning, "unmarshal single bit access: data is invalid");
    return false;
  }
  return true;
}

static bool validateSingleBitAccessResponse(const modbus::Response &resp) {
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
} // namespace modbus
