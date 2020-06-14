#ifndef EXCEPTION_RESPONSE_DATACHECKER_H
#define EXCEPTION_RESPONSE_DATACHECKER_H

#include <modbus/base/modbus.h>

namespace modbus {
static const DataChecker expectionResponseDataChecker = {bytesRequired<1>};
}

#endif /* EXCEPTION_RESPONSE_DATACHECKER_H */
