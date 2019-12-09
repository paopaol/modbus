#ifndef __MODBUS_FRAME_H_
#define __MODBUS_FRAME_H_

#include <modbus/base/modbus.h>
#include <modbus/base/modbus_exception_datachecket.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>

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

inline DataChecker::Result unmarshalAdu(const ByteArray &data, Adu *adu,
                                        Error *error) {
  *error = Error::kNoError;
  /// make sure got serveraddress + function code
  if (data.size() < 2) {
    return DataChecker::Result::kNeedMoreData;
  }

  adu->setServerAddress(data[0]);
  adu->setFunctionCode(static_cast<FunctionCode>(data[1]));

  size_t expectSize = 0;
  DataChecker dataChecker;
  if (adu->isException()) {
    dataChecker = expectionResponseDataChecker;
  } else {
    dataChecker = adu->dataChecker();
  }

  smart_assert(dataChecker.calculateResponseSize &&
               "not set data size checker");
  auto result =
      dataChecker.calculateResponseSize(expectSize, tool::subArray(data, 2));
  if (result == DataChecker::Result::kNeedMoreData) {
    return result;
  }

  adu->setData(tool::subArray(data, 2, expectSize));
  return DataChecker::Result::kSizeOk;
}

class RtuFrame final : public Frame {
public:
  RtuFrame(){};
  ~RtuFrame(){};
  ByteArray marshal() override {
    return rtuMarshalFrame(adu_.marshalAduWithoutCrc());
  }
  size_t marshalSize() override { return adu_.marshalSize() + 2 /*crc*/; }

  DataChecker::Result unmarshal(const ByteArray &data, Error *error) override {
    auto result = unmarshalAdu(data, &adu_, error);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }

    /// server data(expectSize) + crc(2)
    size_t expectSize = adu_.marshalSize();
    size_t totalSize = expectSize + 2;
    if (data.size() != totalSize) {
      return DataChecker::Result::kNeedMoreData;
    }

    auto dataWithCrc = tool::appendCrc(tool::subArray(data, 0, expectSize));

    /**
     * Received frame error
     */
    if (dataWithCrc != data) {
      *error = Error::kStorageParityError;
    }
    if (adu_.isException()) {
      *error = Error(adu_.data()[0]);
    }

    return DataChecker::Result::kSizeOk;
  }
};
} // namespace modbus

#endif // __MODBUS_SERIAL_PORT_H_
