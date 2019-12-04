#ifndef EXCEPTION_RESPONSE_DATACHECKER_H
#define EXCEPTION_RESPONSE_DATACHECKER_H

#include <modbus/base/modbus.h>

namespace modbus {
static const DataChecker expectionResponseDataChecker = {
    [](size_t &size, const ByteArray &array) {
      return DataChecker::Result::kUnkown;
    },
    [](size_t &size, const ByteArray &array) {
      size = 1;
      if (array.size() < 1) {
        return DataChecker::Result::kNeedMoreData;
      }
      return DataChecker::Result::kSizeOk;
    }};
}

#endif /* EXCEPTION_RESPONSE_DATACHECKER_H */
