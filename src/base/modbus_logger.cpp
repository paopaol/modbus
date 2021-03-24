#include <assert.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <modbus/base/modbus.h>

using namespace std::chrono;

namespace modbus {
static std::string timeOfNow();
static LogWriter g_logger = [](LogLevel level, const std::string &msg) {
  std::string levelString;
  std::string date = timeOfNow();

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
  case LogLevel::kError:
    levelString = "[Error  ] ";
    break;
  }
  std::cout << levelString << date << " - " << msg << std::endl;
};

void registerLogMessage(const LogWriter &logger) {
  static std::once_flag once_;
  std::call_once(once_, [&]() { g_logger = logger; });
}

void logString(LogLevel level, const std::string &msg) { g_logger(level, msg); }

static std::string timeOfNow() {
  char tmp[128] = {0};
  time_t timep;

  time(&timep);
  struct tm now_time;
#ifdef WIN32
  localtime_s(&now_time, &timep);
#else
  localtime_r(&timep, &now_time);
#endif
  strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &now_time);
  return tmp;
}

} // namespace modbus
