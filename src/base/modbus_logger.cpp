#include <assert.h>
#include <modbus/base/modbus.h>

namespace modbus {
static LogWriter g_logger;
void registerLogMessage(const LogWriter &logger) { g_logger = logger; }

void log(LogLevel level, const std::string &msg) {
  if (g_logger) {
    g_logger(level, msg);
  }
}

} // namespace modbus
