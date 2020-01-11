#ifndef __MODBUS_FRAME_H_
#define __MODBUS_FRAME_H_

#include <memory>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_exception_datachecket.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>

namespace modbus {
static void appendStdString(ByteArray &array, const std::string &subString) {
  array.insert(array.end(), subString.begin(), subString.end());
}

inline ByteArray marshalRtuFrame(const ByteArray &data) {
  return tool::appendCrc(data);
}

inline ByteArray marshalAsciiFrame(const ByteArray &data) {
  ByteArray ascii;
  ByteArray binary = tool::appendLrc(data);

  auto toUpperHexString = [](const ByteArray &data) {
    auto hexString = tool::dumpHex(data, "");
    std::transform(hexString.begin(), hexString.end(), hexString.begin(),
                   ::toupper);
    return hexString;
  };

  appendStdString(ascii, ":");
  appendStdString(ascii, toUpperHexString(binary));
  appendStdString(ascii, "\r\n");
  return ascii;
}

inline DataChecker::Result
_unmarshalServerAddressFunctionCode(const ByteArray &data,
                                    ServerAddress *serverAddress,
                                    FunctionCode *functionCode) {
  /// make sure got serveraddress + function code
  if (data.size() < 2) {
    return DataChecker::Result::kNeedMoreData;
  }

  *serverAddress = data[0];
  *functionCode = static_cast<FunctionCode>(data[1]);
  return DataChecker::Result::kSizeOk;
}

inline DataChecker::Result unmarshalAdu(const ByteArray &data, Adu *adu,
                                        Error *error) {
  *error = Error::kNoError;
  ServerAddress serverAddress;
  FunctionCode functionCode;
  auto result =
      _unmarshalServerAddressFunctionCode(data, &serverAddress, &functionCode);
  if (result != DataChecker::Result::kSizeOk) {
    return result;
  }

  adu->setServerAddress(serverAddress);
  adu->setFunctionCode(functionCode);

  size_t expectSize = 0;
  DataChecker dataChecker;
  if (adu->isException()) {
    dataChecker = expectionResponseDataChecker;
  } else {
    dataChecker = adu->dataChecker();
  }

  smart_assert(dataChecker.calculateSize && "not set data size checker");
  result = dataChecker.calculateSize(expectSize, tool::subArray(data, 2));
  if (result == DataChecker::Result::kNeedMoreData) {
    return result;
  }

  adu->setData(tool::subArray(data, 2, expectSize));
  if (adu->isException()) {
    *error = Error(adu->data()[0]);
  }
  return DataChecker::Result::kSizeOk;
}

class RtuFrame final : public Frame {
public:
  RtuFrame(){};
  ~RtuFrame(){};
  ByteArray marshal(const uint16_t *frameId = nullptr) override {
    return marshalRtuFrame(adu_.marshalAduWithoutCrc());
  }
  size_t marshalSize() override { return adu_.marshalSize() + 2 /*crc*/; }

  DataChecker::Result
  unmarshalServerAddressFunctionCode(const ByteArray &data,
                                     ServerAddress *serverAddress,
                                     FunctionCode *functionCode) override {
    return _unmarshalServerAddressFunctionCode(data, serverAddress,
                                               functionCode);
  }

  DataChecker::Result unmarshal(const ByteArray &data, Error *error) override {
    auto result = unmarshalAdu(data, &adu_, error);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }

    /// server data(expectSize) + crc(2)
    size_t expectSize = adu_.marshalSize();
    size_t totalSize = expectSize + 2;
    if (data.size() < totalSize) {
      return DataChecker::Result::kNeedMoreData;
    }

    auto dataWithCrc = tool::appendCrc(tool::subArray(data, 0, expectSize));

    /**
     * Received frame error
     */
    if (dataWithCrc != data) {
      *error = Error::kStorageParityError;
    }

    return DataChecker::Result::kSizeOk;
  }
};

class AsciiFrame final : public Frame {
public:
  AsciiFrame() {}
  ~AsciiFrame() {}

  ByteArray marshal(const uint16_t *frameId = nullptr) override {
    return marshalAsciiFrame(adu_.marshalAduWithoutCrc());
  }

  size_t marshalSize() override {
    //":" + hex(adu) + hex(lrc) + "\r\n"
    return kColonSize + 2 * adu_.marshalSize() + kLrcHexSize + kCRLRSize;
  }

  DataChecker::Result
  unmarshalServerAddressFunctionCode(const ByteArray &data,
                                     ServerAddress *serverAddress,
                                     FunctionCode *functionCode) override {
    ByteArray escapedData;

    auto result = escapeColon(data, &escapedData);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }
    escapedData = tool::fromHexString(escapedData);
    return _unmarshalServerAddressFunctionCode(escapedData, serverAddress,
                                               functionCode);
  }

  /**
   * : + hex(adu) + hex(lrc) + \r\n
   */
  DataChecker::Result unmarshal(const ByteArray &data, Error *error) override {
    ByteArray subdata;

    auto result = escapeColon(data, &subdata);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }

    subdata = tool::fromHexString(subdata);
    result = unmarshalAdu(subdata, &adu_, error);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }
    if (data.size() <
        kColonSize + 2 * adu_.marshalSize() + kLrcHexSize + kCRLRSize) {
      return DataChecker::Result::kNeedMoreData;
    }
    ByteArray last2Bytes =
        tool::subArray(data, kColonSize + 2 * adu_.marshalSize() + kLrcHexSize);
    smart_assert(last2Bytes.size() == 2 &&
                 "the modbus ascii data is invalid")(tool::dumpHex(last2Bytes));
    if (last2Bytes[0] != '\r' || last2Bytes[1] != '\n') {
      return DataChecker::Result::kFailed;
    }
    auto dataWithCrc = tool::appendCrc(adu_.marshalAduWithoutCrc());
    if (dataWithCrc != tool::subArray(subdata, 0, adu_.marshalSize())) {
      *error = Error::kStorageParityError;
    }

    return DataChecker::Result::kSizeOk;
  }

private:
  DataChecker::Result escapeColon(const ByteArray &data,
                                  ByteArray *escapedData) {
    if (data.size() < kColonSize) {
      return DataChecker::Result::kNeedMoreData;
    }
    if (data[0] != ':') {
      return DataChecker::Result::kFailed;
    }

    *escapedData = tool::subArray(data, 1); /// skip ':'
    return DataChecker::Result::kSizeOk;
  }

  static const int kColonSize = 1; //':'
  static const int kLrcHexSize = 2;
  static const int kCRLRSize = 2;
};

class MbapFrame final : public Frame {
public:
  MbapFrame() {}
  ~MbapFrame() {}

  size_t marshalSize() override {
    return kTransactionMetaIdSize + kProtocolIdSize + kLenSize +
           adu_.marshalSize();
  }
  ByteArray marshal(const uint16_t *frameId = nullptr) override {
    ByteArray output;

    id_ = frameId ? *frameId : nextTransactionId();

    /// transaction meta id
    output.push_back(id_ / 256);
    output.push_back(id_ % 256);

    /// protocol id
    output.push_back(kProtocolId / 256);
    output.push_back(kProtocolId % 256);

    /// len
    size_t aduSize = adu_.marshalSize();
    output.push_back(aduSize / 256);
    output.push_back(aduSize % 256);

    ByteArray aduArray = adu_.marshalAduWithoutCrc();
    output.insert(output.end(), aduArray.begin(), aduArray.end());

    return output;
  }

  DataChecker::Result
  unmarshalServerAddressFunctionCode(const ByteArray &data,
                                     ServerAddress *serverAddress,
                                     FunctionCode *functionCode) override {
    ByteArray escapedData;
    auto result = escapeMbapHeader(data, escapedData);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }

    return _unmarshalServerAddressFunctionCode(escapedData, serverAddress,
                                               functionCode);
  }

  DataChecker::Result unmarshal(const ByteArray &data, Error *error) override {
    ByteArray escapedData;
    auto result = escapeMbapHeader(data, escapedData);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }
    result = unmarshalAdu(escapedData, &adu_, error);
    if (result != DataChecker::Result::kSizeOk) {
      return result;
    }
    return DataChecker::Result::kSizeOk;
  }

private:
  DataChecker::Result escapeMbapHeader(const ByteArray &data,
                                       ByteArray &escapedData) {
    if (data.size() < 6) {
      return DataChecker::Result::kNeedMoreData;
    }
    TransactionId transactionId = data[0] * 256 + data[1];
    uint16_t protocolId = data[2] * 256 + data[3];
    uint16_t size = data[4] * 256 + data[5];
    uint16_t totalSize = 6 + size;
    if (data.size() < totalSize) {
      return DataChecker::Result::kNeedMoreData;
    }

    escapedData = tool::subArray(data, 6);
    return DataChecker::Result::kSizeOk;
  }

  static const int kTransactionMetaIdSize = 2;
  static const int kProtocolIdSize = 2;
  static const int kLenSize = 2;
  static const int kProtocolId = 0;
};

inline std::shared_ptr<Frame> createModebusFrame(TransferMode mode) {
  switch (mode) {
  case TransferMode::kRtu:
    return std::make_shared<RtuFrame>();
  case TransferMode::kAscii:
    return std::make_shared<AsciiFrame>();
  case TransferMode::kMbap:
    return std::make_shared<MbapFrame>();
  default:
    smart_assert("unsupported modbus transfer mode")(static_cast<int>(mode));
    return nullptr;
  }
}

} // namespace modbus

#endif // __MODBUS_SERIAL_PORT_H_
