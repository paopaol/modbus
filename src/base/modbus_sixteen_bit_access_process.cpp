#include <modbus/base/modbus.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus_logger.h>

namespace modbus {
static bool validateSixteenBitAccessResponse(const Response &resp);

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

} // namespace modbus
