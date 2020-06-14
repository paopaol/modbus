
#include "modbus_test_mocker.h"

using namespace modbus;

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
