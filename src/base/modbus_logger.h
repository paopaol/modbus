#ifndef MODBUS_LOGGER_H
#define MODBUS_LOGGER_H

#include <modbus/base/modbus.h>

namespace modbus {
void log(LogLevel level, const std::string &msg);
}

#endif /* MODBUS_LOGGER_H */
