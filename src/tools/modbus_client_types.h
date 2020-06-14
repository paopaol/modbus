#ifndef MODBUS_CLIENT_TYPES_H
#define MODBUS_CLIENT_TYPES_H

#include <memory>
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

enum class ConnectionState { kOpening, kOpened, kClosing, kClosed };
inline std::ostream &operator<<(std::ostream &output,
                                const ConnectionState &state) {
  switch (state) {
  case ConnectionState::kOpening:
    output << "opening";
    break;
  case ConnectionState::kOpened:
    output << "opened";
    break;
  case ConnectionState::kClosing:
    output << "closing";
    break;
  case ConnectionState::kClosed:
    output << "closed";
    break;
  default:
    output.setstate(std::ios_base::failbit);
  }

  return output;
}

struct Element {
  Request request;
  Response response;
  size_t bytesWritten = 0;
  ByteArray dataRecived; // recived data from serial or socket or other
  int retryTimes = 0;
  std::shared_ptr<Frame> requestFrame;
  std::shared_ptr<Frame> responseFrame;
};
using ElementQueue = std::queue<Element>;
inline Element createElement(const Request &request) {
  Element element;

  element.request = request;
  element.response.setDataChecker(request.dataChecker());
  return element;
}

} // namespace modbus

#endif /* MODBUS_CLIENT_TYPES_H */
