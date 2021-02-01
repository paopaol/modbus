#ifndef MODBUS_CLIENT_TYPES_H
#define MODBUS_CLIENT_TYPES_H

#include <deque>
#include <memory>
#include <modbus/base/modbus.h>

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
  std::unique_ptr<Request> request = nullptr;
  Response response;
  size_t bytesWritten = 0;
  ByteArray dataRecived; // recived data from serial or socket or other
  int retryTimes = 0;
  std::unique_ptr<Frame> requestFrame;
  std::unique_ptr<Frame> responseFrame;
};
using ElementQueue = std::deque<Element *>;
inline void createElement(std::unique_ptr<Request> &request, Element *element) {
  element->request.swap(request);
  element->response.setDataChecker(element->request->dataChecker());
}

} // namespace modbus

#endif /* MODBUS_CLIENT_TYPES_H */
