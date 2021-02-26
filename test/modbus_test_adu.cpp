#include "modbus/base/modbus.h"
#include "modbus/base/modbus_exception_datachecket.h"
#include "modbus/base/modbus_types.h"
#include "modbus_test_mocker.h"
#include <bits/stdint-uintn.h>

using namespace modbus;

static const CheckSizeFunc expectionResponseDataChecker = bytesRequired2<1>;
static QMap<FunctionCode, CheckSizeFunc> requestDataCheckerMap = {
    {kReadCoils, bytesRequired2<4>},
    {kReadInputDiscrete, bytesRequired2<4>},
    {kReadHoldingRegisters, bytesRequired2<4>},
    {kReadInputRegister, bytesRequired2<4>},
    {kWriteSingleCoil, bytesRequired2<4>},
    {kWriteSingleRegister, bytesRequired2<4>},
    {kWriteMultipleCoils, bytesRequiredStoreInArrayIndex2<4>},
    {kWriteMultipleRegisters, bytesRequiredStoreInArrayIndex2<4>},
    {kReadWriteMultipleRegisters, bytesRequiredStoreInArrayIndex2<9>}};

class ModbusRtuFrameDecoder {
  enum class State { kServerAddress, kFunctionCode, kData, kCrc0, kCrc1, kEnd };

public:
  DataChecker::Result Decode(pp::bytes::Buffer &buffer) {
    DataChecker::Result result = DataChecker::Result::kNeedMoreData;
    while (buffer.Len() > 0 || state_ == State::kEnd) {
      switch (state_) {
      case State::kServerAddress: {
        const auto serverAddress = buffer.ReadByte();
        adu_->setServerAddress(serverAddress);
        crcCtx_.crc16((const uint8_t *)&serverAddress, 1);

        state_ = State::kFunctionCode;
      } break;
      case State::kFunctionCode: {
        auto functionCode = buffer.ReadByte();
        adu_->setFunctionCode(static_cast<FunctionCode>(functionCode));
        crcCtx_.crc16((const uint8_t *)&functionCode, 1);

        state_ = State::kData;

        if (adu_->isException()) {
          function_ = bytesRequired2<1>;
        } else {
          auto it = requestDataCheckerMap.find(adu_->functionCode());
          if (it != requestDataCheckerMap.end()) {
            function_ = it.value();
          } else {
            error_ = Error::kIllegalFunctionCode;
            state_ = State::kEnd;
          }
        }

      } break;
      case State::kData: {
        size_t expectSize = 0;

        char *p;
        buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());

        result = function_(expectSize, (uint8_t *)p, buffer.Len());
        if (result == DataChecker::Result::kNeedMoreData) {
          goto exit_function;
        }
        // we need crc,
        result = DataChecker::Result::kNeedMoreData;

        buffer.ZeroCopyRead(p, expectSize);

        adu_->setData((uint8_t *)p, expectSize);
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
        } else if (adu_->isException()) {
          error_ = Error(adu_->data()[0]);
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

  void SetAduRef(Adu *adu) { adu_ = adu; }

  bool IsDone() const { return isDone_; }

  void Clear() {
    state_ = State::kServerAddress;
    adu_ = nullptr;
    isDone_ = false;
    crcCtx_.clear();
    error_ = Error::kNoError;
  }

  Error LasError() const { return error_; }

private:
  State state_ = State::kServerAddress;
  Adu *adu_ = nullptr;
  bool isDone_ = false;
  uint8_t crc_[2];
  CrcCtx crcCtx_;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
};

class ModbusAsciiFrameDecoder {
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
  DataChecker::Result Decode(pp::bytes::Buffer & /**/) {
    assert("ascii mode:not support yet");

    return DataChecker::Result::kSizeOk;
  }

  void SetAduRef(Adu *adu) { adu_ = adu; }

  bool IsDone() const { return isDone_; }

  void Clear() {
    state_ = State::kServerAddress;
    adu_ = nullptr;
    isDone_ = false;
    crcCtx_.clear();
    error_ = Error::kNoError;
  }

  Error LasError() const { return error_; }

private:
  State state_ = State::kServerAddress;
  Adu *adu_ = nullptr;
  bool isDone_ = false;
  CrcCtx crcCtx_;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
};

class ModbusMbapFrameDecoder {
  enum class State { kMBap, kServerAddress, kFunctionCode, kData, kEnd };

public:
  DataChecker::Result Decode(pp::bytes::Buffer &buffer) {
    DataChecker::Result result = DataChecker::Result::kNeedMoreData;
    while (buffer.Len() > 0 || state_ == State::kEnd) {
      switch (state_) {
      case State::kMBap: {
        if (buffer.Len() < 6) {
          goto exit_function;
        }
        char *p;
        buffer.ZeroCopyRead(p, 6);

        // transaction id
        id_ = p[0] * 256 + p[1];
        flag_ = p[2] * 256 + p[3];
        len_ = p[4] * 256 + p[5];

        state_ = State::kServerAddress;
      } break;
      case State::kServerAddress: {
        if (buffer.Len() < len_) {
          goto exit_function;
        }

        const auto serverAddress = buffer.ReadByte();
        adu_->setServerAddress(serverAddress);

        state_ = State::kFunctionCode;
      } break;
      case State::kFunctionCode: {
        auto functionCode = buffer.ReadByte();
        adu_->setFunctionCode(static_cast<FunctionCode>(functionCode));

        state_ = State::kData;

        if (adu_->isException()) {
          function_ = bytesRequired2<1>;
        } else {
          auto it = requestDataCheckerMap.find(adu_->functionCode());
          if (it != requestDataCheckerMap.end()) {
            function_ = it.value();
          } else {
            error_ = Error::kIllegalFunctionCode;
            state_ = State::kEnd;
          }
        }
      } break;
      case State::kData: {
        size_t expectSize = 0;

        char *p;
        buffer.ZeroCopyPeekAt(&p, 0, buffer.Len());

        result = function_(expectSize, (uint8_t *)p, buffer.Len());
        if (result == DataChecker::Result::kNeedMoreData) {
          goto exit_function;
        }

        buffer.ZeroCopyRead(p, expectSize);

        adu_->setData((uint8_t *)p, expectSize);

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

  void SetAduRef(Adu *adu) { adu_ = adu; }

  bool IsDone() const { return isDone_; }

  void Clear() {
    state_ = State::kMBap;
    adu_ = nullptr;
    isDone_ = false;
    error_ = Error::kNoError;
    id_ = 0;
    flag_ = 0;
    len_ = 0;
  }

  Error LasError() const { return error_; }

private:
  State state_ = State::kMBap;
  Adu *adu_ = nullptr;
  bool isDone_ = false;
  Error error_ = Error::kNoError;
  CheckSizeFunc function_;
  uint16_t id_ = 0;
  uint16_t flag_ = 0;
  uint16_t len_ = 0;
};

TEST(ModbusRtuFrameDecoder, decode_oneframe) {
  const ByteArray expect({0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17});
  pp::bytes::Buffer buffer;
  buffer.Write((const char *)expect.data(), expect.size());

  Adu adu;
  adu.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  ModbusRtuFrameDecoder decoder;

  decoder.SetAduRef(&adu);

  decoder.Decode(buffer);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x00);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x01, 0x00, 0x11));
}

TEST(ModbusRtuFrameDecoder, decode_multiframe) {
  const ByteArray expect({0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17, 0x00,
                          0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17});
  pp::bytes::Buffer buffer;
  buffer.Write((const char *)expect.data(), expect.size());

  Adu adu;
  ModbusRtuFrameDecoder decoder;

  decoder.Clear();
  decoder.SetAduRef(&adu);

  decoder.Decode(buffer);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(adu.serverAddress(), 0x00);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x01, 0x00, 0x11));

  decoder.Clear();
  decoder.SetAduRef(&adu);

  decoder.Decode(buffer);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(adu.serverAddress(), 0x00);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x01, 0x00, 0x11));
}

TEST(ModbusRtuFrameDecoder, decode_needmoredata) {
  const ByteArray expect({0x00, 0x01, 0x00, 0x01});
  pp::bytes::Buffer buffer;
  buffer.Write((const char *)expect.data(), expect.size());

  Adu adu;
  ModbusRtuFrameDecoder decoder;

  decoder.Clear();
  decoder.SetAduRef(&adu);

  EXPECT_EQ(DataChecker::Result::kNeedMoreData, decoder.Decode(buffer));
  EXPECT_EQ(false, decoder.IsDone());
}

TEST(ModbusRtuFrameDecoder, decode_bad_funtioncode) {
  const ByteArray expect({0x00, 0x33, 0x00, 0x01});
  pp::bytes::Buffer buffer;
  buffer.Write((const char *)expect.data(), expect.size());

  Adu adu;
  ModbusRtuFrameDecoder decoder;

  decoder.Clear();
  decoder.SetAduRef(&adu);

  decoder.Decode(buffer);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kIllegalFunctionCode, decoder.LasError());
}

TEST(ModbusRtuFrameDecoder, decode_oneframe_multitimes) {
  const ByteArray expect({0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17});
  pp::bytes::Buffer buffer;

  Adu adu;
  ModbusRtuFrameDecoder decoder;

  decoder.Clear();
  decoder.SetAduRef(&adu);

  buffer.Write("\x00\x01", 2);
  EXPECT_EQ(DataChecker::Result::kNeedMoreData, decoder.Decode(buffer));
  EXPECT_EQ(false, decoder.IsDone());

  buffer.Write("\x00\x01", 2);
  EXPECT_EQ(DataChecker::Result::kNeedMoreData, decoder.Decode(buffer));
  EXPECT_EQ(false, decoder.IsDone());

  buffer.Write("\x00\x11", 2);
  EXPECT_EQ(DataChecker::Result::kNeedMoreData, decoder.Decode(buffer));
  EXPECT_EQ(false, decoder.IsDone());

  buffer.Write("\xac\x17", 2);
  EXPECT_EQ(DataChecker::Result::kSizeOk, decoder.Decode(buffer));
  EXPECT_EQ(true, decoder.IsDone());

  EXPECT_EQ(adu.serverAddress(), 0x00);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x01, 0x00, 0x11));
}

TEST(ModbusMbapFrameDecoder, MbapFrame_unmrshal_oneframe_success) {
  std::vector<char> data{0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
                         0x01, 0x01, 0x00, 0x00, 0x00, 0x02};

  pp::bytes::Buffer buffer;
  buffer.Write(data);

  Adu adu;
  ModbusMbapFrameDecoder decoder;
  decoder.Clear();
  decoder.SetAduRef(&adu);

  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kSizeOk);
  EXPECT_EQ(decoder.IsDone(), true);
  EXPECT_EQ(adu.serverAddress(), 1);
  EXPECT_EQ(adu.functionCode(), 1);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x00, 0x00, 0x02));
}

TEST(ModbusMbapFrameDecoder, MbapFrame_unmrshal_multiframe_success) {
  std::vector<char> data{0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x01,
                         0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02};

  pp::bytes::Buffer buffer;
  buffer.Write(data);

  Adu adu;
  ModbusMbapFrameDecoder decoder;
  decoder.Clear();
  decoder.SetAduRef(&adu);

  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kSizeOk);
  EXPECT_EQ(decoder.IsDone(), true);
  EXPECT_EQ(adu.serverAddress(), 1);
  EXPECT_EQ(adu.functionCode(), 1);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x00, 0x00, 0x02));

  decoder.Clear();
  decoder.SetAduRef(&adu);

  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kSizeOk);
  EXPECT_EQ(decoder.IsDone(), true);
  EXPECT_EQ(adu.serverAddress(), 1);
  EXPECT_EQ(adu.functionCode(), 1);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x00, 0x00, 0x02));
}

TEST(ModbusMbapFrameDecoder, MbapFrame_unmrshal_oneframe_needmoredata) {
  std::vector<char> data{0x00, 0x00, 0x00, 0x00, 0x00,
                         0x06, 0x01, 0x01, 0x00, 0x00};

  pp::bytes::Buffer buffer;
  buffer.Write(data);

  Adu adu;
  ModbusMbapFrameDecoder decoder;
  decoder.Clear();
  decoder.SetAduRef(&adu);

  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kNeedMoreData);
  EXPECT_EQ(decoder.IsDone(), false);
}

TEST(ModbusMbapFrameDecoder, MbapFrame_unmrshal_oneframe_multitimes_success) {
  std::vector<char> data{0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
                         0x01, 0x01, 0x00, 0x00, 0x00, 0x02};

  pp::bytes::Buffer buffer;
  Adu adu;
  ModbusMbapFrameDecoder decoder;
  decoder.Clear();
  decoder.SetAduRef(&adu);

  buffer.Write("\x00\x00\x00\x00", 4);
  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kNeedMoreData);
  buffer.Write("\x00\x06", 2);
  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kNeedMoreData);
  buffer.Write("\x01\x01", 2);
  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kNeedMoreData);
  buffer.Write("\x00\x00\x00\x02", 4);
  EXPECT_EQ(decoder.Decode(buffer), DataChecker::Result::kSizeOk);

  EXPECT_EQ(decoder.IsDone(), true);
  EXPECT_EQ(adu.serverAddress(), 1);
  EXPECT_EQ(adu.functionCode(), 1);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x00, 0x00, 0x02));
}

TEST(TestModbusAdu, modbusAduConstructor) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  Adu adu(ServerAddress(1), FunctionCode::kReadCoils, dataChecker);
  EXPECT_EQ(FunctionCode::kReadCoils, adu.functionCode());
}

TEST(TestModbusAdu, modbusAduConstructor2) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  Adu adu(ServerAddress(1), Pdu(FunctionCode::kReadCoils, dataChecker));
  EXPECT_EQ(FunctionCode::kReadCoils, adu.functionCode());
}

TEST(TestAdu, modbusAduMarshalData) {
  struct Result {
    int size;
    modbus::ByteArray data;
  } expect{5, ByteArray({0x01, 0x01, 0x01, 0x02, 0x03})};

  Adu adu;
  struct Result actual;

  adu.setServerAddress(0x1);
  adu.setFunctionCode(FunctionCode::kReadCoils);
  adu.setData({1, 2, 3});
  actual.size = adu.marshalSize();
  actual.data = adu.marshalAduWithoutCrc();

  EXPECT_EQ(actual.data, expect.data);
  EXPECT_EQ(actual.size, expect.size);
}
