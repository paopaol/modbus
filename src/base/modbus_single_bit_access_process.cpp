#include <modbus/base/single_bit_access.h>
#include <modbus_logger.h>

namespace modbus {
static bool validateSingleBitAccessResponse(const modbus::Response &resp);

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

Request createRequest(ServerAddress serverAddress, FunctionCode functionCode,
                      const DataChecker &dataChecker,
                      const SingleBitAccess &access, const ByteArray &data) {
  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(functionCode);
  request.setUserData(access);
  request.setData(data);
  request.setDataChecker(dataChecker);

  return request;
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
