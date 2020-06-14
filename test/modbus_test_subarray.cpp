#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <modbus/base/modbus_tool.h>

TEST(TestSubArray, subarrayToLast) {
  modbus::ByteArray array = {0x1, 0x2, 0x3};

  auto sub = modbus::tool::subArray(array, 2);
  EXPECT_THAT(sub, testing::ElementsAre(0x03));
}

TEST(TestSubArray, subarraySomeWhere) {
  modbus::ByteArray array = {0x1, 0x2, 0x3, 0x4, 0x5};

  auto sub = modbus::tool::subArray(array, 2, 2);
  EXPECT_THAT(sub, testing::ElementsAre(0x3, 0x4));
}
