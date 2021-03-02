#ifndef __MODBUS_FRAME_H_
#define __MODBUS_FRAME_H_

#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>
#include <qmap.h>
#include <array>
#include <memory>
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

inline std::string dump(TransferMode transferMode, const ByteArray &byteArray) {
  return transferMode == TransferMode::kAscii ? tool::dumpRaw(byteArray)
                                              : tool::dumpHex(byteArray);
}

inline std::string dump(TransferMode transferMode, const char *p, int len) {
  return transferMode == TransferMode::kAscii
             ? tool::dumpRaw((uint8_t *)p, len)
             : tool::dumpHex((uint8_t *)p, len);
}

inline std::string dump(TransferMode transferMode, const QByteArray &array) {
  return transferMode == TransferMode::kAscii
             ? tool::dumpRaw((uint8_t *)array.data(), array.size())
             : tool::dumpHex((uint8_t *)array.data(), array.size());
}

inline std::string dump(TransferMode transferMode,
                        const pp::bytes::Buffer &buffer) {
  char *p;
  int len = buffer.Len();
  buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());
  return transferMode == TransferMode::kAscii
             ? tool::dumpRaw((uint8_t *)p, len)
             : tool::dumpHex((uint8_t *)p, len);
}

inline CheckSizeFuncTable creatDefaultCheckSizeFuncTableForClient() {
  static const CheckSizeFuncTable table = {
      nullptr,
      bytesRequiredStoreInArrayIndex<0>,  // kReadCoils 0x01
      bytesRequiredStoreInArrayIndex<0>,  // kReadInputDiscrete 0x02
      bytesRequiredStoreInArrayIndex<0>,  // kReadHoldingRegisters 0x03
      bytesRequiredStoreInArrayIndex<0>,  // kReadInputRegister 0x04
      bytesRequired<4>,                   // kWriteSingleCoil 0x05
      bytesRequired<4>,                   // kWriteSingleRegister 0x06
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      bytesRequired<4>,  //  kWriteMultipleCoils = 0x0f,
      bytesRequired<4>,  //  kWriteMultipleRegisters = 0x10,
      nullptr, nullptr, nullptr,
      nullptr,                            //  kReadFileRecords = 0x14,
      nullptr,                            //  kWriteFileRecords = 0x15,
      nullptr,                            //  kMaskWriteRegister = 0x16,
      bytesRequiredStoreInArrayIndex<0>,  //  kReadWriteMultipleRegisters =
                                          //  0x17,
      nullptr,  //  kReadDeviceIdentificationCode = 0x2b
  };
  return table;
}

inline CheckSizeFuncTable creatDefaultCheckSizeFuncTableForServer() {
  static const CheckSizeFuncTable table = {
      nullptr,
      bytesRequired<4>,  // kReadCoils 0x01
      bytesRequired<4>,  // kReadInputDiscrete 0x02
      bytesRequired<4>,  // kReadHoldingRegisters 0x03
      bytesRequired<4>,  // kReadInputRegister 0x04
      bytesRequired<4>,  // kWriteSingleCoil 0x05
      bytesRequired<4>,  // kWriteSingleRegister 0x06
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      bytesRequiredStoreInArrayIndex<4>,  //  kWriteMultipleCoils = 0x0f,
      bytesRequiredStoreInArrayIndex<4>,  //  kWriteMultipleRegisters = 0x10,
      nullptr, nullptr, nullptr,
      nullptr,                            //  kReadFileRecords = 0x14,
      nullptr,                            //  kWriteFileRecords = 0x15,
      nullptr,                            //  kMaskWriteRegister = 0x16,
      bytesRequiredStoreInArrayIndex<9>,  //  kReadWriteMultipleRegisters =
                                          //  0x17,
      nullptr,  //  kReadDeviceIdentificationCode = 0x2b
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

  CheckSizeResult Decode(pp::bytes::Buffer &buffer, Adu *adu) override {
    CheckSizeResult result = CheckSizeResult::kNeedMoreData;
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
                          ? bytesRequired<1>
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
          if (result == CheckSizeResult::kNeedMoreData) {
            goto exit_function;
          }
          // we need crc,
          result = CheckSizeResult::kNeedMoreData;

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
          result = CheckSizeResult::kSizeOk;
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

  CheckSizeResult Decode(pp::bytes::Buffer & /**/, Adu *) override {
    assert("ascii mode:not support yet");

    return CheckSizeResult::kSizeOk;
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

  CheckSizeResult Decode(pp::bytes::Buffer &buffer, Adu *adu) override {
    CheckSizeResult result = CheckSizeResult::kNeedMoreData;
    while (buffer.Len() > 0 || state_ == State::kEnd) {
      switch (state_) {
        case State::kMBap: {
          if (buffer.Len() < 6) {
            goto exit_function;
          }
          uint8_t *p;
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
                          ? bytesRequired<1>
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
          if (result == CheckSizeResult::kNeedMoreData) {
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
          result = CheckSizeResult::kSizeOk;
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
      buffer.Write(FunctionCode(adu->functionCode() | Adu::kExceptionByte));
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

    int size = data.size() + 2;
    buffer.Write(size / 256);
    buffer.Write(size % 256);

    buffer.Write(adu->serverAddress());

    if (adu->isException()) {
      buffer.Write(FunctionCode(adu->functionCode() | Adu::kExceptionByte));
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

inline std::unique_ptr<ModbusFrameDecoder> createModbusFrameDecoder(
    TransferMode mode, const CheckSizeFuncTable &table) {
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

inline std::unique_ptr<ModbusFrameEncoder> createModbusFrameEncoder(
    TransferMode mode) {
  switch (mode) {
    case TransferMode::kRtu:
      return std::unique_ptr<ModbusRtuFrameEncoder>(
          new ModbusRtuFrameEncoder());
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

}  // namespace modbus

#endif  // __MODBUS_SERIAL_PORT_H_
