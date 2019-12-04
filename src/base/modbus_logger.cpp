#include <assert.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <modbus/base/modbus.h>

using namespace std::chrono;

namespace modbus {
static LogWriter g_logger = [](LogLevel level, const std::string &msg) {
  std::string levelString;
  auto now = system_clock::to_time_t(system_clock::now());
  auto date = std::put_time(std::localtime(&now), "%F %T");

  switch (level) {
  case LogLevel::kDebug:
    levelString = "[Debug  ] ";
    break;
  case LogLevel::kInfo:
    levelString = "[Info   ] ";
    break;
  case LogLevel::kWarning:
    levelString = "[Warning] ";
    break;
  }
  std::cout << levelString << date << " - " << msg << std::endl;
};
void registerLogMessage(const LogWriter &logger) { g_logger = logger; }

void log(LogLevel level, const std::string &msg) { g_logger(level, msg); }

} // namespace modbus
