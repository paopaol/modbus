#ifndef MODBUS_LOGGER_H
#define MODBUS_LOGGER_H

#include <fmt/core.h>
#include <modbus/base/modbus.h>

namespace modbus {
void logString(LogLevel level, const std::string &msg);

template <typename S, typename... Args, typename Char = fmt::char_t<S>>
void log(LogLevel level, const S &format_str, Args &&... args) {
  logString(level, fmt::format(format_str, args...));
}
} // namespace modbus

#endif /* MODBUS_LOGGER_H */
