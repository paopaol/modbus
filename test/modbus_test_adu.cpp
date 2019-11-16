
#include "modbus_test_mocker.h"

TEST(TestModbusAdu, modbusAduConstructor) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  modbus::Adu adu0(modbus::ServerAddress(1), modbus::FunctionCode::kReadCoils,
                   dataChecker);
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu0.functionCode());

  modbus::Adu adu1(modbus::ServerAddress(1),
                   modbus::Pdu(modbus::FunctionCode::kReadCoils, dataChecker));
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu1.functionCode());
}

TEST(TestAdu, modbusAduMarshalData) {
  modbus::Adu adu;

  adu.setServerAddress(0x1);
  adu.setFunctionCode(modbus::FunctionCode::kReadCoils);
  adu.setData({1, 2, 3});
  size_t buildSize = adu.marshalSize();
  EXPECT_EQ(buildSize, 1 + 1 + 3 /*serveraddress|function code|data*/);
  modbus::ByteArray buildData = adu.marshalData();
  EXPECT_EQ(buildData,
            modbus::ByteArray({1, modbus::FunctionCode::kReadCoils, 1, 2, 3}));
}
