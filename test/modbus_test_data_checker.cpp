#include <gtest/gtest.h>
#include <modbus/base/modbus.h>

using namespace modbus;

using DCR = DataChecker::Result;

struct Result {
  DCR retCode;
  size_t size;
};

struct Tester {
  DataChecker::calculateRequiredSizeFunc runner;
  ByteArray data;
  Result expect;
};

TEST(DC, bytesRequired) {
  ByteArray array({0x01, 0x02, 0x03, 0x04});
  ByteArray enougn({0x03, 0x02, 0x03, 0x04});
  ByteArray short_({0x03, 0x02});

  std::vector<Tester> testers = {
      {bytesRequired<4>, array, {DCR::kSizeOk, 4}},
      {bytesRequired<2>, array, {DCR::kSizeOk, 4}},
      {bytesRequired<8>, array, {DCR::kNeedMoreData, 4}},
      {bytesRequiredStoreInArrayIndex<0>, enougn, {DCR::kSizeOk, 4}},
      {bytesRequiredStoreInArrayIndex<0>, short_, {DCR::kNeedMoreData, 4}},
  };

  for (auto &tester : testers) {
    struct Result actual;
    actual.retCode = tester.runner(actual.size, tester.data);
    EXPECT_EQ(actual.retCode, tester.expect.retCode);
  }
}
