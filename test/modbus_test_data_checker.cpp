#include <gtest/gtest.h>
#include <modbus/base/modbus.h>

using namespace modbus;

struct Result {
  CheckSizeResult retCode;
  size_t size;
};

struct Tester {
  CheckSizeFunc func;
  ByteArray data;
  Result expect;
};

TEST(DC, bytesRequired) {
  ByteArray array({0x01, 0x02, 0x03, 0x04});
  ByteArray enougn({0x03, 0x02, 0x03, 0x04});
  ByteArray short_({0x03, 0x02});

  std::vector<Tester> testers = {
      {bytesRequired<4>, array, {CheckSizeResult::kSizeOk, 4}},
      {bytesRequired<2>, array, {CheckSizeResult::kSizeOk, 4}},
      {bytesRequired<8>, array, {CheckSizeResult::kNeedMoreData, 4}},
      {bytesRequiredStoreInArrayIndex<0>,
       enougn,
       {CheckSizeResult::kSizeOk, 4}},
      {bytesRequiredStoreInArrayIndex<0>,
       short_,
       {CheckSizeResult::kNeedMoreData, 4}},
  };

  for (auto &tester : testers) {
    struct Result actual;
    actual.retCode =
        tester.func(actual.size, tester.data.data(), tester.data.size());
    EXPECT_EQ(actual.retCode, tester.expect.retCode);
  }
}
