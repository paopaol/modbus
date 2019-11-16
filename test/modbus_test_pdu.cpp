#include "modbus_test_mocker.h"

TEST(TestPdu, modbusPduConstructor) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  modbus::Pdu pdu0(modbus::FunctionCode::kReadCoils, dataChecker);
  auto fcode0 = pdu0.functionCode();
  EXPECT_EQ(fcode0, modbus::FunctionCode::kReadCoils);
}

TEST(TestPdu, modbusPduFunctionCodeTest) {
  modbus::Pdu pdu1;
  pdu1.setFunctionCode(modbus::FunctionCode::kReadCoils);
  auto fcode = pdu1.functionCode();
  EXPECT_EQ(fcode, modbus::FunctionCode::kReadCoils);
}

TEST(TestPdu, modbusPduExpecptionTest) {
  modbus::Pdu pdu2;
  pdu2.setFunctionCode(modbus::FunctionCode(modbus::FunctionCode::kReadCoils +
                                            modbus::Pdu::kExceptionByte));
  EXPECT_EQ(true, pdu2.isException());
  auto fcode = pdu2.functionCode();
  EXPECT_EQ(fcode, modbus::FunctionCode::kReadCoils);
}

TEST(TestPdu, modbusPdu_defaultFunctionCode_isInvalid) {
  modbus::Pdu pdu4;
  EXPECT_EQ(modbus::FunctionCode::kInvalidCode, pdu4.functionCode());
}
