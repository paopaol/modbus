#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "modbus.h"
#include "modbus_tool.h"
#include <map>

namespace modbus {
class SingleBitAccess {
public:
  SingleBitAccess() {}

  void buildFromArray(const ByteArray &array) {}
  void setStartAddress(Address startAddress) { startAddress_ = startAddress; }
  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  ByteArray marshalRequest() {
    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity_ / 256);
    array.push_back(quantity_ % 256);
    return array;
  }

  bool unmarshalResponse(const ByteArray &array) {
    if (array.empty()) {
      return false;
    }
    /**
     * bytes bumber
     */
    int bytes = array[0];
    if (bytes + 1 != array.size()) {
      return false;
    }

    ByteArray bitvalues(tool::subArray(array, 1));
    for (const auto &n : bitvalues) {
      for (int i = 0; i < quantity_; i++) {
        Address address = startAddress_ + i;
        bool status = n & (0x01 << i);
        valueMap_[address] = status ? BitValue::kOn : BitValue::kOff;
      }
    }
    return true;
  }

  BitValue value(Address address) {
    BitValue value = BitValue::kBadValue;

    auto it = valueMap_.find(address);
    if (it != valueMap_.end()) {
      value = it->second;
    }

    return value;
  }

private:
  Address startAddress_ = 0;
  Quantity quantity_ = 0;
  std::map<Address, BitValue> valueMap_;
};

} // namespace modbus

#endif /* FUNCTIONS_H */
