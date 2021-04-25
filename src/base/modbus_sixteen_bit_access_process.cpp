#include "modbus_logger.h"
#include <modbus/base/modbus.h>
#include <modbus/base/sixteen_bit_access.h>
#include <sstream>

namespace modbus {
static bool validateSixteenBitAccessResponse(const Response &resp,
                                             const std::string &log_prefix);

bool processReadRegisters(const Request &request, const Response &response,
                          SixteenBitAccess *access,
                          const std::string &log_prefix) {
  if (!access) {
    log(log_prefix, LogLevel::kError, "SixteenBitAccess access is nullptr");
    return false;
  }

  bool success = validateSixteenBitAccessResponse(response, log_prefix);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SixteenBitAccess>(request.userData());
  success = access->unmarshalReadResponse(response.data());
  if (!success) {
    log(log_prefix, LogLevel::kWarning,
        "unmarshalReadRegister: data is invalid");
    return false;
  }
  return true;
}

static bool validateSixteenBitAccessResponse(const Response &resp,
                                             const std::string &log_prefix) {
  if (resp.isException()) {
    log(log_prefix, LogLevel::kError, resp.errorString());
    return false;
  }
  return true;
}

} // namespace modbus
