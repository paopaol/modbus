#include <assert.h>
#include <modbus/base/modbus.h>

namespace modbus {
static LogWriter g_logger = [](LogLevel level, const std::string &msg) {
  switch (level) {
  case LogLevel::kDebug:
    std::cout << "[Debug] " << msg << std::endl;
    break;
  case LogLevel::kInfo:
    std::cout << "[Info] " << msg << std::endl;
    break;
  case LogLevel::kWarning:
    std::cout << "[Warning] " << msg << std::endl;
    break;
  }
};
void registerLogMessage(const LogWriter &logger) { g_logger = logger; }

void log(LogLevel level, const std::string &msg) { g_logger(level, msg); }

} // namespace modbus
