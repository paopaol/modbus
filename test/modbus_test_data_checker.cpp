#include <gtest/gtest.h>
#include <modbus/base/modbus.h>

TEST(DataChecker, bytesRequired_success) {
  modbus::ByteArray array({0x01, 0x02, 0x03, 0x04});

  size_t size = 0;
  auto result = modbus::bytesRequired<4>(size, array);
  EXPECT_EQ(result, modbus::DataChecker::Result::kSizeOk);
  EXPECT_EQ(size, 4);

  size = 0;
  result = modbus::bytesRequired<2>(size, array);
  EXPECT_EQ(result, modbus::DataChecker::Result::kSizeOk);
  size = 0;
  result = modbus::bytesRequired<8>(size, array);
  EXPECT_EQ(result, modbus::DataChecker::Result::kNeedMoreData);
}
