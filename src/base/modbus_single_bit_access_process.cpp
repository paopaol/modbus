#include "modbus_logger.h"
#include <modbus/base/single_bit_access.h>

namespace modbus {
static bool validateSingleBitAccessResponse(const modbus::Response &resp,
                                            const std::string &log_prefix);

bool processReadSingleBit(const Request &request, const Response &response,
                          SingleBitAccess *access,
                          const std::string &log_prefix) {
  if (!access) {
    log(log_prefix, LogLevel::kError, "SingleBitAccess access is nullptr");
    return false;
  }

  bool success = validateSingleBitAccessResponse(response, log_prefix);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SingleBitAccess>(request.userData());
  success = access->unmarshalReadResponse(response.data());
  if (!success) {
    log(log_prefix, LogLevel::kWarning,
        "unmarshal single bit access: data is invalid");
    return false;
  }
  return true;
}

static bool validateSingleBitAccessResponse(const modbus::Response &resp,
                                            const std::string &log_prefix) {
  if (resp.isException()) {
    log(log_prefix, LogLevel::kError, resp.errorString());
    return false;
  }
  return true;
}
} // namespace modbus
