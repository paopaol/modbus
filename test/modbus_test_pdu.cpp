#include "modbus_test_mocker.h"

using namespace modbus;

TEST(TestPdu, modbusPduConstructor) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  Pdu pdu(FunctionCode::kReadCoils, dataChecker);
  auto fcode = pdu.functionCode();
  EXPECT_EQ(fcode, FunctionCode::kReadCoils);
}

TEST(TestPdu, modbusPduConstructor_default_isInvalidFunctionCode) {
  Pdu pdu;
  auto fcode = pdu.functionCode();
  EXPECT_EQ(fcode, FunctionCode::kInvalidCode);
}

TEST(TestPdu, modbusPduFunctionCodeTest) {
  Pdu pdu;
  pdu.setFunctionCode(FunctionCode::kReadCoils);
  auto fcode = pdu.functionCode();
  EXPECT_EQ(fcode, FunctionCode::kReadCoils);
}

TEST(TestPdu, modbusPduExpecptionTest) {
  Pdu pdu;
  pdu.setFunctionCode(
      FunctionCode(FunctionCode::kReadCoils + Pdu::kExceptionByte));

  struct Result {
    bool isException;
    FunctionCode fcode;
  } expect{true, FunctionCode::kReadCoils};

  Result actual;
  actual.isException = pdu.isException();
  actual.fcode = pdu.functionCode();

  EXPECT_EQ(actual.fcode, expect.fcode);
  EXPECT_EQ(actual.isException, expect.isException);
}

TEST(TestPdu, modbusPdu_defaultFunctionCode_isInvalid) {
  Pdu pdu;
  EXPECT_EQ(FunctionCode::kInvalidCode, pdu.functionCode());
}
