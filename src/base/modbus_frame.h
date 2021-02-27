#ifndef __MODBUS_FRAME_H_
#define __MODBUS_FRAME_H_

#include <array>
#include <memory>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_exception_datachecket.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>
#include <qmap.h>
#include <unordered_map>

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
    escapedData = tool::fromHexString(escapedData.data(), escapedData.size());
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

    subdata = tool::fromHexString(subdata.data(), subdata.size());
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
    output.reserve(6 + adu_.marshalSize());

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
    for (int i = 0, size = aduArray.size(); i < size; i++) {
      output.push_back(aduArray[i]);
    }

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
    id_ = data[0] * 256 + data[1];
    // uint16_t protocolId = data[2] * 256 + data[3];
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

inline std::unique_ptr<Frame> createModbusFrame(TransferMode mode) {
  switch (mode) {
  case TransferMode::kRtu:
    return std::unique_ptr<Frame>(new RtuFrame());
  case TransferMode::kAscii:
    return std::unique_ptr<Frame>(new AsciiFrame());
  case TransferMode::kMbap:
    return std::unique_ptr<Frame>(new MbapFrame());
  default:
    smart_assert("unsupported modbus transfer mode")(static_cast<int>(mode));
    return nullptr;
  }
}

inline CheckSizeFuncTable creatDefaultCheckSizeFuncTableForClient() {
  static const CheckSizeFuncTable table = {
      nullptr,
      bytesRequiredStoreInArrayIndex2<0>, // kReadCoils 0x01
      bytesRequiredStoreInArrayIndex2<0>, // kReadInputDiscrete 0x02
      bytesRequiredStoreInArrayIndex2<0>, // kReadHoldingRegisters 0x03
      bytesRequiredStoreInArrayIndex2<0>, // kReadInputRegister 0x04
      bytesRequired2<4>,                  // kWriteSingleCoil 0x05
      bytesRequired2<4>,                  // kWriteSingleRegister 0x06
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      bytesRequired2<4>, //  kWriteMultipleCoils = 0x0f,
      bytesRequired2<4>, //  kWriteMultipleRegisters = 0x10,
      nullptr, nullptr, nullptr,
      nullptr,                            //  kReadFileRecords = 0x14,
      nullptr,                            //  kWriteFileRecords = 0x15,
      nullptr,                            //  kMaskWriteRegister = 0x16,
      bytesRequiredStoreInArrayIndex2<0>, //  kReadWriteMultipleRegisters =
                                          //  0x17,
      nullptr, //  kReadDeviceIdentificationCode = 0x2b
  };
  return table;
}

class ModbusRtuFrameDecoder : public ModbusFrameDecoder {
  enum class State { kServerAddress, kFunctionCode, kData, kCrc0, kCrc1, kEnd };

public:
  explicit ModbusRtuFrameDecoder(const CheckSizeFuncTable &table)
      : ModbusFrameDecoder(table) {
    Clear();
  }

  ~ModbusRtuFrameDecoder() override = default;

  DataChecker::Result Decode(pp::bytes::Buffer &buffer, Adu *adu) override {
    DataChecker::Result result = DataChecker::Result::kNeedMoreData;
    while (buffer.Len() > 0 || state_ == State::kEnd) {
      switch (state_) {
      case State::kServerAddress: {
        const auto serverAddress = buffer.ReadByte();
        adu->setServerAddress(serverAddress);
        crcCtx_.crc16((const uint8_t *)&serverAddress, 1);

        state_ = State::kFunctionCode;
      } break;
      case State::kFunctionCode: {
        auto functionCode = buffer.ReadByte();
        adu->setFunctionCode(static_cast<FunctionCode>(functionCode));
        crcCtx_.crc16((const uint8_t *)&functionCode, 1);

        state_ = State::kData;

        function_ = adu->isException()
                        ? bytesRequired2<1>
                        : checkSizeFuncTable_[adu->functionCode()];

        if (!function_) {
          error_ = Error::kIllegalFunctionCode;
          state_ = State::kEnd;
        }

      } break;
      case State::kData: {
        size_t expectSize = 0;

        uint8_t *p;
        buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());

        result = function_(expectSize, p, buffer.Len());
        if (result == DataChecker::Result::kNeedMoreData) {
          goto exit_function;
        }
        // we need crc,
        result = DataChecker::Result::kNeedMoreData;

        buffer.ZeroCopyRead(&p, expectSize);

        adu->setData(p, expectSize);
        crcCtx_.crc16((uint8_t *)p, expectSize);

        state_ = State::kCrc0;
      } break;
      case State::kCrc0: {
        crc_[0] = buffer.ReadByte();

        state_ = State::kCrc1;
      } break;
      case State::kCrc1: {
        crc_[1] = buffer.ReadByte();

        uint16_t crc = crcCtx_.end();
        if (crc % 256 != crc_[0] || crc / 256 != crc_[1]) {
          error_ = Error::kStorageParityError;
        } else if (adu->isException()) {
          error_ = Error(adu->data()[0]);
        }

        state_ = State::kEnd;
      } break;
      case State::kEnd: {

        result = DataChecker::Result::kSizeOk;
        isDone_ = true;
        goto exit_function;
      } break;
      }
    }
  exit_function:
    return result;
  }

  bool IsDone() const override { return isDone_; }

  void Clear() override {
    state_ = State::kServerAddress;
    isDone_ = false;
    crcCtx_.clear();
    error_ = Error::kNoError;
  }

  Error LasError() const override { return error_; }

private:
  State state_ = State::kServerAddress;
  bool isDone_ = false;
  uint8_t crc_[2];
  CrcCtx crcCtx_;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
};

class ModbusAsciiFrameDecoder : public ModbusFrameDecoder {
  enum class State {
    kStartChar,
    kServerAddress,
    kFunctionCode,
    kData,
    LRC,
    kEndChar,
    kEnd
  };

public:
  explicit ModbusAsciiFrameDecoder(const CheckSizeFuncTable &table)
      : ModbusFrameDecoder(table) {
    Clear();
  }

  ~ModbusAsciiFrameDecoder() override = default;

  DataChecker::Result Decode(pp::bytes::Buffer & /**/, Adu *) override {
    assert("ascii mode:not support yet");

    return DataChecker::Result::kSizeOk;
  }

  bool IsDone() const override { return isDone_; }

  void Clear() override {
    state_ = State::kServerAddress;
    isDone_ = false;
    crcCtx_.clear();
    error_ = Error::kNoError;
    function_ = nullptr;
  }

  Error LasError() const override { return error_; }

private:
  State state_ = State::kServerAddress;
  bool isDone_ = false;
  CrcCtx crcCtx_;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
};

class ModbusMbapFrameDecoder : public ModbusFrameDecoder {
  enum class State { kMBap, kServerAddress, kFunctionCode, kData, kEnd };

public:
  explicit ModbusMbapFrameDecoder(const CheckSizeFuncTable &table)
      : ModbusFrameDecoder(table) {
    Clear();
  }

  ~ModbusMbapFrameDecoder() override = default;

  DataChecker::Result Decode(pp::bytes::Buffer &buffer, Adu *adu) override {
    DataChecker::Result result = DataChecker::Result::kNeedMoreData;
    while (buffer.Len() > 0 || state_ == State::kEnd) {
      switch (state_) {
      case State::kMBap: {
        if (buffer.Len() < 6) {
          goto exit_function;
        }
        char *p;
        buffer.ZeroCopyRead(&p, 6);

        // transaction id
        adu->setTransactionId(p[0] * 256 + p[1]);

        flag_ = p[2] * 256 + p[3];
        len_ = p[4] * 256 + p[5];

        state_ = State::kServerAddress;
      } break;
      case State::kServerAddress: {
        if (buffer.Len() < len_) {
          goto exit_function;
        }

        const auto serverAddress = buffer.ReadByte();
        adu->setServerAddress(serverAddress);

        state_ = State::kFunctionCode;
      } break;
      case State::kFunctionCode: {
        auto functionCode = buffer.ReadByte();
        adu->setFunctionCode(static_cast<FunctionCode>(functionCode));

        state_ = State::kData;

        function_ = adu->isException()
                        ? bytesRequired2<1>
                        : checkSizeFuncTable_[adu->functionCode()];

        if (!function_) {
          error_ = Error::kIllegalFunctionCode;
          state_ = State::kEnd;
        }
      } break;
      case State::kData: {
        size_t expectSize = 0;

        uint8_t *p;
        buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());

        result = function_(expectSize, p, buffer.Len());
        if (result == DataChecker::Result::kNeedMoreData) {
          goto exit_function;
        }

        buffer.ZeroCopyRead(&p, expectSize);

        adu->setData((uint8_t *)p, expectSize);
        if (adu->isException()) {
            error_ = Error(adu->data()[0]);
        }

        state_ = State::kEnd;
      } break;

      case State::kEnd: {

        result = DataChecker::Result::kSizeOk;
        isDone_ = true;
        goto exit_function;
      } break;
      }
    }
  exit_function:
    return result;
  }

  bool IsDone() const override { return isDone_; }

  void Clear() override {
    state_ = State::kMBap;
    isDone_ = false;
    error_ = Error::kNoError;
    flag_ = 0;
    len_ = 0;
  }

  Error LasError() const override { return error_; }

private:
  State state_ = State::kMBap;
  bool isDone_ = false;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
  uint16_t flag_ = 0;
  uint16_t len_ = 0;
};

class ModbusRtuFrameEncoder : public ModbusFrameEncoder {
public:
  virtual ~ModbusRtuFrameEncoder() = default;

  void Encode(const Adu *adu, pp::bytes::Buffer &buffer) override {
    buffer.Write(adu->serverAddress());
    if (adu->isException()) {
      buffer.Write(FunctionCode(adu->functionCode() | Pdu::kExceptionByte));
    } else {
      buffer.Write(adu->functionCode());
    }
    buffer.Write(adu->data());

    uint8_t *data;
    int len = buffer.Len();
    buffer.ZeroCopyPeekAt(&data, 0, len);
    auto crc = tool::crc16_modbus(data, len);
    buffer.Write(crc % 256);
    buffer.Write(crc / 256);
  }
};

class ModbusMbapFrameEncoder : public ModbusFrameEncoder {
public:
  virtual ~ModbusMbapFrameEncoder() = default;

  void Encode(const Adu *adu, pp::bytes::Buffer &buffer) override {
    const auto &data = adu->data();

    buffer.Write(adu->transactionId() / 256);
    buffer.Write(adu->transactionId() % 256);

    // protocol id
    buffer.Write(0);
    buffer.Write(0);

    buffer.Write(data.size() / 256);
    buffer.Write(data.size() % 256);

    buffer.Write(adu->serverAddress());

    if (adu->isException()) {
      buffer.Write(FunctionCode(adu->functionCode() | Pdu::kExceptionByte));
    } else {
      buffer.Write(adu->functionCode());
    }

    buffer.Write(data);
  }
};

class ModbusAsciiFrameEncoder : public ModbusFrameEncoder {
public:
  virtual ~ModbusAsciiFrameEncoder() = default;

  void Encode(const Adu *adu, pp::bytes::Buffer &buffer) override {
    static_assert(true, "ascii mode not support yet");
  }
};

inline std::unique_ptr<ModbusFrameDecoder>
createModbusFrameDecoder(TransferMode mode, const CheckSizeFuncTable &table) {
  switch (mode) {
  case TransferMode::kRtu:
    return std::unique_ptr<ModbusFrameDecoder>(
        new ModbusRtuFrameDecoder(table));
  case TransferMode::kAscii:
    return std::unique_ptr<ModbusFrameDecoder>(
        new ModbusAsciiFrameDecoder(table));
  case TransferMode::kMbap:
    return std::unique_ptr<ModbusFrameDecoder>(
        new ModbusMbapFrameDecoder(table));
  default:
    smart_assert("unsupported modbus transfer mode")(static_cast<int>(mode));
    return nullptr;
  }
}

inline std::unique_ptr<ModbusFrameEncoder>
createModbusFrameEncoder(TransferMode mode) {
  switch (mode) {
  case TransferMode::kRtu:
    return std::unique_ptr<ModbusRtuFrameEncoder>(new ModbusRtuFrameEncoder());
  case TransferMode::kAscii:
    return std::unique_ptr<ModbusAsciiFrameEncoder>(
        new ModbusAsciiFrameEncoder());
  case TransferMode::kMbap:
    return std::unique_ptr<ModbusMbapFrameEncoder>(
        new ModbusMbapFrameEncoder());
  default:
    smart_assert("unsupported modbus transfer mode")(static_cast<int>(mode));
    return nullptr;
  }
}

} // namespace modbus

#endif // __MODBUS_SERIAL_PORT_H_
