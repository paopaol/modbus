#include "base/modbus_frame.h"
#include "modbus/base/modbus.h"
#include "modbus/base/modbus_exception_datachecket.h"
#include "modbus/base/modbus_types.h"
#include "modbus_test_mocker.h"
#include <atomic>

using namespace modbus;

// TEST(ModbusRtuFrameDecoder, server_decode_readCoils_response_success) {
//  const ByteArray expect({0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17});
//  pp::bytes::Buffer buffer;
//  buffer.Write(expect);

//  Adu adu;
//  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

//  decoder.Decode(buffer, &adu);
//  EXPECT_EQ(true, decoder.IsDone());
//  EXPECT_EQ(Error::kNoError, decoder.LasError());
//  EXPECT_EQ(adu.serverAddress(), 0x00);
//  EXPECT_EQ(adu.functionCode(), 0x01);
//  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x01, 0x00, 0x11));
//}

//============================================================

TEST(TestModbusAdu, modbusAduConstructor) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  Adu adu(ServerAddress(1), FunctionCode::kReadCoils, dataChecker);
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
