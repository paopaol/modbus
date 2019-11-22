#ifndef MODBUS_CLIENT_TYPES_H
#define MODBUS_CLIENT_TYPES_H

#include <modbus/base/modbus.h>
#include <queue>

namespace modbus {
template <typename StateType> class StateManager {
public:
  StateManager() {}
  StateManager(StateType state) : state_(state) {}
  void setState(StateType state) { state_ = state; }
  StateType state() { return state_; }

private:
  StateType state_;
};

enum class ConnectionState { kOpening, kOpened, kClosing, kClosed, kError };

struct Element {
  Request request;
  Response response;
  size_t byteWritten = 0;
};
using ElementQueue = std::queue<Element>;
inline Element createElement(const Request &request) {
  Element element;

  element.request = request;
  return element;
}

} // namespace modbus

#endif /* MODBUS_CLIENT_TYPES_H */
