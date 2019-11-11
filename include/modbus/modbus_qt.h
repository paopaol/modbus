#ifndef __MODBUS_QT_H_
#define __MODBUS_QT_H_

#include "modbus.h"

namespace modbus {
// for qt framework
class QResponse {
public:
  Request *request() const { return request_; }
  std::vector<char> dataByteArray() const { return dataByteArray_; }

private:
  Request *request_;
  std::vector<char> dataByteArray_;
};
} // namespace modbus

#endif // __MODBUS_QT_H_
