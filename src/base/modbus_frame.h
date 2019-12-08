#ifndef __MODBUS_FRAME_H_
#define __MODBUS_FRAME_H_

#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>

namespace modbus {
static void appendStdString(ByteArray &array, const std::string &subString) {
  array.insert(array.end(), subString.begin(), subString.end());
}

inline ByteArray rtuMarshalFrame(const ByteArray &data) {
  return tool::appendCrc(data);
}

inline ByteArray asciiMarshalFrame(const ByteArray &data) {
  ByteArray ascii;
  ByteArray binary = tool::appendLrc(data);

  appendStdString(ascii, ":");
  appendStdString(ascii, tool::dumpHex(binary, ""));
  appendStdString(ascii, "\r\n");
  return ascii;
}

class RtuFrame final : public Frame {
public:
  RtuFrame(){};
  ~RtuFrame(){};
  ByteArray marshal() override {
    return rtuMarshalFrame(adu_.marshalAduWithoutCrc());
  }
  DataChecker::Result unmarshal(const ByteArray) override {}
};
} // namespace modbus

#endif // __MODBUS_SERIAL_PORT_H_
